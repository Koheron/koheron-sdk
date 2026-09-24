// https://github.com/pavel-demin/red-pitaya-notes/blob/master/patches/cma.c

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/dma-map-ops.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define CMA_ALLOC _IOWR('Z', 0, u32)
#define CMA_ALLOC_BOARD _IOWR('Z', 1, u32)

static unsigned long cma_size = 0;
static struct page *cma_page = NULL;
static struct page **cma_pages = NULL;
static struct device *allocation_device;
static struct device *board_device;
static struct file *allocation_owner;
static DEFINE_MUTEX(cma_lock);

static void cma_free(void)
{
  if(cma_pages)
  {
    kvfree(cma_pages);
    cma_pages = NULL;
  }

  if(cma_page)
  {
    dma_release_from_contiguous(allocation_device, cma_page, cma_size);
    cma_page = NULL;
  }
  allocation_device = NULL;
  allocation_owner = NULL;
  cma_size = 0;
}

static long cma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int i;
  long rc = 0;
  u32 buffer;

  if(cmd != CMA_ALLOC && cmd != CMA_ALLOC_BOARD) return -ENOTTY;
  if(cmd == CMA_ALLOC_BOARD && !board_device) return -ENODEV;

  if(copy_from_user(&buffer, (void __user *)arg, sizeof(buffer))) return -EFAULT;
  if(!buffer || (cmd == CMA_ALLOC_BOARD && buffer > 0x08000000)) return -EINVAL;

  mutex_lock(&cma_lock);
  if(cma_page && allocation_owner != file)
  {
    rc = -EBUSY;
    goto unlock;
  }

  cma_free();

  cma_size = PAGE_ALIGN(buffer) >> PAGE_SHIFT;

  cma_pages = kvmalloc_array(cma_size, sizeof(struct page *), GFP_KERNEL);

  if(!cma_pages)
  {
    rc = -ENOMEM;
    goto unlock;
  }

  allocation_device = cmd == CMA_ALLOC_BOARD ? board_device : NULL;
  cma_page = dma_alloc_from_contiguous(allocation_device, cma_size, 0, false);

  if(!cma_page)
  {
    cma_free();
    rc = -ENOMEM;
    goto unlock;
  }

  allocation_owner = file;

  for(i = 0; i < cma_size; ++i) cma_pages[i] = &cma_page[i];

  buffer = page_to_phys(cma_page);
  if(copy_to_user((void __user *)arg, &buffer, sizeof(buffer)))
  {
    cma_free();
    rc = -EFAULT;
  }
unlock:
  mutex_unlock(&cma_lock);
  return rc;
}

static int cma_mmap(struct file *file, struct vm_area_struct *vma)
{
  int rc;
  mutex_lock(&cma_lock);
  if(!cma_pages || allocation_owner != file) rc = -ENXIO;
  else
  {
    vm_flags_set(vma, VM_MIXEDMAP);
    rc = vm_map_pages(vma, cma_pages, cma_size);
  }
  mutex_unlock(&cma_lock);
  return rc;
}

static int cma_release(struct inode *inode, struct file *file)
{
  mutex_lock(&cma_lock);
  if(allocation_owner == file) cma_free();
  mutex_unlock(&cma_lock);
  return 0;
}

static struct file_operations cma_fops =
{
  .unlocked_ioctl = cma_ioctl,
  .mmap = cma_mmap,
  .release = cma_release
};

struct miscdevice cma_device =
{
  .minor = MISC_DYNAMIC_MINOR,
  .name = "cma",
  .fops = &cma_fops
};

static int __init cma_init(void)
{
  struct device_node *node;
  struct reserved_mem *rmem;
  int rc = misc_register(&cma_device);

  if(rc) return rc;

  /* The ALPHA15/ALPHA250 FPGA DMA ring lives in this dedicated CMA pool.
   * Keep CMA_ALLOC on the normal Linux pool for existing callers. */
  node = of_find_node_by_path("/reserved-memory/cma@18000000");
  if(!node) return 0;
  rmem = of_reserved_mem_lookup(node);
  of_node_put(node);
  if(!rmem || rmem->base != 0x18000000 ||
     rmem->size != 0x08000000 || !rmem->ops || !rmem->ops->device_init)
    return 0;

  rc = rmem->ops->device_init(rmem, cma_device.this_device);
  if(!rc) board_device = cma_device.this_device;
  return 0;
}

static void __exit cma_exit(void)
{
  cma_free();
  misc_deregister(&cma_device);
}

module_init(cma_init);
module_exit(cma_exit);
MODULE_LICENSE("MIT");
