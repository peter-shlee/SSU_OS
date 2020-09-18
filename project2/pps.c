#include <ncurses.h>

int main(void) {
	initscr();
	printw("pps");
	refresh();
	getch();
	endwin();

	return 0;
}
