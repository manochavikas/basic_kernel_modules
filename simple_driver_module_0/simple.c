#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>         // struct file_operations
#include <linux/sched.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>      // put_user
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/device.h>  // dynamic device node creation     
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>


#define LENTH 100  	// length of buffer is 100 bytes
static char * kbuffer = NULL;  // This buffer will be pointing to kmalloc memory that we will allocate in init 
static char  message[] = "Linux Device Driver, Advance - Simple Driver";

#define MAGIC 'R'
#define GETMSG _IOR(MAGIC, 1, char *)
#define SETMSG _IOW(MAGIC, 2, char *)

// information needed to fill in for Character device

static char simple_devname[]= "simple_drv"; // appears in /proc/devices

static int use_count = 0;

#define SIMPLE_MAJOR	249 // for static  major and minor device node. Don't need it if allocating dynamically
#define SIMPLE_MINOR	5  // For static minor. Don't need it if allocating dynamically

// decleration of file operation methods
static int my_open(struct inode *inode, struct file *filp);
static int simple_release(struct inode *inode, struct file *filp);
static ssize_t simple_write(struct file *filp, const char *buffer, size_t len, loff_t *offs);
static ssize_t simple_read(struct file *filp, char *buffer, size_t len, loff_t *offs);
static long simple_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
// static int simple_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg); 

static struct file_operations fops = {
  owner:   THIS_MODULE,
  read:    simple_read,       
  write:   simple_write,
  open:    my_open,
  release: simple_release,
  unlocked_ioctl:   simple_ioctl,
};

// http://www.makelinux.net/ldd3/chp-3-sect-9
static        dev_t   dev;   // device structure is used to represent major  and minor number in kernel 
static struct cdev   *simple_cdev;

unsigned int simple_major;   // not needed if dynamically allocating a major, minor 
unsigned int simple_mior; ;

static struct class *simple_class;  /* for sysfs entries. This will create a directory in sys  */

// init is the first driver routine that is get called when module is loaded or linked into live kernel
static int __init simple_init(void)
{
  int res=0, major;

  if(( kbuffer = kmalloc(LENTH, GFP_KERNEL)) == NULL)  // Allocates dynamic memory or kernel heap memory of length 100 bytes
   printk ("kmalloc failure\n");

  printk("Allocating kernel memory starting at address, where buffer is pointing: 0x%p\n", kbuffer);
  memset(kbuffer,0,LENTH);
  printk("Initializing kbuffer it with string Linux Device Driver, Advance - Simple Driver\n");
  memcpy(kbuffer, message, strlen(message));  //copy message into buffer 

  printk ("\n---- %s ---\n", kbuffer);

/** Information specific to char driver */

/**
  static device major and minor. We don't need it if we are using dynamic major allocation 
  simple_major = SIMPLE_MAJOR;
  simple_minor = SIMPLE_MINOR;

  dev = MKDEV(simple_major, simple_minor);  // dev structure represents the device node built with, major and minor numbers

  res = register_chrdev(dev,simple_devname,&fops);  //statically registering cdev structure and associating device node and file operation struct

  printk("The device is registered by Major no: %d\n",res);
  if(res == -1)
  {
    printk("Error in registering the module\n");
    return -1;
  }
*/

// Instead we allocate major number dynamically. On return dev structure will contain major and minor number assigned to our device node 
 res = alloc_chrdev_region(&dev, 0, 1, simple_devname);
 if (res<0) { return res; }

  major = MAJOR(dev); // extract device major number from device struct so that we know what major number is assigned to our device node 

 // allocate cdev structure and point to our device fops
  simple_cdev = cdev_alloc();    // allocate a cdev structure
  cdev_init(simple_cdev, &fops);  // initialize the cdev structure by associating  file operation structure that contains mapping of our driver methods
  simple_cdev->owner = THIS_MODULE;

// connect major number to cdev struct 
  res = cdev_add(simple_cdev, dev, 1);
  if (res<0){
    unregister_chrdev_region(dev, 1);
    return res;
  }

 // creates sysfs entry for simple_class. This will create a directory in the sys directory 
  simple_class = class_create(THIS_MODULE, simple_devname);
  if (IS_ERR(simple_class)) {     // macro IS_ERR checks for error return the errno to application and do clean up
      printk(KERN_ERR "Error creating simple_dev class \n");
      res = PTR_ERR(simple_class);   // return errno
      cdev_del(simple_cdev);  //free the cdev structure
      unregister_chrdev_region(dev, 1);  // releasing the major and minor number assigned to our device
      return -1;
  }

  /** 
      Interface to create device node dynamically via udev framework. Otherwise, you have to create device nodes 
      manually using mknod command. This requires name of the class,sysfs directory name, device struct, device node string
      First NULL represents parent directory. Considering we are creating simple_class in top directory we set it to NULL
      Second NULL represents data to be addede to the device for call backs. We don't use it
      It returns pointer to device structure on success. We don't need so we are not saving it.
  */

  device_create(simple_class, NULL, dev, NULL, simple_devname, 0); 
  printk(" %s init - major: %d  \r\n", simple_devname, major);

  return res;
}

static void __exit simple_exit(void)
{

 // Free the buffer allocated in init
  kfree(kbuffer);
  printk("Freeing kernel memory address where buffer is pointing: 0x%p\n", kbuffer);

// char driver specific 
  cdev_del(simple_cdev);
  unregister_chrdev_region(dev, 1);

  device_destroy(simple_class, dev);
  class_destroy(simple_class);

 /** For static device nodes, use
  unregister_chrdev(simple_major,simple_devname);
 */

  printk(" %s remove \r\n", simple_devname);
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_AUTHOR("Amer Ather - LDDA");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Description");


/* File Operation Methods like: open, read, write, poll, lseek etc.. invoked by user application when it issues a system call */

static int my_open(struct inode *inode, struct file *filp)
{

  use_count++; 

  printk(" %s Number of times open is called since driver loaded : %d \r\n", simple_devname, use_count);
  printk(" PID  of the process that called open: %d\n", current->pid );

  return 0;
}

static int simple_release(struct inode *inode, struct file *filp)
{
  int use;

  use = --use_count;

  printk(" %s close - count %d \r\n", simple_devname, use);

  return 0;
}

static ssize_t simple_write(struct file *filp, const char *buffer, size_t len, loff_t *offs)
{
	return copy_from_user(kbuffer, buffer, len) ? -EFAULT: 0;  //copying into kernel buffer, kbuffer, from user buffer sent by user application 
}

static ssize_t simple_read(struct file *filp, char *buffer, size_t len, loff_t *offs)
{
	return copy_to_user(buffer, kbuffer, len) ? -EFAULT: 0;  // copying into user buffer from kernel buffer, kbuffer
}

static long simple_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
// static int simple_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) 
{
 switch(cmd)
 {
      case GETMSG:
             if (!arg)         return -EINVAL;       //this  ioctl requires application buffer 
             if (copy_to_user((char *)arg, kbuffer, LENTH))   
                    return -EFAULT;
             printk(" kbuffer return:%s\n", kbuffer);
             break;
    case SETMSG:
            if (!arg)             return -EINVAL;  //this ioctl requires application buffer
            if (copy_from_user(kbuffer, (char *) arg, LENTH))  
             return  -EFAULT;    
            printk("kbuffer set to: %s\n", kbuffer);
            break;
     default:          /* unknown command */
     printk ("\n\n No matching IOCTL");
     return -ENOTTY;
 }
 return 0;
}
