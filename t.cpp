#include <ncurses.h>
#include <unistd.h>
#include <thread>
#include <random>
#include <chrono>
#include <mutex>
#include <vector>

const int WINDOW_WIDTH = 120;
const int WINDOW_HEIGHT = 40;
const int WINDOW_X = 0;
const int WINDOW_Y = 0;

std::mutex windowMutex;

void moveSymbol(WINDOW* win, char symbol, int startX, int startY) {
    int x = startX;
    int y = startY;
    int dx = 1;
    int dy = 1;
    int bounces = 0;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(windowMutex); // Lock the window
            mvwaddch(win, y, x, symbol);
            wrefresh(win);
        } // Release the lock

        usleep(50000); // sleep for 0.05 second

        {
            std::lock_guard<std::mutex> lock(windowMutex); // Lock the window
            mvwaddch(win, y, x, ' ');
        } // Release the lock

        // Move the symbol
        x += dx;
        y += dy;

        // Check for collision with window boundaries
        if (x >= WINDOW_WIDTH - 2 || x <= 1) {
            dx = -dx; // Change direction on collision
            bounces++;
        }
        if (y >= WINDOW_HEIGHT - 2 || y <= 1) {
            dy = -dy; // Change direction on collision
            bounces++;
        }

        // Check if the symbol has bounced 6 times
        if (bounces >= 6) {
            break; // Exit the loop if 6 bounces reached
        }
    }
}


void spawnSymbol(WINDOW* win) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> disX(1, WINDOW_WIDTH - 3);
    std::uniform_int_distribution<int> disY(1, WINDOW_HEIGHT - 3);
    std::uniform_int_distribution<int> disSymbol('a', 'z');

    while (true) {
        char symbol = static_cast<char>(disSymbol(gen));
        int x = disX(gen);
        int y = disY(gen);
        
        std::thread symbolThread(moveSymbol, win, symbol, x, y);
        symbolThread.detach(); // Detach the thread to allow it to run independently
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char ** argv){
    initscr();
    noecho();

    WINDOW* win = newwin(
        WINDOW_HEIGHT,
        WINDOW_WIDTH,
        WINDOW_Y,
        WINDOW_X
    );

    int left,right,top,bottom, tlc, trc, blc, brc;
    left = right = (int)'|';
    top = bottom = (int)'-';
    tlc = trc = blc = brc = (int)'+';
    wborder(win,left,right,top,bottom,tlc,trc,blc,brc);
    refresh();
    wrefresh(win);

    // Collection of symbol threads
    std::vector<std::thread> symbolThreads;

    // Spawning symbols in a separate thread
    std::thread spawnThread(spawnSymbol, win);

    getch();

    endwin();

    return 0;
}
