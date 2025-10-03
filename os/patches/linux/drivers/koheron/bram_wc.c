// SPDX-License-Identifier: GPL-2.0
// Koheron BRAM window character device (write-combine mmap)
// Creates /dev/bram_wc0x<BASE> only when xlnx,s-axi-id-width > 0 (AXI4).

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

#define DRV_NAME "bram_wc"

#ifndef BRAM_IOC_MAGIC
#define BRAM_IOC_MAGIC 'B'
struct bram_inv { __u64 uaddr; __u64 len; };
#define BRAM_INV_RANGE _IOW(BRAM_IOC_MAGIC, 1, struct bram_inv)
#endif

struct bram_wc {
	void __iomem    *base;
	phys_addr_t      phys_start;
	resource_size_t  size;
	struct miscdevice misc;
};

static int bram_wc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct bram_wc *wc = container_of(filp->private_data, struct bram_wc, misc);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long pgoff = vma->vm_pgoff << PAGE_SHIFT;

	if (pgoff + vsize > wc->size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

	return remap_pfn_range(vma,
			       vma->vm_start,
			       (wc->phys_start + pgoff) >> PAGE_SHIFT,
			       vsize,
			       vma->vm_page_prot);
}

static long bram_wc_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case BRAM_INV_RANGE: {
		struct bram_inv inv;
		if (copy_from_user(&inv, (void __user *)arg, sizeof(inv)))
			return -EFAULT;
		return 0; /* WC mapping: explicit invalidate is a no-op */
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations bram_wc_fops = {
	.owner          = THIS_MODULE,
	.mmap           = bram_wc_mmap,
	.unlocked_ioctl = bram_wc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = bram_wc_ioctl,
#endif
	.llseek         = default_llseek,
};

static int bram_wc_probe(struct platform_device *pdev)
{
	struct bram_wc *wc;
	struct resource *res;
	u32 idw = 0;
	int ret;

	/* AXI selection: require xlnx,s-axi-id-width > 0 (AXI4). */
	if (of_property_read_u32(pdev->dev.of_node, "xlnx,s-axi-id-width", &idw) || idw == 0) {
		dev_info(&pdev->dev, "bram_wc: AXI4-Lite (id-width=%u) — skipping\n", idw);
		return -ENODEV;
	}

	wc = devm_kzalloc(&pdev->dev, sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	wc->phys_start = res->start;
	wc->size       = resource_size(res);

	/* Optional kernel mapping (not required for user mmap). */
	wc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(wc->base))
		return PTR_ERR(wc->base);

	/* Name device as bram_wc0x<BASEADDR-low32> */
	{
		char namebuf[32];
		u32 base32 = (u32)wc->phys_start;

		snprintf(namebuf, sizeof(namebuf), "bram_wc0x%08x", base32);

		wc->misc.minor  = MISC_DYNAMIC_MINOR;
		wc->misc.name   = kstrdup(namebuf, GFP_KERNEL);
		if (!wc->misc.name)
			return -ENOMEM;
		wc->misc.fops   = &bram_wc_fops;
		wc->misc.parent = &pdev->dev;

		ret = misc_register(&wc->misc);
		if (ret) {
			kfree(wc->misc.name);
			return ret;
		}
	}

	platform_set_drvdata(pdev, wc);

	dev_info(&pdev->dev,
		 "bram_wc: mapped base=0x%pa size=%pa (AXI4 id-width=%u) → /dev/%s\n",
		 &wc->phys_start, &wc->size, idw, wc->misc.name);

	return 0;
}

static void bram_wc_remove(struct platform_device *pdev)
{
	struct bram_wc *wc = platform_get_drvdata(pdev);

	if (wc) {
		misc_deregister(&wc->misc);
		kfree(wc->misc.name);
	}
}

static const struct of_device_id bram_wc_of_match[] = {
	{ .compatible = "koheron,bram-wc-1.0" },
	{ .compatible = "xlnx,axi-bram-ctrl-4.1" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, bram_wc_of_match);

static struct platform_driver bram_wc_driver = {
	.probe  = bram_wc_probe,
	.remove = bram_wc_remove,   /* void signature in modern kernels */
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = bram_wc_of_match,
	},
};
module_platform_driver(bram_wc_driver);

MODULE_AUTHOR("Koheron");
MODULE_DESCRIPTION("Koheron BRAM window chardev (WC mmap, AXI4 only via xlnx,s-axi-id-width)");
MODULE_LICENSE("GPL");
