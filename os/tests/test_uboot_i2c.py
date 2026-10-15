"""Exercise the vendor transfer loop against a bus requiring a final STOP."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
PATCH = SDK / 'os/scripts/patch_uboot.sh'
FIXTURE = SDK / 'os/tests/fixtures/uboot-cdns-i2c-xfer.c'

HARNESS = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
typedef uint8_t u8;
struct regs { unsigned control; };
struct i2c_cdns_bus { int hold_flag; unsigned quirks; struct regs *regs; };
struct udevice { struct i2c_cdns_bus *priv; };
struct i2c_msg { unsigned addr, flags, len; u8 *buf; };
#define CDNS_I2C_BROKEN_HOLD_BIT 1
#define CDNS_I2C_CONTROL_HOLD 16
#define CDNS_I2C_ARB_LOST_MAX_RETRIES 10
#define I2C_M_RD 1
#define debug(...) ((void)0)
#define setbits_le32(addr, bits) (*(addr) |= (bits))
static void *dev_get_priv(struct udevice *dev) { return dev->priv; }
static int calls, expected_calls, retry_at, bad_hold;
static bool expected_hold[8];
static int transfer(struct i2c_cdns_bus *bus, unsigned addr, u8 *data, unsigned len)
{
    int call = calls++;
    if (call >= expected_calls || bus->hold_flag != expected_hold[call]) {
        bad_hold = 1;
        return -ETIMEDOUT;
    }
    return call == retry_at ? -EAGAIN : 0;
}
#define cdns_i2c_read_data transfer
#define cdns_i2c_write_data transfer
#include "drivers/i2c/i2c-cdns.c"

static int run(struct udevice *dev, struct i2c_msg *messages, int count,
               const bool *hold, int transfers, int arbitration)
{
    calls = bad_hold = 0;
    expected_calls = transfers;
    retry_at = arbitration;
    for (int i = 0; i < transfers; ++i) expected_hold[i] = hold[i];
    int result = cdns_i2c_xfer(dev, messages, count);
    if (result || bad_hold || calls != transfers) {
        fprintf(stderr, "transfer failed: result=%d, hold=%d, calls=%d/%d\n",
                result, bad_hold, calls, transfers);
        return 1;
    }
    return 0;
}
int main(void)
{
    struct regs regs = {0};
    struct i2c_cdns_bus bus = {.quirks=CDNS_I2C_BROKEN_HOLD_BIT, .regs=&regs};
    struct udevice dev = {&bus};
    u8 buffer[1024] = {0};
    struct i2c_msg messages[] = {{0x50, 0, 2, buffer},
                                {0x50, I2C_M_RD, 1024, buffer},
                                {0x50, I2C_M_RD, 1, buffer}};
    const unsigned lengths[] = {1, 16, 17, 252, 1024};
    const bool combined[] = {true, false};
    const bool single[] = {false};
    for (unsigned i=0; i<sizeof(lengths)/sizeof(lengths[0]); ++i) {
        messages[1].len = lengths[i];
        if (run(&dev, messages, 2, combined, 2, -1)) return 1;
        if (run(&dev, &messages[1], 1, single, 1, -1)) return 1;
    }
    /* A combined write must also release the bus after its final message. */
    messages[1].flags = 0;
    if (run(&dev, messages, 2, combined, 2, -1)) return 1;
    const bool triple[] = {true, true, false};
    if (run(&dev, messages, 3, triple, 3, -1)) return 1;
    /* Arbitration can restart the sequence at either the first or last message. */
    messages[1].flags = I2C_M_RD;
    const bool retry_first[] = {true, true, false};
    const bool retry_last[] = {true, false, true, false};
    if (run(&dev, messages, 2, retry_first, 3, 0)) return 1;
    if (run(&dev, messages, 2, retry_last, 4, 1)) return 1;
    calls = 0;
    if (cdns_i2c_xfer(&dev, &messages[1], 2) != -EOPNOTSUPP || calls) return 1;
    return 0;
}
'''


class UBootI2CTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('gcc'), 'native gcc required')
    def test_final_stop_and_arbitration_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            driver = root / 'drivers/i2c/i2c-cdns.c'
            driver.parent.mkdir(parents=True)
            shutil.copyfile(FIXTURE, driver)
            harness = root / 'test.c'
            harness.write_text(HARNESS)
            executable = root / 'test'

            def run():
                subprocess.run(['gcc', '-std=c11', '-Wall', '-Werror', str(harness),
                                '-o', str(executable)], check=True, capture_output=True,
                               text=True, timeout=10)
                return subprocess.run([str(executable)], capture_output=True,
                                      text=True, timeout=10)

            original = run()
            self.assertNotEqual(original.returncode, 0, 'vendor regression must reproduce')
            self.assertIn('hold=1', original.stderr)
            subprocess.run(['bash', str(PATCH), str(root)], check=True, timeout=10)
            patched = driver.read_bytes()
            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run(['bash', str(PATCH), str(root)], check=True, timeout=10)
            self.assertEqual(driver.read_bytes(), patched)

    def test_unknown_transfer_loop_fails_explicitly(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            driver = root / 'drivers/i2c/i2c-cdns.c'
            driver.parent.mkdir(parents=True)
            driver.write_text('/* Unexpected vendor revision. */\n')
            result = subprocess.run(['bash', str(PATCH), str(root)], capture_output=True,
                                    text=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('FAILED', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
