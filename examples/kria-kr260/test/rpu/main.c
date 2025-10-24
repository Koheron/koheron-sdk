#include <stdint.h>

// Minimal Cortex-R5 firmware stub. It simply increments a counter and waits
// for interrupts so that remoteproc can attach to the image.
static volatile uint32_t heartbeat = 0;

int main(void)
{
    for (;;) {
        heartbeat++;
        __asm__ volatile("wfi");
    }

    return 0;
}
