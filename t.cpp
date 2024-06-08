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

using namespace std;

const int WINDOW_WIDTH = 120;
const int WINDOW_HEIGHT = 40;
const int FIRST_LINE = WINDOW_WIDTH / 3;
const int SECOND_LINE = FIRST_LINE * 2;
const int WINDOW_X = 0;
const int WINDOW_Y = 0;

int countB = 0;

mutex windowMutex;
mutex countBMutex;
condition_variable countBCond;
atomic<bool> stopThreads(false);
vector<thread> symbolThreads;

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
                    countBCond.wait(lock, []{ return countB < 2; });
                    countB++;
                    speed *= 0.7; // Zwiększenie prędkości o 30%
                } else { // B -> A
                    speed *= 1.4; // Zmniejszenie prędkości o 40%
                    countB--;
                    countBCond.notify_one();
                }
            } else if (x == SECOND_LINE) {
                if (dx == 1) { // B -> C
                    speed *= 1.1; // Zwiększenie prędkości o 10%
                    countB--;
                    countBCond.notify_one();
                } else { // C -> B
                    countBCond.wait(lock, []{ return countB < 2; });
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
            if(x >= FIRST_LINE && x <= SECOND_LINE){
                countB--;
                countBCond.notify_one();
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
                countBCond.wait(lock, []{ return countB < 2; });
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
