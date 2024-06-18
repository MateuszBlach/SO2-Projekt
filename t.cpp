#include <ncurses.h>
#include <unistd.h>
#include <thread>
#include <random>
#include <chrono>
#include <mutex>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <string>
#include <queue>
#include <climits> // for INT_MAX

using namespace std;

const int WINDOW_WIDTH = 120;
const int WINDOW_HEIGHT = 40;
const int FIRST_LINE = WINDOW_WIDTH / 3;
const int SECOND_LINE = FIRST_LINE * 2;
const int WINDOW_X = 0;
const int WINDOW_Y = 0;

mutex windowMutex;
atomic<bool> stopThreads(false);
vector<thread> symbolThreads;

int countB = 0;
mutex countBMutex;
condition_variable countBCond;

// Priorytetowa kolejka do przechowywania symboli czekających na wejście do strefy B
priority_queue<pair<int, thread::id>> waitingQueue;

void moveSymbol(WINDOW *win, char symbol, int startX, int startY) {
    int x = startX;
    int y = startY;
    int dx = 1;
    int dy = 1;
    int bounces = 0;
    int speed = 50000;

    while (!stopThreads) {
        {
            lock_guard<mutex> lock(windowMutex);
            mvwaddch(win, y, x, symbol);
            wrefresh(win);
        }

        {
            unique_lock<mutex> lock(countBMutex);

            if (x == FIRST_LINE) {
                if (dx == 1) { // A -> B
                    waitingQueue.push(make_pair(-speed, this_thread::get_id())); // Dodaj do kolejki priorytetowej
                    countBCond.wait(lock, [&, id=this_thread::get_id()]{ return countB < 2 && waitingQueue.top().second == id; });
                    waitingQueue.pop();
                    countB++;
                    speed *= 0.7; // Zwiększenie prędkości o 30%
                } else { // B -> A
                    speed *= 1.4; // Zmniejszenie prędkości o 40%
                    countB--;
                    countBCond.notify_all();
                }
            } else if (x == SECOND_LINE) {
                if (dx == 1) { // B -> C
                    speed *= 1.1; // Zwiększenie prędkości o 10%
                    countB--;
                    countBCond.notify_all();
                } else { // C -> B
                    waitingQueue.push(make_pair(-speed, this_thread::get_id())); // Dodaj do kolejki priorytetowej
                    countBCond.wait(lock, [&, id=this_thread::get_id()]{ return countB < 2 && waitingQueue.top().second == id; });
                    waitingQueue.pop();
                    countB++;
                    speed *= 0.8; // Zwiększenie prędkości o 20%
                }
            }
        }

        usleep(speed);

        {
            lock_guard<mutex> lock(windowMutex);
            mvwaddch(win, y, x, ' ');
        }

        x += dx;
        y += dy;

        if (x >= WINDOW_WIDTH - 2 || x <= 1) {
            dx = -dx;
            bounces++;
        }
        if (y >= WINDOW_HEIGHT - 2 || y <= 1) {
            dy = -dy;
            bounces++;
        }

        if (bounces >= 6) {
            if (x >= FIRST_LINE && x <= SECOND_LINE) {
                unique_lock<mutex> lock(countBMutex);
                countB--;
                countBCond.notify_all();
            }
            break;
        }
    }
}

void spawnSymbol(WINDOW *win) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> disX(1, WINDOW_WIDTH - 3);
    uniform_int_distribution<int> disY(1, WINDOW_HEIGHT - 3);
    uniform_int_distribution<int> disSymbol('a', 'z');

    while (!stopThreads) {
        char symbol = static_cast<char>(disSymbol(gen));
        int x = disX(gen);
        int y = disY(gen);

        {
            unique_lock<mutex> lock(countBMutex);
            if (x >= FIRST_LINE && x <= SECOND_LINE) {
                waitingQueue.push(make_pair(INT_MAX, this_thread::get_id())); // Wstaw symbol z bardzo niskim priorytetem
                countBCond.wait(lock, [&, id=this_thread::get_id()]{ return countB < 2 && waitingQueue.top().second == id; });
                waitingQueue.pop();
                countB++;
            }
        }

        thread symbolThread(moveSymbol, win, symbol, x, y);
        {
            lock_guard<mutex> lock(windowMutex);
            symbolThreads.push_back(move(symbolThread));
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void keyboardHandler() {
    while (true) {
        int ch = getch();
        if (ch == ' ') {
            stopThreads = true;
            countBCond.notify_all(); // Powiadomienie wszystkich oczekujących wątków
            break;
        }
    }
}

int main(int argc, char **argv) {
    initscr();
    noecho();
    curs_set(0);

    WINDOW *win = newwin(WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_Y, WINDOW_X);
    int left, right, top, bottom, tlc, trc, blc, brc;
    left = right = (int)'|';
    top = bottom = (int)'-';
    tlc = trc = blc = brc = (int)'+';
    wborder(win, left, right, top, bottom, tlc, trc, blc, brc);
    mvprintw(WINDOW_HEIGHT, WINDOW_X, "|-------------------A-------------------|-------------------B-------------------|-------------------C-------------------|");
    refresh();
    wrefresh(win);

    // Wątek odpowiedzialny za tworzenie nowych symboli
    thread spawnThread(spawnSymbol, win);

    // Wątek odpowiedzialny za obsługę klawiatury
    thread keyboardThread(keyboardHandler);

    // Zakończenie wszystkich wątków przed zakończeniem programu
    if (spawnThread.joinable()) {
        spawnThread.join();
    }

    if (keyboardThread.joinable()) {
        keyboardThread.join();
    }

    for (auto &thread : symbolThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    endwin();

    return 0;
}
