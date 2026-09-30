/* Run on an idle system with /dev/cma and a CMA pool; no FPGA DMA required. */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMA_ALLOC _IOWR('Z', 0, uint32_t)
static size_t page_size;

static int open_cma(void)
{
    int fd = open("/dev/cma", O_RDWR | O_CLOEXEC);
    assert(fd >= 0);
    return fd;
}

static uint32_t allocate(int fd, size_t bytes)
{
    uint32_t value = bytes;
    assert(ioctl(fd, CMA_ALLOC, &value) == 0);
    return value;
}

static void expect_ioctl(int fd, void *arg, int error)
{
    errno = 0;
    assert(ioctl(fd, CMA_ALLOC, arg) == -1);
    assert(errno == error);
}

static unsigned char *map_buffer(int fd, size_t bytes, off_t offset)
{
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    assert(p != MAP_FAILED);
    return p;
}

static void expect_map(int fd, size_t bytes, off_t offset, int flags, int error)
{
    errno = 0;
    assert(mmap(NULL, bytes, PROT_READ | PROT_WRITE, flags, fd, offset) == MAP_FAILED);
    assert(errno == error);
}

static void test_isolation_and_lifetime(void)
{
    int a = open_cma(), b = open_cma(), alias = dup(a);
    uint32_t size = page_size;
    uint32_t pa = allocate(a, 3 * page_size);
    uint32_t pb = allocate(b, page_size);
    unsigned char *x = map_buffer(a, 3 * page_size, 0);
    unsigned char *y = map_buffer(b, page_size, 0);
    assert(pa != pb);
    for (size_t i = 0; i < 3 * page_size; ++i) assert(x[i] == 0);
    x[0] = 42;
    x[2 * page_size] = 73;
    y[0] = 91;
    expect_ioctl(alias, &size, EBUSY);
    assert(close(b) == 0);
    assert(y[0] == 91 && x[0] == 42);
    assert(munmap(y, page_size) == 0);

    /* Splitting a VMA must keep both surviving pieces pinned. */
    assert(munmap(x + page_size, page_size) == 0);
    expect_ioctl(a, &size, EBUSY);
    assert(munmap(x, page_size) == 0);
    expect_ioctl(a, &size, EBUSY);
    assert(close(a) == 0);
    assert(x[2 * page_size] == 73);
    assert(munmap(x + 2 * page_size, page_size) == 0);
    allocate(alias, page_size);
    assert(close(alias) == 0);
    puts("PASS independent opens, dup, VMA split, close before unmap");
}

static void test_errors_and_offsets(void)
{
    int fd = open_cma();
    uint32_t size = 0;
    expect_map(fd, page_size, 0, MAP_SHARED, ENXIO);
    expect_ioctl(fd, &size, EINVAL);
    expect_ioctl(fd, (void *)1, EFAULT);
    errno = 0;
    assert(ioctl(fd, _IO('Z', 1), &size) == -1 && errno == ENOTTY);
    allocate(fd, 2 * page_size - 1);
    unsigned char *p = map_buffer(fd, 2 * page_size, 0);
    p[page_size] = 55;
    assert(munmap(p, 2 * page_size) == 0);

    /* copy_to_user failure must not destroy the previous allocation. */
    uint32_t *ro = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ro != MAP_FAILED);
    *ro = page_size;
    assert(mprotect(ro, page_size, PROT_READ) == 0);
    expect_ioctl(fd, ro, EFAULT);
    assert(munmap(ro, page_size) == 0);
    FILE *info = fopen("/proc/meminfo", "r");
    char line[256];
    unsigned long pool_kib = 0;
    assert(info);
    while (fgets(line, sizeof(line), info)) {
        if (sscanf(line, "CmaTotal: %lu kB", &pool_kib) == 1) break;
    }
    assert(fclose(info) == 0);
    assert(pool_kib > 0 && pool_kib * 1024ULL + page_size <= UINT32_MAX);
    size = pool_kib * 1024ULL + page_size;
    expect_ioctl(fd, &size, ENOMEM);
    size = 0;
    expect_ioctl(fd, &size, EINVAL);
    p = map_buffer(fd, page_size, page_size);
    assert(p[0] == 55);
    assert(munmap(p, page_size) == 0);
    expect_map(fd, 3 * page_size, 0, MAP_SHARED, ENXIO);
    expect_map(fd, page_size, 2 * page_size, MAP_SHARED, ENXIO);
    expect_map(fd, page_size, 0, MAP_PRIVATE, EINVAL);
    allocate(fd, page_size);
    p = map_buffer(fd, page_size, 0);
    for (size_t i = 0; i < page_size; ++i) assert(p[i] == 0);
    assert(munmap(p, page_size) == 0);
    assert(close(fd) == 0);
    puts("PASS invalid arguments, failed replacement, offsets, zeroing");
}

static void test_fork(void)
{
    int fd = open_cma(), ready[2], done[2];
    uint32_t size = page_size;
    char token;
    allocate(fd, page_size);
    unsigned char *p = map_buffer(fd, page_size, 0);
    p[0] = 19;
    assert(pipe(ready) == 0 && pipe(done) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        assert(close(fd) == 0);
        assert(write(ready[1], "x", 1) == 1);
        assert(read(done[0], &token, 1) == 1);
        assert(p[0] == 19);
        assert(munmap(p, page_size) == 0);
        _exit(0);
    }
    assert(read(ready[0], &token, 1) == 1);
    assert(munmap(p, page_size) == 0);
    expect_ioctl(fd, &size, EBUSY);
    assert(write(done[1], "x", 1) == 1);
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    allocate(fd, page_size);
    assert(close(fd) == 0);
    close(ready[0]); close(ready[1]); close(done[0]); close(done[1]);
    puts("PASS fork holds allocation after parent unmaps");
}

static void *stress(void *arg)
{
    int fd = *(int *)arg;
    for (int i = 0; i < 200; ++i) {
        uint32_t size = 2 * page_size;
        int ret = ioctl(fd, CMA_ALLOC, &size);
        assert(ret == 0 || (ret == -1 && errno == EBUSY));
        unsigned char *p = map_buffer(fd, 2 * page_size, 0);
        /* Fault both pages without concurrent writes to shared contents. */
        volatile unsigned char value = p[0] ^ p[page_size];
        (void)value;
        assert(munmap(p, 2 * page_size) == 0);
    }
    return NULL;
}

static void test_stress(void)
{
    int fd = open_cma();
    pthread_t threads[4];
    allocate(fd, 2 * page_size);
    for (int i = 0; i < 4; ++i) assert(pthread_create(&threads[i], NULL, stress, &fd) == 0);
    for (int i = 0; i < 4; ++i) assert(pthread_join(threads[i], NULL) == 0);
    assert(close(fd) == 0);
    /* Cumulative allocation exceeds the test VM's 32 MiB CMA pool. */
    for (int i = 0; i < 128; ++i) {
        fd = open_cma();
        allocate(fd, 512 * 1024);
        unsigned char *p = map_buffer(fd, 512 * 1024, 0);
        assert(close(fd) == 0);
        p[0] = 1;
        assert(munmap(p, 512 * 1024) == 0);
    }
    for (int i = 0; i < 128; ++i) {
        pid_t child = fork();
        assert(child >= 0);
        if (!child) {
            fd = open_cma();
            allocate(fd, 512 * 1024);
            unsigned char *p = map_buffer(fd, 512 * 1024, 0);
            p[0] = 1;
            /* Exit without explicitly closing or unmapping. */
            _exit(0);
        }
        int status;
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    puts("PASS concurrent allocation/mapping, reclamation, process exit");
}

int main(int argc, char **argv)
{
    page_size = sysconf(_SC_PAGESIZE);
    if (argc == 2 && strcmp(argv[1], "--high-address") == 0) {
        int fd = open_cma();
        for (int i = 0; i < 100; ++i) {
            uint32_t size = page_size;
            expect_ioctl(fd, &size, EOVERFLOW);
        }
        assert(close(fd) == 0);
        puts("HIGH ADDRESS EOVERFLOW TEST PASSED");
        return 0;
    }
    assert(argc == 1);
    test_isolation_and_lifetime();
    test_errors_and_offsets();
    test_fork();
    test_stress();
    puts("ALL CMA TESTS PASSED");
    return 0;
}
