#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

// Provide weak default stubs for system calls that newlib expects. Projects can
// override them by defining their own strong symbols.

int __attribute__((weak)) _close(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return -1;
}

int __attribute__((weak)) _fstat(int fd, struct stat *st)
{
    (void)fd;
    if (st != NULL) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}

int __attribute__((weak)) _isatty(int fd)
{
    (void)fd;
    return 1;
}

off_t __attribute__((weak)) _lseek(int fd, off_t offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ENOSYS;
    return (off_t)-1;
}

ssize_t __attribute__((weak)) _read(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    errno = ENOSYS;
    return -1;
}

ssize_t __attribute__((weak)) _write(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

int __attribute__((weak)) _open(const char *pathname, int flags, ...)
{
    (void)pathname;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int __attribute__((weak)) _unlink(const char *pathname)
{
    (void)pathname;
    errno = ENOSYS;
    return -1;
}

extern char __heap_start__[];
extern char __heap_end__[];

void *__attribute__((weak)) _sbrk(ptrdiff_t increment)
{
    static char *current = __heap_start__;
    char *next = current + increment;

    if ((increment < 0) || (next < __heap_start__) || (next > __heap_end__)) {
        errno = ENOMEM;
        return (void *)-1;
    }

    char *prev = current;
    current = next;
    return prev;
}

void __attribute__((weak)) _exit(int status)
{
    (void)status;
    for (;;) {
        __asm__ volatile("wfi");
    }
}

int __attribute__((weak)) _getpid(void)
{
    return 1;
}

int __attribute__((weak)) _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = ENOSYS;
    return -1;
}
