/*
 * 
 * Implement USB portion of rtl8139 Network Card driver
 * Use this as a template. Add code in areas matching string "CODE HERE".  
 * In this phase of the project we will be writing USB routines. 
 * Compile the driver as module. Use the makefile provided earlier
 * Make sure to unload the production module: 
 * # lsmod|grep rtl8150 
 * # rmmod rtl8150  
 * # lsmod 
 * Load the driver and run "ifconfig -a", You should see the MAC 
 * Address read from the device memory. 
 *
 * Guidelines are provided to assist you with writing a usb portion of the 
 * driver. Do not limit yourself to it. You are encouraged to review source 
 * code of production driver. 
 */

#include <linux/init.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/usb.h>
#include <asm/uaccess.h>

#define DRV_NAME "LDDA_USB"  // Use it to change name of interface from eth

// rtl8150 Datasheet - Page 9, Vendor Specific Memory Read/Write Commands

#define RTL8150_REQT_READ       0xc0
#define RTL8150_REQT_WRITE      0x40

#define RTL8150_REQ_GET_REGS    0x05
#define RTL8150_REQ_SET_REGS    0x05

// Register offset in device memory  - rtl8150 Datasheet page 17 

#define	IDR	0x0120  	//  Device memory where MAC address is found

#define VID_REALTEK	0X0BDA
#define PID_RTL_8150	0x8150
#define HAVE_NET_DEVICE_OPS
/* Device private structure */

struct rtl8150 {
	struct usb_device *udev; 	// USB device
	struct net_device *netdev;	// Net device
	struct net_device_stats stats;  // Net device stats
	spinlock_t lock;

	// Add rtl8150 device specific stuff later
};

static int rtl8150_open(struct net_device *dev)
{
	printk("rtl8150_open: Add code later\n");
        netif_start_queue(dev); /* transmission queue start */
        return 0;

}

static int rtl8150_close(struct net_device *dev)
{
        printk("rtl8150_close: Add code later \n");
        netif_stop_queue(dev); /* transmission queue stop */
        return 0;

}

static int rtl8150_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	printk("rtl8150_start_xmit: Add code later\n");
        dev_kfree_skb(skb); /* Just free it for now */

        return 0;
}

static struct net_device_stats* rtl8150_get_stats(struct net_device *dev)
{
        struct rtl8150 *priv = netdev_priv( dev );

        printk("dev_get_stats: Add code later\n");

        /**
         * You cannot return NULL, make sure to return the address
         * of net_dev_stat that is in device private structure
         */

        /* CODE HERE */
}

#ifdef HAVE_NET_DEVICE_OPS
static struct net_device_ops rtl8150_netdev_ops = {
        .ndo_open               = rtl8150_open,
        .ndo_stop               = rtl8150_close,
        .ndo_get_stats          = rtl8150_get_stats,
        .ndo_start_xmit         = rtl8150_start_xmit
};
#endif

/********* USB ROUTINES*************/

#define TIMEOUT_JIFFIES		500

static int get_rtl_8150_register(struct usb_device *udev, u32 index, char *buff, int size)
{
	unsigned int pipe = usb_rcvctrlpipe(udev, 0); // receive in ep0
	return(usb_control_msg(udev, pipe, RTL8150_REQ_GET_REGS, RTL8150_REQT_READ,
			      index, 0, buff, size, TIMEOUT_JIFFIES));
}

static int set_rtl_8150_register(struct usb_device *udev, u32 index, char *buff, int size)
{
	unsigned int pipe = usb_sndctrlpipe(udev, 0); // send in ep0
	return(usb_control_msg(udev, pipe, RTL8150_REQ_SET_REGS, RTL8150_REQT_WRITE,
			      index, 0, buff, size, TIMEOUT_JIFFIES));
}


static int rtl8150_probe(struct usb_interface *intf,
                         const struct usb_device_id *id)
{
        struct net_device *netdev;
        struct rtl8150 *priv;
	int ret;

	/* extract usb_device from the usb_interface structure */
	/* CODE HERE */
	struct usb_device *udev = interface_to_usbdev(intf);

	/**
          * Linux Network Stack works with network device not the USB device. 
          * We need to allocate an ethernet network device. alloc_etherdev() 
          * allocates net device structure with memory allocated for 
          * rtl8150 private structure. Return ENOMEM if failed
	  */
	
	/* CODE HERE */
	netdev = alloc_etherdev(sizeof(struct rtl8150));
	if(!netdev)
		return -ENOMEM;
	priv = netdev_priv(netdev);
	priv->netdev = netdev;
	priv->udev = udev;

	/** 
         * Set the device name to DRV_NAME instead of eth via memcpy 
	 *
         * What routine provides access to device's private structure from the 
         * net_device instance 
         */

	/* CODE HERE */

        /* sysfs stuff. Sets up device link in /sys/class/net/interface_name */
        SET_NETDEV_DEV(netdev, &intf->dev);

#ifdef HAVE_NET_DEVICE_OPS
        netdev->netdev_ops = &rtl8150_netdev_ops;
#else
        netdev->open = rtl8150_open;
        netdev->stop = rtl8150_close;
        netdev->hard_start_xmit = rtl8150_start_xmit;
        netdev->get_stats = rtl8150_get_stats;
#endif

	/* Initialize Device private structure and initialize the spinlock*/

	/* CODE HERE */


	/* Register netdevice. If fail goto out  */
	
	/* CODE HERE */

        /**
          * You can stuff device private structure into USB Interface 
	  * structure using usb_set_intfdata. That can be retrieved later 
	  * using usb__get_drvdata, for example, in disconnect or other driver 
	  * functions
          */

	/* CODE HERE */

	/**
          * Update net device struct with MAC and Broadcast address
          * RealTek8150 datasheet - page 17 states that IDR registers, IDR0-5, 
          * found at device memory location 0x0120 contains MAC Address of the
	  * the NIC device
	  * 
	  * Fill net device netdev->dev_addr[6] and netdev->broadcast[6] 
	  * arrays.  For broadcast address fill all octets with 0xff. 
	  * 
	  * In USB, you don't map device memory, instead you submit
	  * control urb to USB core when needs to read/write device register memory. 
	  * 
	  * The structure that sends the control request to a USB device has to 
	  * conform to USB specification (chapter 9) and is defined in 
	  *	include/linux/usb/ch8.h
	  *
	  * struct usb_ctrlrequest {
	  *	__u8 bRequestType;
	  *	__u8 bRequest;
	  *	__le16 mValue;
	  *	__le16 wIndex;
	  *	__le16 wLength;
	  * } __attribute__ ((packed));
	  * 
	  * Read IDR memory location in device memory by submitting 
	  * usb_control_msg() to USB core
	  * int usb_control_msg(struct usb_device *dev, unsigned int pipe,
	  * 		__u8 request, __u8 requesttype, __u16 value, 
          *		__u16 index, void *data, __u16 size, int timeout);
	  * where: 
	  *   - struct usb_device *dev: pointer to the target USB device
	  *   - unsigned int pipe: endpoint of the target USB device where
          *	this message will be sent. This value is created  by calling
	  *	usb_snd|usb_rcvctrlpipe
	  *   - __u8 request: Requets value for the control message 
	  *		- vendor specific (RTL8150_REQ_GET_REGS)
	  *   - __u8 requesttype: Request type 
		        - vendor specific (RTL8150_REQT_READ)
	  *   - __u16 value: the USB message value for the control message
	  *	   driver uses it to specify the memory location in 
		   device memory such as IDR register location.
	  *   - __u16 indx: Desired offset.  driver sets it to 0 
	  *	            -  means start of the requested memory location 
	  *   - void *data: A pointer to the data to send to the device if this
	  *	 is an OUT endpoint. If this is an IN endpoint, this is a
	  *	 pointer to where the data should be placed after being read
	  *	 from the device. 
	  *   - __u16 size: size of the buffer that is pointed to by the data
	  *	 paramter above. It is a number of bytes to transfer
	  *   - int timeout: The amount of time, in jiffies, that should be
	  *	waited before timing out. If this value is 0, the function
	  *	will wait forever for the message to complete. For USB compliant
	  *	host, device requests with a data stage must start to return data
	  *	in 500ms after the request.
	  *
	  *   Return Value: on success, it returns the number of bytes that
	  *   are transferred to or from the device, othervise negative number
	  *		
	  *  Some parameters for usb_control_msg: request, requesttype, value, 
	  * indx,size map directly to the USB specification for how a USB 
	  * control message is defined.
	  *   	  
	  * usb_control_msg or usb_bulk_msg cannot be called from the 
	  * interrupt context. Also, this function cannot be cancelled by
	  * any other function, so be careful when using it; make sure that
	  * your driver disconnect function knows enough to wait for the call
	  * to complete before allowing itself to be unloaded from memory
	  * 
	  * Read six bytes into net_device structure member dev_addr from 
	  * device memory location IDR 
	  */ 

	  /* CODE HERE */

	ret = get_rtl_8150_register(udev, IDR, netdev->dev_addr, sizeof(netdev->dev_addr));
	if(ret) {
		dev_err(&udev->dev, "device unable to read rtl 8150 registers");
		goto out;
	}
//	printk(KERN_ALERT "dev_addr = %x", netdev->dev_addr);

        /* Length of Ethernet frame. It is a "hardware header length", number 
         * of octets that lead the transmitted packet before IP header, or 
         * other protocol information.  Value is 14 for Ethernet interfaces.
         */

        netdev->hard_header_len = 14;

        usb_set_intfdata(intf, priv);
	return 0;

out:
        usb_set_intfdata(intf, NULL);
        free_netdev(netdev);
        return -EIO;
}
/* USB disconnect routine - required else can't rmmod */
static void rtl8150_disconnect(struct usb_interface *intf)
{
	/* Get address of device private structure */

	/* CODE HERE */
	struct rtl8150 *priv = usb_get_intfdata(intf);

	if (priv) {
	
	/**
	  * Unregister and free memory of net_device structure
	  * Call usb_set_intfdata(intf, NULL) to free memory	
	  */

	  /* CODE HERE */
		usb_set_intfdata(intf, NULL);
		kfree(priv);

	}
}

// Table of devices that work with this driver
static const struct usb_device_id rtl8150_table[] = {
	{USB_DEVICE(VID_REALTEK, PID_RTL_8150)},
	{0, }
};

MODULE_DEVICE_TABLE(usb, rtl8150_table);

/**
  * This marks the usb_device_id table in the module image. This information
  * loads the module on demand when the USBcard is inserted into  USB slot.
  * It is part of Module auotload mechanism supported in Linux
  */

static struct usb_driver rtl8150_driver = {
	.name =		DRV_NAME,
	.id_table =	rtl8150_table,
	.probe =	rtl8150_probe,
	.disconnect =	rtl8150_disconnect
};

/************* USB init and exit routines ***************/
static int __init usb_rtl8150_init(void)
{
	/* CODE HERE */
	int res = usb_register(&rtl8150_driver);
	if(res) {
		printk(KERN_ALERT "not able to register the driver with the core\n");
	}
	return 0;
}

static void __exit usb_rtl8150_exit(void)
{
	/* CODE HERE */
	usb_deregister(&rtl8150_driver);
}

module_init(usb_rtl8150_init);
module_exit(usb_rtl8150_exit);

MODULE_AUTHOR("Vikas MANOCHA");
MODULE_DESCRIPTION("USB Driver for Realtek rtl8150 USB Ethernet Wired Card");
MODULE_LICENSE("Dual BSD/GPL");
