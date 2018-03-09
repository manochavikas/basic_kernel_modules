/**
 *  This is not a complete program. Provided as an aid for 
 *  developing  mmap driver method 
 */

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
#include <linux/proc_fs.h>

static char *kmalloc_area = NULL;
static char *kmalloc_ptr = NULL;

#define LEN (16*1024) 

unsigned long virt_addr;
static dev_t dev;
#define DEV_NAME	"TEST_MMAP"
static struct cdev *test_mmap_cdev;
bool read_flag = true;

static int mmap_kmalloc(struct file * filp, struct vm_area_struct * vma)
{
	int ret;
	unsigned long length;
	length = vma->vm_end - vma->vm_start;

	// Restrict to size of device memory

	if (length > LEN * PAGE_SIZE)
		return -EIO;

	/**
	 * remap_pfn_range function arguments:
	 * vma: vm_area_struct has passed to the mmap method
	 * vma->vm_start: start of mapping user address space
	 * Page frame number of first page that you can get by:
	 *   virt_to_phys((void *)kmalloc_area) >> PAGE_SHIFT
	 * size: length of mapping in bytes which is simply vm_end - vm_start
	 * vma->>vm_page_prot: protection bits received from the application
	 */

	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	ret = remap_pfn_range(
			vma, 
			vma->vm_start,
			virt_to_phys((void*)((unsigned long)kmalloc_area)) >> PAGE_SHIFT,
			vma->vm_end-vma->vm_start,
			vma->vm_page_prot 
			);
	if(ret != 0) {
		return -EAGAIN;
	}
	return 0;
}

static int test_mmap_proc_read(struct file *file, char __user *buf, size_t size,
			       loff_t *offset)
{
	int len = 2 * PAGE_SIZE;

	if(read_flag == true)
		read_flag = false;
	else {
		read_flag = true;
		return 0;
	}

	printk("Hello from read_proc\n");
	copy_to_user(buf, kmalloc_area, len);
	return len;
}

static int test_mmap_open(struct inode *inode, struct file *file)
{
	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static struct file_operations test_mmap_proc_ops = {
	.read = test_mmap_proc_read,
};

static struct  file_operations test_mmap_ops = {
	.open = test_mmap_open,
	.mmap =	mmap_kmalloc,
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

static int __init mmap_kmalloc_init_module (void)
{
	int i, j;
	int ret;

	ret = register_this_driver_as_char();
	if(ret)
		return ret;

	proc_create("test_mmap_proc_hello", 0, NULL, &test_mmap_proc_ops);
	read_flag = true;
	/**
	 * kmalloc() returns memory in bytes instead of PAGE_SIZE
	 * mmap memory should be PAGE_SIZE and aligned on a PAGE boundary.
	 */

	kmalloc_ptr = kmalloc(LEN + (2 * PAGE_SIZE), GFP_KERNEL);
	if (!kmalloc_ptr) {
		printk("kmalloc failed\n");
		return -ENOMEM;
	}
	printk("kmalloc_ptr at 0x%p \n", kmalloc_ptr);

	/**
	 * This is the same as:
	 * (int *)((((unsigned long)kmalloc_ptr) + ((1<<12) - 1)) & 0xFFFF0000);
	 * where: PAGE_SIZE is defined as 1UL <<PAGE_SHIFT.
	 * That is 4k on x86. 0xFFFF0000 is a PAGE_MASK to mask out the upper
	 * bits in the page. This will align it at 4k page boundary that means
	 * kmalloc start address is now page aligned.
	 */

	kmalloc_area = (char *)(((unsigned long)kmalloc_ptr + PAGE_SIZE -1) & PAGE_MASK);

	printk("kmalloc_area: 0x%p\n", kmalloc_area);

	/* reserve kmalloc memory as pages to make them remapable */
	for (virt_addr=(unsigned long)kmalloc_area; virt_addr < (unsigned long)kmalloc_area + LEN;
			virt_addr+=PAGE_SIZE) {
		SetPageReserved(virt_to_page(virt_addr));
	}
	printk("kmalloc_area: 0x%p\n" , kmalloc_area);
	printk("kmalloc_area :0x%p \t physical Address 0x%llx)\n", kmalloc_area,
			virt_to_phys((void *)(kmalloc_area)));

	/**
	 *  Write code to init memory with ascii 0123456789. Where ascii
	 *  equivalent of 0 is 48  and 9 is 58. This is read from mmap() by
	 *  user level application
	 */
	memset(kmalloc_area, 'Z', 2 * PAGE_SIZE);
	for(i = 0; i < 2 * PAGE_SIZE; ) {
		for(j = 0; j < 10; j++) {
			*(kmalloc_area + i) = '0' + j;
			i++;
		}
		j  = 0;
	}

	return 0;
}


static void char_driver_cleanup(void)
{
	/* free cdev */
	cdev_del(test_mmap_cdev);
	/* free dev_t device allocated for character driver */
	unregister_chrdev_region(dev, 1);

}
// close and cleanup module

static void __exit mmap_kmalloc_cleanup_module (void) {
	printk("cleaning up %s module\n", DEV_NAME);

	for (virt_addr=(unsigned long)kmalloc_area; virt_addr < (unsigned long)kmalloc_area + LEN;
			virt_addr+=PAGE_SIZE) {
		// clear all pages
		ClearPageReserved(virt_to_page(virt_addr));
	}
	kfree(kmalloc_ptr);

	// Also all required clean up for character drivers
	char_driver_cleanup();
	remove_proc_entry("test_mmap_proc_hello", NULL);
}

MODULE_AUTHOR("Vikas MANOCHA");
MODULE_LICENSE("GPL");
module_init(mmap_kmalloc_init_module);
module_exit(mmap_kmalloc_cleanup_module);
