// koheron_init.c

#include <env.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/kconfig.h>   // IS_ENABLED()
#include <dm.h>
#include <i2c.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <stdlib.h>

_Static_assert(IS_ENABLED(CONFIG_ARCH_ZYNQ), "koheron_init: CONFIG_ARCH_ZYNQ is required");
_Static_assert(IS_ENABLED(CONFIG_MISC_INIT_R), "koheron_init: CONFIG_MISC_INIT_R must be enabled");
_Static_assert(IS_ENABLED(CONFIG_DM_I2C), "koheron_init: CONFIG_DM_I2C must be enabled");
_Static_assert(IS_ENABLED(CONFIG_OF_SYSTEM_SETUP), "koheron_init: CONFIG_DM_I2C must be enabled");

#define OCM_ENV_ADDR   ((const char *)0xFFFFFC00)
#define OCM_ENV_MAX    128

#define EE_BUS   0
#define EE_ADDR  0x54
#define EE_OFF   0x0000
#define EE_ALEN  2

static bool is_valid_mac_str(const char *s)
{
	if (!s) return false;
	for (int i = 0; i < 17; i++) {
		if ((i % 3) == 2) {
			if (s[i] != ':') return false;
		} else {
			if (!isxdigit((int)s[i])) return false;
		}
	}
	return s[17] == '\0';
}

static void set_ethaddr_from_ocm(void)
{
	const char *p = OCM_ENV_ADDR;

	/* Expect exactly: "ethaddr=aa:bb:cc:dd:ee:ff\n" */
	if (strncmp(p, "ethaddr=", 8) != 0)
		return;

	char line[OCM_ENV_MAX];
	int n = 0;
	while (n + 1 < (int)sizeof(line) && p[n] && p[n] != '\n') {
		line[n] = p[n];
		n++;
	}
	line[n] = '\0';

	const char *mac = line + 8;

	/* Only set if not already defined (avoids "Can't overwrite ethaddr") */
	const char *cur = env_get("ethaddr");
	if ((!cur || !*cur) && is_valid_mac_str(mac)) {
		env_set("ethaddr", mac);
	}
}

static void set_serial_from_eeprom(void)
{
	if (env_get("koheron_serial"))
		return;

	struct udevice *chip;
	u8 b[4];

	/* Get 24C64 on I2C0 @ 0x54 (16-bit word address) */
	if (!i2c_get_chip_for_busnum(EE_BUS, EE_ADDR, EE_ALEN, &chip)) {
		if (!dm_i2c_read(chip, EE_OFF, b, sizeof(b))) {
			/* little-endian uint32: c1 36 00 00 -> 0x000036C1 (14017) */
			u32 serial = (u32)b[0] | ((u32)b[1] << 8) |
			             ((u32)b[2] << 16) | ((u32)b[3] << 24);
			if (serial != 0 && serial != 0xFFFFFFFFU) {
				env_set_ulong("koheron_serial", serial);
				printf("koheron_serial=%lu\n", (unsigned long)serial);
			}
		}
	}
}

/* ---------- U-Boot hooks ---------- */

int misc_init_r(void)
{
	set_ethaddr_from_ocm();
	set_serial_from_eeprom();
	return 0;
}

int ft_system_setup(void *blob, struct bd_info *bd)
{
	// Serial number will show at /proc/device-tree/serial-number
    const char *s = env_get("koheron_serial");
    if (!s || !*s)
        return 0;

    int root = fdt_path_offset(blob, "/");
    if (root >= 0)
        fdt_setprop_string(blob, root, "serial-number", s);

    int chosen = fdt_path_offset(blob, "/chosen");
    if (chosen >= 0)
        fdt_setprop_u32(blob, chosen, "koheron,serial",
                        (u32)strtoul(s, NULL, 10));
    return 0;
}
