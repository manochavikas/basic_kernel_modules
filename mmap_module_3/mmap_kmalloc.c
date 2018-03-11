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
static char *vmalloc_ptr = NULL;

#define LEN (16*1024) 

unsigned long virt_addr;
static dev_t dev;
#define DEV_NAME	"TEST_MMAP"
static struct cdev *test_mmap_cdev;
bool read_flag = true;
#define MINOR_NUMBER_0	0
#define MINOR_NUMBER_1	1
static struct file_operations test_mmap_ops;

static int mmap_kmalloc(struct file * filp, struct vm_area_struct * vma)
{
	int ret;
	unsigned long length;
	length = vma->vm_end - vma->vm_start;
	printk("file->private_data = %d\n", *((unsigned int *)(filp->private_data)));

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

static int mmap_vmalloc(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	unsigned long pfn;
	char *vmalloc_area_ptr = vmalloc_ptr;
	unsigned long start = vma->vm_start;
	unsigned int length = vma->vm_end - vma->vm_start;


	printk("file->private_data = %d\n", *((unsigned int *)(filp->private_data)));
	if(length > 2 * PAGE_SIZE)
		return -EIO;

	/*
	 * vmalloc pages are not contigous in physical memory while
	 * remap_pfn_range works on continous physical length. So we
	 * need to remap_pfn_range for each page one by one.
	 * vmalloc_to_pfn(vmalloc_pointer) provides us the physical
	 * address for the page (actually page framen number) vmalloc_pointer
	 * is in.
	 * */
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
	while(length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		ret = remap_pfn_range(vma, start, pfn, PAGE_SIZE, vma->vm_page_prot);
		if (ret)
			return ret;

		vmalloc_area_ptr += PAGE_SIZE;
		start += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	return 0;
}

static ssize_t test_mmap_proc_read(struct file *file, char __user *buf, size_t size,
			       loff_t *offset)
{
	char *alloc_area;
	int len = 2 * PAGE_SIZE;
	alloc_area = kmalloc_area;
	printk("proc read only valid for kmalloc area, so minor no 0\n");

	if(read_flag == true)
		read_flag = false;
	else {
		read_flag = true;
		return 0;
	}

	printk("Hello from read_proc\n");
	copy_to_user(buf, alloc_area, len);
	return len;
}

static int test_mmap_open(struct inode *inode, struct file *file)
{
	int i, j;
	int *priv;
	char *alloc_area;

	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);
	printk("Major number = %d & minor number = %d\n", imajor(inode), iminor(inode));
	priv = kmalloc(sizeof(int), GFP_KERNEL);
	*priv = iminor(inode);
	file->private_data = priv;
	printk("minor number stored in file private data pointer = %d\n", *((int *)(file->private_data)));

	if(*priv == MINOR_NUMBER_0) {
		alloc_area = kmalloc_area;
		test_mmap_ops.mmap = mmap_kmalloc;
	}
	else if(*priv == MINOR_NUMBER_1) {
		alloc_area = vmalloc_ptr;
		test_mmap_ops.mmap = mmap_vmalloc;
	}
	else {
		printk("not supported minor number");
		return -1;
	}

	/* reserve kmalloc memory as pages to make them remapable */
	for (virt_addr = (unsigned long)alloc_area;
	     virt_addr < (unsigned long)alloc_area + ( 2 * PAGE_SIZE);
	     virt_addr += PAGE_SIZE) {
		if(*priv == MINOR_NUMBER_0)
			SetPageReserved(virt_to_page((unsigned long *)virt_addr));
		else
			SetPageReserved(vmalloc_to_page((unsigned long *)virt_addr));
	}

	printk("alloc_area \t:0x%p \nphysical Address \t: 0x%llx\n", alloc_area,
	       virt_to_phys((void *)(alloc_area)));

	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);
	/**
	 *  Write code to init memory with ascii 0123456789. Where ascii
	 *  equivalent of 0 is 48  and 9 is 58. This is read from mmap() by
	 *  user level application
	 */
	memset(alloc_area, 'Z', 2 * PAGE_SIZE);
	for(i = 0; i < 2 * PAGE_SIZE; ) {
		for(j = 0; j < 10 && i < 2 * PAGE_SIZE; j++) {
			*(alloc_area + i) = '0' + j;
			i++;
		}
		j  = 0;
	}

	return 0;
}

static int test_mmap_release(struct inode *inode, struct file *file)
{
	char *alloc_area;
	int minor_number = *((int *)(file->private_data));
	printk("minor number stored in file private data pointer = %d\n", *((int *)(file->private_data)));

	if(minor_number == MINOR_NUMBER_0)
		alloc_area = kmalloc_area;
	else if(minor_number == MINOR_NUMBER_1)
		alloc_area = vmalloc_ptr;
	else {
		printk("%s, not supported minor number = %d\n", __FUNCTION__, minor_number);
		return -1;
	}

	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);

	for(virt_addr = (unsigned long)alloc_area;
	    virt_addr < (unsigned long)alloc_area + (2 * PAGE_SIZE);
	    virt_addr += PAGE_SIZE) {
		// clear all pages
		if(minor_number == MINOR_NUMBER_0)
			ClearPageReserved(virt_to_page((unsigned long *)virt_addr));
		else
			ClearPageReserved(vmalloc_to_page((unsigned long *)virt_addr));
	}

	kfree(file->private_data);
	return 0;
}

static int test_mmap_flush(struct file *file, fl_owner_t id)
{
	printk("inside function %s, line = %d\n", __FUNCTION__, __LINE__);

	return 0;
}

static struct file_operations test_mmap_proc_ops = {
	.read = test_mmap_proc_read,
};

static struct file_operations test_mmap_ops = {
	.open = test_mmap_open,
	.flush = test_mmap_flush,
	.release = test_mmap_release,
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

static int __init mmap_kmalloc_init_module (void)
{
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
	printk("kmalloc_ptr \t: 0x%p \n", kmalloc_ptr);

	/**
	 * This is the same as:
	 * (int *)((((unsigned long)kmalloc_ptr) + ((1<<12) - 1)) & 0xFFFF0000);
	 * where: PAGE_SIZE is defined as 1UL <<PAGE_SHIFT.
	 * That is 4k on x86. 0xFFFF0000 is a PAGE_MASK to mask out the upper
	 * bits in the page. This will align it at 4k page boundary that means
	 * kmalloc start address is now page aligned.
	 */

	kmalloc_area = (char *)(((unsigned long)kmalloc_ptr + PAGE_SIZE -1) & PAGE_MASK);

	printk("kmalloc_area \t: 0x%p\n", kmalloc_area);

	vmalloc_ptr = vmalloc(2 * PAGE_SIZE);
	if (!vmalloc_ptr) {
		printk(KERN_ALERT "not able to allocate memory to vmalloc_ptr\n");
		return -ENOMEM;
	}
	printk("vmalloc_ptr \t: 0x%p\n", vmalloc_ptr);
	return 0;
}


static void char_driver_cleanup(void)
{
	printk("free cdev, test_mmap_cdev address = %p\n", (char *)test_mmap_cdev);
	cdev_del(test_mmap_cdev);
	printk("free dev_t device allocated for character driver\n");
	unregister_chrdev_region(dev, 1);

}
// close and cleanup module

static void __exit mmap_kmalloc_cleanup_module (void) {
	printk("cleaning up %s module\n", DEV_NAME);
	printk("freeing memeory allocated to kmalloc_ptr : %p\n", kmalloc_ptr);
	kfree(kmalloc_ptr);
	printk("freeing memeory allocated to vmalloc_ptr : %p\n", vmalloc_ptr);
	vfree(vmalloc_ptr);

	// Also all required clean up for character drivers
	char_driver_cleanup();
	printk("remove test_mmap_proc_hello entry\n");
	remove_proc_entry("test_mmap_proc_hello", NULL);
}

MODULE_AUTHOR("Vikas MANOCHA");
MODULE_LICENSE("GPL");
module_init(mmap_kmalloc_init_module);
module_exit(mmap_kmalloc_cleanup_module);
