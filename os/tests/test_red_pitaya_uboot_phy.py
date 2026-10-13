"""Exercise PHY configuration with the register state left by Linux reboot."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
DRIVER = SDK / 'boards/red-pitaya/stemlab-125-14/patches/u-boot/drivers/net/phy/intel_xway.c'


class RedPitayaUBootPhyTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('gcc'), 'native gcc required')
    def test_warm_reboot_delays_and_mdio_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'linux').mkdir()
            (root / 'linux/bitfield.h').write_text('''\
#define GENMASK(h, l) ((~0U >> (31 - (h))) & (~0U << (l)))
#define FIELD_PREP(mask, val) (((val) << __builtin_ctz(mask)) & (mask))
''')
            (root / 'phy.h').write_text('''\
#include <stdint.h>
typedef uint32_t u32;
typedef int ofnode;
struct phy_device { unsigned regs[32]; int error, negotiated; };
struct phy_driver {
    const char *name;
    unsigned uid, mask, features;
    int (*config)(struct phy_device *);
    int (*startup)(struct phy_device *);
    int (*shutdown)(struct phy_device *);
};
#define U_BOOT_PHY_DRIVER(name) struct phy_driver name
#define MDIO_DEVAD_NONE (-1)
#define MII_BMCR 0
#define BMCR_PDOWN 0x0800
#define PHY_GBIT_FEATURES 1
ofnode phy_get_ofnode(struct phy_device *phy);
int ofnode_valid(ofnode node);
u32 ofnode_read_u32_default(ofnode node, const char *name, u32 fallback);
int phy_modify(struct phy_device *phy, int devad, int reg, u32 mask, u32 value);
int genphy_config_aneg(struct phy_device *phy);
int genphy_startup(struct phy_device *phy);
int genphy_shutdown(struct phy_device *phy);
''')
            harness = root / 'test.c'
            harness.write_text(f'#include "{DRIVER}"\n' + '''\
#include <assert.h>
#include <string.h>
ofnode phy_get_ofnode(struct phy_device *phy) { return 1; }
int ofnode_valid(ofnode node) { return node != 0; }
u32 ofnode_read_u32_default(ofnode node, const char *name, u32 fallback) {
    return !strcmp(name, "rx-internal-delay-ps") ? 2000 : 2500;
}
int phy_modify(struct phy_device *phy, int devad, int reg, u32 mask, u32 value) {
    if (phy->error) return phy->error;
    unsigned previous = phy->regs[reg];
    phy->regs[reg] = (previous & ~mask) | value;
    return previous != phy->regs[reg];
}
int genphy_config_aneg(struct phy_device *phy) {
    assert(!(phy->regs[MII_BMCR] & BMCR_PDOWN));
    phy->negotiated++;
    return 0;
}
int genphy_startup(struct phy_device *phy) { return 0; }
int genphy_shutdown(struct phy_device *phy) { return 0; }
int main(void) {
    /* 0x1a40 was captured on hardware after Linux powered the PHY down. */
    struct phy_device warm = { .regs = { 0x1a40 } };
    warm.regs[0x17] = 0x0800; /* Preserve the MII voltage setting. */
    assert(xway.config(&warm) == 0);
    assert(warm.regs[0] == 0x1240);
    assert(warm.regs[0x17] == 0x4d00); /* SDK kernel: RX 2 ns, TX 2.5 ns. */
    assert(warm.negotiated == 1);
    assert(xway.config(&warm) == 0); /* Reconfiguration also succeeds. */
    assert(warm.regs[0] == 0x1240 && warm.regs[0x17] == 0x4d00);
    assert(warm.negotiated == 2);
    struct phy_device failure = { .error = -5 };
    assert(xway.config(&failure) == -5);
    assert(failure.negotiated == 0);
    return 0;
}
''')
            executable = root / 'test'
            result = subprocess.run(
                ['gcc', '-std=c11', '-Wall', '-Werror', '-I', str(root),
                 str(harness), '-o', str(executable)],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(executable)], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
