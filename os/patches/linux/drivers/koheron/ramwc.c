// SPDX-License-Identifier: GPL-2.0
// Koheron RAM window character device (write-combine mmap)
// Creates /dev/ramwc0x<BASE> for a reserved-memory carveout referenced by memory-region.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DRV_NAME "ramwc"

#ifndef BRAM_IOC_MAGIC
#define BRAM_IOC_MAGIC 'B'
struct bram_inv { __u64 uaddr; __u64 len; };
#define BRAM_INV_RANGE _IOW(BRAM_IOC_MAGIC, 1, struct bram_inv)
#endif

struct ramwc {
	void __iomem    *base;        /* optional kernel mapping (not required) */
	phys_addr_t      phys_start;  /* physical start of reserved region */
	resource_size_t  size;        /* size of reserved region */
	struct miscdevice misc;
};

static int ramwc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ramwc *wc = container_of(filp->private_data, struct ramwc, misc);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long pgoff = vma->vm_pgoff << PAGE_SHIFT;

	if (pgoff + vsize > wc->size)
		return -EINVAL;

	/* write-combine for fast streaming writes */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

	return remap_pfn_range(vma,
			       vma->vm_start,
			       (wc->phys_start + pgoff) >> PAGE_SHIFT,
			       vsize,
			       vma->vm_page_prot);
}

static long ramwc_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case BRAM_INV_RANGE: {
		struct bram_inv inv;
		if (copy_from_user(&inv, (void __user *)arg, sizeof(inv)))
			return -EFAULT;
		/* WC userspace mapping: explicit invalidate is a no-op here. */
		return 0;
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ramwc_fops = {
	.owner          = THIS_MODULE,
	.mmap           = ramwc_mmap,
	.unlocked_ioctl = ramwc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ramwc_ioctl,
#endif
	.llseek         = default_llseek,
};

static int ramwc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *mem_np;
	struct resource res;
	struct ramwc *wc;
	int ret;

	/* Find the reserved-memory region referenced by memory-region */
	mem_np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!mem_np) {
		dev_err(dev, "ramwc: missing memory-region phandle\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_np, 0, &res);
	of_node_put(mem_np);
	if (ret) {
		dev_err(dev, "ramwc: failed to parse reserved-memory reg: %d\n", ret);
		return ret;
	}

	wc = devm_kzalloc(dev, sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	wc->phys_start = res.start;
	wc->size       = resource_size(&res);

	/* Optional kernel mapping (not required for user mmap). */
	wc->base = devm_ioremap_wc(dev, wc->phys_start, wc->size);
	if (IS_ERR(wc->base)) {
		/* If WC ioremap not available on this arch, fall back to noncached. */
		wc->base = devm_ioremap(dev, wc->phys_start, wc->size);
		if (IS_ERR(wc->base))
			return PTR_ERR(wc->base);
	}

	/* Name device as ramwc0x<BASE-low32> to match bram_wc naming style */
	{
		char namebuf[32];
		u32 base32 = (u32)wc->phys_start;

		snprintf(namebuf, sizeof(namebuf), "ramwc0x%08x", base32);

		wc->misc.minor  = MISC_DYNAMIC_MINOR;
		wc->misc.name   = kstrdup(namebuf, GFP_KERNEL);
		if (!wc->misc.name)
			return -ENOMEM;
		wc->misc.fops   = &ramwc_fops;
		wc->misc.parent = dev;

		ret = misc_register(&wc->misc);
		if (ret) {
			kfree(wc->misc.name);
			return ret;
		}
	}

	platform_set_drvdata(pdev, wc);

	dev_info(dev,
		 "ramwc: reserved phys=0x%pa size=%pa → /dev/%s\n",
		 &wc->phys_start, &wc->size, wc->misc.name);

	return 0;
}

static void ramwc_remove(struct platform_device *pdev)
{
	struct ramwc *wc = platform_get_drvdata(pdev);

	if (wc) {
		misc_deregister(&wc->misc);
		kfree(wc->misc.name);
	}
}

static const struct of_device_id ramwc_of_match[] = {
	{ .compatible = "koheron,ramwc" },   /* your overlay node */
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ramwc_of_match);

static struct platform_driver ramwc_driver = {
	.probe  = ramwc_probe,
	.remove = ramwc_remove,   /* void signature (matches many vendor kernels) */
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = ramwc_of_match,
	},
};
module_platform_driver(ramwc_driver);

MODULE_AUTHOR("Koheron");
MODULE_DESCRIPTION("Koheron RAM window chardev (WC mmap for reserved-memory)");
MODULE_LICENSE("GPL");
