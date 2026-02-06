#include "../tty_platform.h"
#include <unistd.h>
#include <termios.h>

int tty_platform_winsize(int fd, int* rows, int* cols) {
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, &ws, sizeof(ws)) != 0) {
		return -1;
	}
	*rows = ws.ws_row;
	*cols = ws.ws_col;
	return 0;
}
