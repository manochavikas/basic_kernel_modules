/* This is not a complete program, provided as an aid for writing mmap driver method */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>


static char *vmalloc_ptr = NULL;
static dev_t dev;
static struct cdev *test_mmap_cdev;
#define DEV_NAME	"TEST_MMAP"

#define LEN (16*1024)

int mmap_vmem(struct file *filp, struct vm_area_struct *vma)
{
        int ret;
        long length = vma->vm_end - vma->vm_start;
        unsigned long start = vma->vm_start;
        char *vmalloc_area_ptr = (char *)vmalloc_ptr;
        unsigned long pfn;

        /* Restrict it to size of device memory */
        if (length > LEN * PAGE_SIZE)
                return -EIO;

        /** 
	  * Considering vmalloc pages are not contiguous in physical memory
          * You need to loop over all pages and call remap_pfn_range 
	  * for each page individuallay. Also, use 
          * vmalloc_to_pfn(vmalloc_area_prt)
	  * instead to get the page frame number of each virtual page
	  */
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
        while (length > 0) {
                pfn = vmalloc_to_pfn(vmalloc_area_ptr);
                printk("vmalloc_area_ptr: 0x%p \n", vmalloc_area_ptr);

                if ((ret = remap_pfn_range(vma, start, pfn, PAGE_SIZE,
                                           vma->vm_page_prot)) < 0) {
                        return ret;
                }
                start += PAGE_SIZE;
                vmalloc_area_ptr += PAGE_SIZE;
                length -= PAGE_SIZE;
        }
        return 0;
}

static int mmap_vmalloc(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}
static int test_vmalloc_open(struct inode *inode, struct file *file)
{
  /* reserve vmalloc memory to make them remapable */
        for (virt_addr=(unsigned long)vmalloc_ptr; 
	        virt_addr < (unsigned long)vmalloc_ptr + LEN; 
			virt_addr+=PAGE_SIZE) {
                           SetPageReserved(vmalloc_to_page((unsigned long *)virt_addr));
                        }
        printk("vmalloc_ptr: 0x%p\n" , vmalloc_ptr);
        printk("vmalloc_ptr :0x%p \t physical Address 0x%llx)\n", vmalloc_ptr,
                         virt_to_phys((void *)(vmalloc_ptr)));
 
	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);
	memset(vmalloc_ptr, 'Z', 2 * PAGE_SIZE);
	return 0;

}
static struct  file_operations test_mmap_ops = {
	.open = test_vmalloc_open,
	.mmap =	mmap_vmalloc,
};
static int register_this_driver_as_char(void)
{
	int major, result;
	result = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
	if(result) {
		printk(KERN_ALERT "not able to allocated character region");
		return result;
	}
	major = MAJOR(dev);
	printk(KERN_ALERT "mmap major number = %d", major);

	test_mmap_cdev = cdev_alloc();
	cdev_init(test_mmap_cdev, &test_mmap_ops);
	test_mmap_cdev->owner = THIS_MODULE;

	result = cdev_add(test_mmap_cdev, dev, 1);
	if (result) {
		printk("not able to register %s module\n", DEV_NAME);
		unregister_chrdev_region(dev, 1);
		return result;
	}
	printk(KERN_ALERT "%s registered as char driver\n", DEV_NAME);

	return 0;
}
static void char_driver_cleanup(void)
{
	printk("free cdev, test_mmap_cdev address = %p\n", (char *)test_mmap_cdev);
	cdev_del(test_mmap_cdev);
	printk("free dev_t device allocated for character driver\n");
	unregister_chrdev_region(dev, 1);

}
static int __init mmap_vmem_init_module (void) {

  unsigned long virt_addr;
/* Do required char driver initialization, see helloplus.c as an example */
	int ret;

	ret = register_this_driver_as_char();
	if(ret)
		return ret;

	/* Allocate  memory  with vmalloc. It is already page aligned */
        vmalloc_ptr = ((char *)vmalloc(LEN));
        if (!vmalloc_ptr) {
                printk("vmalloc failed\n");
                return -ENOMEM;
        }
        printk("vmalloc_ptr at 0x%p \n", vmalloc_ptr);

       /**
	  *  Initialize memory with "abcdefghijklmnopqrstuvwxyz" to 
          *  distinguish between kmalloc and vmalloc initialized memory. 
	  */

	/* CODE HERE */

        return 0;
}

// close and cleanup module
static void __exit mmap_vmem_cleanup_module (void) {
	unsigned long virt_addr;
        printk("cleaning up module\n");

        for (virt_addr=(unsigned long)vmalloc_ptr; virt_addr < (unsigned long)vmalloc_ptr + LEN;
                virt_addr+=PAGE_SIZE) {
                        // clear all pages
                        ClearPageReserved(vmalloc_to_page((unsigned long *)virt_addr));
        }
        vfree(vmalloc_ptr);
	/* Also all the required cleanup for character drivers */
	char_driver_cleanup();
}
module_init(mmap_vmem_init_module);
module_exit(mmap_vmem_cleanup_module);
