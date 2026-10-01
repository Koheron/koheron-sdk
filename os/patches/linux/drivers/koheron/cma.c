// Originally based on pavel-demin/red-pitaya-notes/patches/cma.c

#include <linux/compat.h>
#include <linux/dma-map-ops.h>
#include <linux/highmem.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* Keep the existing ABI: byte count in, 32-bit physical address out. */
#define CMA_ALLOC _IOWR('Z', 0, u32)

struct cma_file {
  struct mutex lock;
  struct page *page;
  unsigned long count;
  atomic_t mappings;
};

static int cma_open(struct inode *inode, struct file *file)
{
  struct cma_file *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

  if (!ctx)
    return -ENOMEM;
  mutex_init(&ctx->lock);
  atomic_set(&ctx->mappings, 0);
  file->private_data = ctx;
  return 0;
}

static long cma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  struct cma_file *ctx = file->private_data;
  struct page *page;
  unsigned long count, i;
  u64 bytes, address;
  u32 buffer;
  int ret;

  if (cmd != CMA_ALLOC)
    return -ENOTTY;
  if (copy_from_user(&buffer, (void __user *)arg, sizeof(buffer)))
    return -EFAULT;
  if (!buffer)
    return -EINVAL;

  /* Round in 64 bits, including on 32-bit ARM. */
  bytes = ALIGN((u64)buffer, (u64)PAGE_SIZE);
  if (bytes > ULONG_MAX)
    return -EOVERFLOW;
  count = bytes >> PAGE_SHIFT;

  mutex_lock(&ctx->lock);
  if (atomic_read(&ctx->mappings)) {
    ret = -EBUSY;
    goto unlock;
  }

  page = dma_alloc_from_contiguous(NULL, count, 0, false);
  if (!page) {
    ret = -ENOMEM;
    goto unlock;
  }

  address = page_to_phys(page);
  if (address + bytes - 1 > U32_MAX) {
    ret = -EOVERFLOW;
    goto free_new;
  }

  /* Do not expose data left behind by the previous page owner. */
  for (i = 0; i < count; ++i)
    clear_highpage(pfn_to_page(page_to_pfn(page) + i));

  buffer = address;
  if (copy_to_user((void __user *)arg, &buffer, sizeof(buffer))) {
    ret = -EFAULT;
    goto free_new;
  }

  /* A failed replacement leaves the previous allocation intact. */
  if (ctx->page)
    dma_release_from_contiguous(NULL, ctx->page, ctx->count);
  ctx->page = page;
  ctx->count = count;
  ret = 0;
  goto unlock;

free_new:
  dma_release_from_contiguous(NULL, page, count);
unlock:
  mutex_unlock(&ctx->lock);
  return ret;
}

#ifdef CONFIG_COMPAT
static long cma_compat_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg)
{
  return cma_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static void cma_vma_open(struct vm_area_struct *vma)
{
  struct cma_file *ctx = vma->vm_private_data;

  atomic_inc(&ctx->mappings);
}

static void cma_vma_close(struct vm_area_struct *vma)
{
  struct cma_file *ctx = vma->vm_private_data;

  atomic_dec(&ctx->mappings);
}

static vm_fault_t cma_fault(struct vm_fault *vmf)
{
  struct cma_file *ctx = vmf->vma->vm_private_data;

  if (vmf->pgoff >= ctx->count)
    return VM_FAULT_SIGBUS;
  return vmf_insert_page(vmf->vma, vmf->address,
                        pfn_to_page(page_to_pfn(ctx->page) + vmf->pgoff));
}

static const struct vm_operations_struct cma_vm_ops = {
  .open = cma_vma_open,
  .close = cma_vma_close,
  .fault = cma_fault,
};

static int cma_mmap(struct file *file, struct vm_area_struct *vma)
{
  struct cma_file *ctx = file->private_data;
  unsigned long count = vma_pages(vma);
  int ret = 0;

  mutex_lock(&ctx->lock);
  if (!ctx->page) {
    ret = -ENXIO;
  } else if (!(vma->vm_flags & VM_SHARED)) {
    ret = -EINVAL;
  } else if (vma->vm_pgoff >= ctx->count ||
             count > ctx->count - vma->vm_pgoff) {
    ret = -ENXIO;
  } else {
    vm_flags_set(vma, VM_MIXEDMAP | VM_DONTEXPAND | VM_DONTDUMP);
    vma->vm_private_data = ctx;
    vma->vm_ops = &cma_vm_ops;
    cma_vma_open(vma);
  }
  mutex_unlock(&ctx->lock);
  return ret;
}

static int cma_release(struct inode *inode, struct file *file)
{
  struct cma_file *ctx = file->private_data;

  /* Each VMA holds vm_file: release runs after the last fd AND mapping. */
  if (ctx->page)
    dma_release_from_contiguous(NULL, ctx->page, ctx->count);
  kfree(ctx);
  return 0;
}

static const struct file_operations cma_fops = {
  .owner = THIS_MODULE,
  .open = cma_open,
  .unlocked_ioctl = cma_ioctl,
#ifdef CONFIG_COMPAT
  .compat_ioctl = cma_compat_ioctl,
#endif
  .mmap = cma_mmap,
  .release = cma_release,
};

static struct miscdevice cma_device = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "cma",
  .fops = &cma_fops,
};

static int __init cma_init(void)
{
  return misc_register(&cma_device);
}

static void __exit cma_exit(void)
{
  misc_deregister(&cma_device);
}

module_init(cma_init);
module_exit(cma_exit);
MODULE_LICENSE("MIT");
