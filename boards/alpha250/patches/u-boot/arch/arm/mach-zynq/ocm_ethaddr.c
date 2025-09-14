// arch/arm/mach-zynq/ocm_ethaddr.c
#include <env.h>
#include <linux/ctype.h>

#define OCM_ENV_ADDR   ((const char *)0xFFFFFC00)   /* FSBL wrote "ethaddr=..:\n" */
#define OCM_ENV_MAX    128

static bool is_valid_mac_str(const char *s)
{
	if (!s) return false;
	for (int i = 0; i < 17; i++) {
		if ((i % 3) == 2) { if (s[i] != ':') return false; }
		else { if (!isxdigit((int)s[i])) return false; }
	}
	return s[17] == '\0';
}

/* Runs early in the “_r” phase, before networking */
int misc_init_r(void)
{
	const char *p = OCM_ENV_ADDR;

	/* Expect exactly: ethaddr=aa:bb:cc:dd:ee:ff\n */
	if (strncmp(p, "ethaddr=", 8) != 0)
		return 0;

	char line[OCM_ENV_MAX];
	int n;

	for (n = 0; n + 1 < (int)sizeof(line) && p[n] && p[n] != '\n'; ++n)
		line[n] = p[n];
	line[n] = '\0';

	const char *mac = line + 8;

	/* Only set if not already present (avoids “Can't overwrite ethaddr”) */
	const char *cur = env_get("ethaddr");
	if ((!cur || !*cur) && is_valid_mac_str(mac)) {
		env_set("ethaddr", mac);
	}

	return 0;
}