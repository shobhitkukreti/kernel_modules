/* Kernel Module which allows MMAP on a character driver file from userspace
 * *
 * *
 * * Author: Shobhit Kukreti
 * */


#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("T-1000");
MODULE_DESCRIPTION("Char Dev Driver");

#define DEVICE_NAME "som_mmap"


struct som_mmap_struct {
  dev_t num;
  char *data;
  struct cdev cdev_struct;
};

struct som_mmap_struct som_mmap_drv;

struct class *class_desc;


int som_open (struct inode *in, struct file *filp) {

  printk("PCI Test Open Function\n");
  return 0;
}


int som_release ( struct inode *in, struct file *filep) {
  
  printk("PCI TEST Release Function\n");
  kfree(som_mmap_drv.data);
  return 0;
}

void som_mmap_open_fn ( struct vm_area_struct *vma )
{
  printk(KERN_INFO "PCI_VMA_OPEN, Virt %lx, Phy %lx\n",
    vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void som_mmap_close_fn ( struct vm_area_struct *vma )
{
  printk(KERN_INFO "PCI_VMA_CLOSE, Virt %lx, Phy %lx\n",
    vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);

}


struct vm_operations_struct som_mmap_ops = 
{
  .open = som_mmap_open_fn, //mmap_open,
  .close = som_mmap_close_fn, // mmap_close,
  .fault = NULL, //mmap_fault,

};

int som_mmap_fn(struct file *filp, struct vm_area_struct *vma) 
{
  int ret=0;
  unsigned long size = (unsigned long)vma->vm_end - vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  printk(KERN_INFO "Buf Size Req:%lu\n", size);
  som_mmap_open_fn(vma);
  som_mmap_drv.data = kmalloc(sizeof(char)*size, GFP_KERNEL);
  
  if(!som_mmap_drv.data) {
    printk(KERN_ERR "KMALLOC_Failed\n");
    ret = -ENOMEM;
  }

  printk(KERN_INFO "KMALLOC Addr %lx",(unsigned long)som_mmap_drv.data);
  memcpy(som_mmap_drv.data, "Hello\n", 7);  
  
  ret |= remap_pfn_range(vma, start,virt_to_phys((void*)som_mmap_drv.data)\
       >> PAGE_SHIFT, size, vma->vm_page_prot);

  if(ret !=0)
    printk(KERN_ERR "REMAP_PFN_ERR: %d\n", ret);

  vma->vm_ops = &som_mmap_ops;
  vma->vm_private_data = filp->private_data;

  return ret;
}


static struct file_operations fops = {

        .owner = THIS_MODULE,
        .open  = som_open,
        .release = som_release,
  .mmap = som_mmap_fn,
  
};

/* Chardev Init */
int chardev_init(void)
{

  
  class_desc = class_create(THIS_MODULE, DEVICE_NAME);
  if(alloc_chrdev_region(&som_mmap_drv.num, 0, 1, DEVICE_NAME) < 0) 
  {
                printk(KERN_DEBUG "Cannot register device '%s'\n",DEVICE_NAME);
                return -1;
        }

  cdev_init(&(som_mmap_drv.cdev_struct), &fops);
  som_mmap_drv.cdev_struct.owner = THIS_MODULE; 

  if(cdev_add(&som_mmap_drv.cdev_struct,som_mmap_drv.num,1)) {

                printk(KERN_ALERT "cdev_add failed '%s'\n",DEVICE_NAME);
                return -1;
        }

  device_create(class_desc, NULL, som_mmap_drv.num, NULL, DEVICE_NAME);
  printk(KERN_INFO "PCI_MMAP INIT\n");

  return 0;

}


static int __init char_init(void)
{
  if(chardev_init()!=0)
    return -1;

  return 0;

}


static void __exit char_cleanup(void) 
{

  printk(KERN_INFO "PCI_MMAP Cleanup\n");
  cdev_del(&som_mmap_drv.cdev_struct);
  unregister_chrdev_region(som_mmap_drv.num, 1);
  device_destroy(class_desc, som_mmap_drv.num);
  class_destroy(class_desc);

}

module_init(char_init);
module_exit(char_cleanup);
