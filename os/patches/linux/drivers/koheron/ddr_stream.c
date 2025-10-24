// SPDX-License-Identifier: GPL-2.0
/*
 * ddr_stream.c — Splice-able stream from reserved DDR to sockets
 *
 * Exposes a reserved-memory region (WITHOUT "no-map") as a read-only
 * char device that supports .splice_read, allowing zero-copy
 * DDR -> TCP (sendpage) transfers.
 *
 * Device tree requirements:
 *   reserved-memory {
 *     ddr_stream_region: ddr-stream@18000000 {
 *       reg = <0x18000000 0x08000000>;  // don't add "no-map"
 *       reusable;
 *     };
 *   };
 *   ddr_stream@0 {
 *     compatible = "koheron,ddr-stream";
 *     memory-region = <&ddr_stream_region>;
 *   };
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/minmax.h>
#include <linux/splice.h>
#include <linux/pipe_fs_i.h>   /* struct splice_pipe_desc / partial_page */
#include <linux/pagemap.h>     /* page helpers */
#include <linux/slab.h>

#define DRV_NAME "ddr_stream"
#define DEV_NAME "ddr_stream"

struct ddr_stream {
	struct device *dev;

	phys_addr_t    phys_base;   /* e.g. 0x1800_0000 */
	size_t         size;        /* e.g. 0x0800_0000 (128 MiB) */
	size_t         rp;          /* read pointer within [0..size] */

	/* chardev */
	struct cdev    cdev;
	dev_t          devt;
	struct class  *cls;
};

static struct ddr_stream *gds;

/* --- file ops ---------------------------------------------------------- */

static loff_t dds_llseek(struct file *f, loff_t off, int whence)
{
	struct ddr_stream *ds = gds;
	loff_t newrp;

	switch (whence) {
	case SEEK_SET: newrp = off; break;
	case SEEK_CUR: newrp = (loff_t)ds->rp + off; break;
	case SEEK_END: newrp = (loff_t)ds->size + off; break;
	default: return -EINVAL;
	}
	if (newrp < 0 || newrp > ds->size)
		return -EINVAL;

	ds->rp = (size_t)newrp;
	return newrp;
}

/*
 * Minimal, robust version: offer at most one (partial) page per call.
 * The caller (splice core) will loop to drain more.
 */
static ssize_t dds_splice_read(struct file *file, loff_t *ppos,
			       struct pipe_inode_info *pipe, size_t len,
			       unsigned int flags)
{
	struct ddr_stream *ds = gds;
	size_t avail, want, chunk;
	phys_addr_t phys;
	unsigned long pfn;
	size_t page_off;
	struct page *page;
	struct partial_page part;
	struct splice_pipe_desc spd = {
		.pages        = &page,
		.nr_pages     = 1,
		.nr_pages_max = 1,
		.partial      = &part,
		.ops          = &page_cache_pipe_buf_ops,
	};
	ssize_t n;

	if (!ds)
		return -ENODEV;

	/* bytes still available in our finite window */
	avail = (ds->size > ds->rp) ? (ds->size - ds->rp) : 0;
	if (!avail)
		return 0;

	want = min_t(size_t, len, avail);

	/* Convert current rp into (page, offset) */
	phys     = ds->phys_base + ds->rp;
	pfn      = phys >> PAGE_SHIFT;
	page_off = phys & (PAGE_SIZE - 1);
	chunk    = min_t(size_t, PAGE_SIZE - page_off, want);

	page = pfn_to_page(pfn);
	if (unlikely(!page))
		return -EFAULT;

	get_page(page);                 /* pipe will own a ref for enqueued page */
	part.offset = page_off;
	part.len    = chunk;

	n = splice_to_pipe(pipe, &spd);
	if (n < 0) {
		put_page(page);         /* not queued -> drop ref */
		return n;
	}
	/* n is the number of bytes actually queued (<= chunk). Advance rp by n. */
	ds->rp += n;
	*ppos   = ds->rp;

	/* If the pipe was full and took 0 (very unlikely), don't busy-spin. */
	if (n == 0)
		return -EAGAIN;

	return n;
}

static const struct file_operations dds_fops = {
	.owner       = THIS_MODULE,
	.splice_read = dds_splice_read,
	.llseek      = dds_llseek,
	/* .read_iter could be added for debug copies, but is not required */
};

/* --- probe/remove ------------------------------------------------------ */

static int dds_probe(struct platform_device *pdev)
{
	struct ddr_stream *ds;
	int rc;

	ds = devm_kzalloc(&pdev->dev, sizeof(*ds), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;
	ds->dev = &pdev->dev;

	/* Bind to reserved-memory phandle; DO NOT use "no-map" in DT */
	{
		struct of_phandle_args args;
		struct reserved_mem *rmem;

		rc = of_parse_phandle_with_args(pdev->dev.of_node,
						"memory-region", NULL, 0, &args);
		if (rc) {
			dev_err(&pdev->dev, "missing memory-region\n");
			return rc;
		}
		rmem = of_reserved_mem_lookup(args.np);
		of_node_put(args.np);
		if (!rmem) {
			dev_err(&pdev->dev, "reserved memory not found\n");
			return -ENODEV;
		}

		ds->phys_base = rmem->base;
		ds->size      = rmem->size;
		ds->rp        = 0;
	}

	/* Basic sanity: pfn_to_page must work on first/last PFN */
	{
		unsigned long first_pfn = ds->phys_base >> PAGE_SHIFT;
		unsigned long last_pfn  = (ds->phys_base + ds->size - 1) >> PAGE_SHIFT;
		struct page *a = pfn_to_page(first_pfn);
		struct page *b = pfn_to_page(last_pfn);
		if (!a || !b) {
			dev_err(ds->dev, "pfn_to_page failed (no-map?)\n");
			return -EINVAL;
		}
	}

	/* Char device setup */
	rc = alloc_chrdev_region(&ds->devt, 0, 1, DEV_NAME);
	if (rc)
		return rc;

	cdev_init(&ds->cdev, &dds_fops);
	ds->cdev.owner = THIS_MODULE;
	rc = cdev_add(&ds->cdev, ds->devt, 1);
	if (rc)
		goto out_unreg;

	ds->cls = class_create(DEV_NAME);           /* 6.12 signature */
	if (IS_ERR(ds->cls)) {
		rc = PTR_ERR(ds->cls);
		goto out_cdev;
	}

	if (!device_create(ds->cls, NULL, ds->devt, NULL, DEV_NAME)) {
		rc = -ENODEV;
		goto out_class;
	}

	gds = ds;
	dev_info(ds->dev, "ready: phys=%pa size=%zu\n", &ds->phys_base, ds->size);
	return 0;

out_class:
	class_destroy(ds->cls);
out_cdev:
	cdev_del(&ds->cdev);
out_unreg:
	unregister_chrdev_region(ds->devt, 1);
	return rc;
}

static void dds_remove(struct platform_device *pdev) /* 6.12: void remove */
{
	struct ddr_stream *ds = gds;

	if (!ds)
		return;

	device_destroy(ds->cls, ds->devt);
	class_destroy(ds->cls);
	cdev_del(&ds->cdev);
	unregister_chrdev_region(ds->devt, 1);

	gds = NULL;
}

/* --- OF / module boilerplate ------------------------------------------ */

static const struct of_device_id dds_of_match[] = {
	{ .compatible = "koheron,ddr-stream" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dds_of_match);

static struct platform_driver dds_drv = {
	.probe  = dds_probe,
	.remove = dds_remove,      /* void in 6.12 */
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = dds_of_match,
	},
};
module_platform_driver(dds_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Koheron");
MODULE_DESCRIPTION("Splice-able stream from reserved DDR to sockets");
