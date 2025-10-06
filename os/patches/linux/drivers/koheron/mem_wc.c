// SPDX-License-Identifier: GPL-2.0
// Koheron generic memory window character device (write-combine mmap)
// Exposes a linear MMIO region to userspace with WC mapping via /dev/mem_wc0x<BASE>.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DRV_NAME "mem_wc"

struct mem_wc {
	phys_addr_t        phys_start;
	resource_size_t    size;
	struct miscdevice  misc;
};

static int mem_wc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct mem_wc *mw = container_of(filp->private_data, struct mem_wc, misc);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long pgoff = vma->vm_pgoff << PAGE_SHIFT;

	if (pgoff + vsize > mw->size)
		return -EINVAL;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	return remap_pfn_range(vma,
			       vma->vm_start,
			       (mw->phys_start + pgoff) >> PAGE_SHIFT,
			       vsize,
			       vma->vm_page_prot);
}

static const struct file_operations mem_wc_fops = {
	.owner  = THIS_MODULE,
	.mmap   = mem_wc_mmap,
	.llseek = default_llseek,
};

static int mem_wc_probe(struct platform_device *pdev)
{
	struct mem_wc *mw;
	struct resource *res;
	int ret;

	mw = devm_kzalloc(&pdev->dev, sizeof(*mw), GFP_KERNEL);
	if (!mw)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	mw->phys_start = res->start;
	mw->size       = resource_size(res);

	/* Name device as mem_wc0x<BASEADDR> (full phys addr width) */
	mw->misc.minor  = MISC_DYNAMIC_MINOR;
	mw->misc.name   = devm_kasprintf(&pdev->dev, GFP_KERNEL,
	                                 "mem_wc0x%llx",
	                                 (unsigned long long)mw->phys_start);
	if (!mw->misc.name)
		return -ENOMEM;

	mw->misc.fops   = &mem_wc_fops;
	mw->misc.parent = &pdev->dev;

	ret = misc_register(&mw->misc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, mw);
	return 0;
}

static void mem_wc_remove(struct platform_device *pdev)
{
	struct mem_wc *mw = platform_get_drvdata(pdev);

	if (mw)
		misc_deregister(&mw->misc);
}

static const struct of_device_id mem_wc_of_match[] = {
	{ .compatible = "koheron,mem-wc-1.0" },
	{ .compatible = "xlnx,axi-bram-ctrl-4.1" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mem_wc_of_match);

static struct platform_driver mem_wc_driver = {
	.probe  = mem_wc_probe,
	.remove = mem_wc_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = mem_wc_of_match,
	},
};
module_platform_driver(mem_wc_driver);

MODULE_AUTHOR("Koheron");
MODULE_DESCRIPTION("Generic memory window chardev with write-combine mmap");
MODULE_LICENSE("GPL");
