#include <env.h>
#include <i2c.h>
#include <linux/types.h>
#include <string.h>

#define EEPROM_BUS     0
#define EEPROM_ADDR    0x50
#define ENV_OFFSET     0x1804
#define ENV_SIZE       0x400

static const char *find_value(const uint8_t *buf, size_t len, const char *key)
{
	const char *p = (const char *)buf;
	const char *end = (const char *)buf + len;
	const size_t klen = strlen(key);

	while (p < end && *p) {
		size_t l = strnlen(p, end - p);
		if (!l || p + l >= end)
			break;
		if (l > klen + 1 && !strncmp(p, key, klen) && p[klen] == '=')
			return p + klen + 1;
		p += l + 1;
	}
	return NULL;
}

static int eeprom_read_block(uint8_t *dst, size_t len)
{
	struct udevice *chip;
	int ret = i2c_get_chip_for_busnum(EEPROM_BUS, EEPROM_ADDR, 2, &chip);
	if (ret)
		return ret;
	return dm_i2c_read(chip, ENV_OFFSET, dst, len);
}

int misc_init_r(void)
{
	uint8_t need = 0;
	const char *cur;

	cur = env_get("ethaddr");
	if (!cur || !*cur)
		need |= 0x1;
	cur = env_get("hw_rev");
	if (!cur || !*cur)
		need |= 0x2;
	cur = env_get("serial");
	if (!cur || !*cur)
		need |= 0x4;

	if (!need)
		return 0;

	uint8_t buf[ENV_SIZE];
	if (eeprom_read_block(buf, sizeof(buf)))
		return 0;

	if (need & 0x1) {
		const char *v = find_value(buf, sizeof(buf), "ethaddr");
		if (v && *v)
			env_set("ethaddr", v);
	}
	if (need & 0x2) {
		const char *v = find_value(buf, sizeof(buf), "hw_rev");
		if (v && *v)
			env_set("hw_rev", v);
	}
	if (need & 0x4) {
		const char *v = find_value(buf, sizeof(buf), "serial");
		if (v && *v)
			env_set("serial", v);
	}

	return 0;
}
