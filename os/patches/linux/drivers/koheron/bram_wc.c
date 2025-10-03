// drivers/koheron/bram_wc.c
// SPDX-License-Identifier: MIT
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>

struct bram_wc {
	struct resource   res;
	struct miscdevice misc;
};

static int bram_wc_open(struct inode *ino, struct file *f) { return 0; }
static int bram_wc_release(struct inode *ino, struct file *f) { return 0; }

static int bram_wc_mmap(struct file *f, struct vm_area_struct *vma)
{
	struct bram_wc *priv = container_of(f->private_data, struct bram_wc, misc);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	phys_addr_t phys = priv->res.start + ((phys_addr_t)vma->vm_pgoff << PAGE_SHIFT);

	if (phys < priv->res.start || (phys + vsize - 1) > priv->res.end)
		return -EINVAL;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	/* modern kernels expose vm_flags_set() */
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	if (remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT, vsize, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations bram_wc_fops = {
	.owner   = THIS_MODULE,
	.open    = bram_wc_open,
	.release = bram_wc_release,
	.mmap    = bram_wc_mmap,
	.llseek  = noop_llseek,   /* use noop_llseek on new kernels */
};

static int bram_wc_probe(struct platform_device *pdev)
{
	struct bram_wc *priv;
	const char *devname = NULL;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) return -ENOMEM;

	if (of_address_to_resource(pdev->dev.of_node, 0, &priv->res))
		return -EINVAL;

	of_property_read_string(pdev->dev.of_node, "linux,devname", &devname);
	if (!devname)
		devname = devm_kasprintf(&pdev->dev, GFP_KERNEL, "bram_wc%pa", &priv->res.start);
	if (!devname)
		return -ENOMEM;

	priv->misc.minor  = MISC_DYNAMIC_MINOR;
	priv->misc.name   = devname;
	priv->misc.fops   = &bram_wc_fops;
	priv->misc.parent = &pdev->dev;

	ret = misc_register(&priv->misc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);
	dev_info(&pdev->dev, "WC mmap: /dev/%s phys=%pa size=0x%lx\n",
		 priv->misc.name, &priv->res.start,
		 (unsigned long)resource_size(&priv->res));
	return 0;
}

static void bram_wc_remove(struct platform_device *pdev)
{
	struct bram_wc *priv = platform_get_drvdata(pdev);
	misc_deregister(&priv->misc);
}

static const struct of_device_id bram_wc_of_match[] = {
	{ .compatible = "xlnx,axi-bram-ctrl-4.1" },
	{ .compatible = "koheron,bram-wc" },
	{ }
};
MODULE_DEVICE_TABLE(of, bram_wc_of_match);

static struct platform_driver bram_wc_driver = {
	.probe  = bram_wc_probe,
	.remove = bram_wc_remove,
	.driver = {
		.name           = "bram_wc",
		.of_match_table = bram_wc_of_match,
	},
};

module_platform_driver(bram_wc_driver);
MODULE_LICENSE("MIT");
