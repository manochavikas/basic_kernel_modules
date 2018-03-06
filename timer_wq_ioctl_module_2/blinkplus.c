/* 
 *  kbleds.c - Blink keyboard leds until the module is unloaded.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/tty.h>		/* For fg_console, MAX_NR_CONSOLES */
#include <linux/kd.h>		/* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/workqueue.h>

MODULE_DESCRIPTION("Example module illustrating the use of Keyboard LEDs.");
MODULE_AUTHOR("Daniele Paolo Scarpazza");
MODULE_LICENSE("GPL");

struct timer_list my_timer;
struct tty_driver *my_driver;
char kbledstatus = 0;

#define BLINK_DELAY   (HZ/5)
#define ALL_LEDS_ON   0x07
#define RESTORE_LEDS  0xFF

int cur_expire_jiffies = BLINK_DELAY;
static dev_t   dev;
static struct cdev  *blink_cdev;
static char *my_dev_name = "blinkplus";  // This will appears in /proc/devices
int selected_led = ALL_LEDS_ON;

#define MAGIC 'z'
#define GETINV  _IOR(MAGIC, 1, int *)
#define SETLED  _IOW(MAGIC, 2, int *)
#define SETINV  _IOW(MAGIC, 3, int *)

/*
 * Function my_timer_func blinks the keyboard LEDs periodically by invoking
 * command KDSETLED of ioctl() on the keyboard driver. To learn more on virtual 
 * terminal ioctl operations, please see file:
 *     /usr/src/linux/drivers/char/vt_ioctl.c, function vt_ioctl().
 *
 * The argument to KDSETLED is alternatively set to 7 (thus causing the led 
 * mode to be set to LED_SHOW_IOCTL, and all the leds are lit) and to 0xFF
 * (any value above 7 switches back the led mode to LED_SHOW_FLAGS, thus
 * the LEDs reflect the actual keyboard status).  To learn more on this, 
 * please see file:
 *     /usr/src/linux/drivers/char/keyboard.c, function setledstate().
 * 
 */

static void my_timer_func(unsigned long ptr)
{
	int *pstatus = (int *)ptr;

	if (*pstatus == selected_led)
		*pstatus = RESTORE_LEDS;
	else
		*pstatus = selected_led;

	/** vc_cons is a struct type vc that contains a pointer to a 
	  * virtual console (d) of type vc_data	vc_tty is the tty port where 
	  * console is attached, fg_console is the current console 
	  * vt_ioctl(struct tty_struct *tty, struct file *file, 	
	  *                         unsigned int cmd, unsigned long arg)
	  */
	
	// calling ioctl in timer function may result in hang. -- FIX IT
//	(my_driver->ops->ioctl) (vc_cons[fg_console].d->vc_tty, NULL, KDSETLED, *pstatus);

	(my_driver->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, *pstatus);

	my_timer.expires = jiffies + cur_expire_jiffies;
        printk(KERN_ALERT "my_timer_func, %d\n", *pstatus);
	add_timer(&my_timer);
}

static ssize_t blink_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	arg = *(int *)arg;
	switch(cmd) {
	case SETINV:
		cur_expire_jiffies = HZ * arg;
		//printk(KERN_ALERT "inside SETINV, cur_expire_jiffies ticks = %d\n", cur_expire_jiffies);
		//mod_timer(&my_timer, jiffies + cur_expire_jiffies);
		return 0;
	case GETINV:
		printk(KERN_ALERT "inside GETINV");
		return cur_expire_jiffies/HZ;
	case SETLED:
		printk(KERN_ALERT "inside SETLED");
		selected_led &= ~ALL_LEDS_ON;
		selected_led |= arg;
		return 0;
	default:
		printk("blinkplus: unsupported choice\n");
	};
	return -ENOTTY;
}

static int blink_open(struct inode *inode, struct file *file)
{
	printk("inside blink open\n");
	return 0;
}

static struct file_operations blink_fops = {
	.open 		= blink_open,
	.unlocked_ioctl = blink_ioctl,
//	.close		= blink_close,
};

int register_this_driver_as_char(void)
{
	int major, result;
	result = alloc_chrdev_region(&dev, 0, 1, my_dev_name);
	if(result) {
		printk("not able to allocate major/minor number to blinkplus driver\n");
		return result;
	}
	major = MAJOR(dev);
		printk("driver major number allocated is %d\n", major);

	blink_cdev = cdev_alloc();
	cdev_init(blink_cdev, &blink_fops);
	blink_cdev->owner = THIS_MODULE;

	result = cdev_add(blink_cdev, dev, 1);
	if(result) {
		printk("not able to register blinkplus module\n");
		unregister_chrdev_region(dev, 1);
		return result;
	}
	printk("blinkplus line number : %d\n", __LINE__);
	return 0;
}
static int __init kbleds_init(void)
{
	int i, result;

	printk(KERN_INFO "kbleds: loading\n");
	printk(KERN_INFO "kbleds: fgconsole is %x\n", fg_console);
	printk(KERN_INFO "kbleds: HZ value is %d\n", HZ);
	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (!vc_cons[i].d)
			break;
		printk(KERN_INFO " console[%i/%i] #%i, tty %lx\n", i,
		       MAX_NR_CONSOLES, vc_cons[i].d->vc_num, (unsigned long)vc_cons[i].d->port.tty);
		}
	printk(KERN_INFO "kbleds: finished scanning consoles\n");

	my_driver = vc_cons[fg_console].d->port.tty->driver;
	printk(KERN_INFO "kbleds: tty driver magic %x\n", my_driver->magic);
	/*
		register driver as char driver & connect it to its fops
	*/
	result = register_this_driver_as_char();
	if(result)
		return result;

	printk("blinkplus registered as char device\n");

	/*
	 * Set up the LED blink timer the first time
	 */
	init_timer(&my_timer);
	my_timer.function = my_timer_func;
	my_timer.data = (unsigned long)&kbledstatus;
	my_timer.expires = jiffies + cur_expire_jiffies;
	add_timer(&my_timer);

	return 0;
}

static void __exit kbleds_cleanup(void)
{
	printk(KERN_INFO "kbleds: unloading...\n");
	del_timer(&my_timer);
	(my_driver->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, RESTORE_LEDS);

	cdev_del(blink_cdev);
	unregister_chrdev_region(dev, 1);
}

module_init(kbleds_init);
module_exit(kbleds_cleanup);
