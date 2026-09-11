#ifndef QD_QDTTY_TTY_PLATFORM_H
#define QD_QDTTY_TTY_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Get terminal window size via ioctl.
// Returns 0 on success, -1 on failure.
int tty_platform_winsize(int fd, int* rows, int* cols);

#ifdef __cplusplus
}
#endif

#endif // QD_QDTTY_TTY_PLATFORM_H
