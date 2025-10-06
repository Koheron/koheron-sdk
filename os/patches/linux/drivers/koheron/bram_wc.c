// SPDX-License-Identifier: GPL-2.0
// Koheron BRAM window character device (write-combine mmap)
// Creates /dev/bram_wc0x<BASE> for Xilinx AXI BRAM controller.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DRV_NAME "bram_wc"

struct bram_wc {
	phys_addr_t       phys_start;
	resource_size_t   size;
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

static const struct file_operations bram_wc_fops = {
	.owner  = THIS_MODULE,
	.mmap   = bram_wc_mmap,
	.llseek = default_llseek,
};

static int bram_wc_probe(struct platform_device *pdev)
{
	struct bram_wc *wc;
	struct resource *res;
	int ret;

	wc = devm_kzalloc(&pdev->dev, sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	wc->phys_start = res->start;
	wc->size       = resource_size(res);

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
	.remove = bram_wc_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = bram_wc_of_match,
	},
};
module_platform_driver(bram_wc_driver);

MODULE_AUTHOR("Koheron");
MODULE_DESCRIPTION("Koheron BRAM window chardev (WC mmap)");
MODULE_LICENSE("GPL");
