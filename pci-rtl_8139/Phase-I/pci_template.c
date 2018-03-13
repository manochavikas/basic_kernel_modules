/* 
 * Realtek RTL8139 PCI Ethernet Wired card Driver template.
 * 
 * Implement PCI portion of rtl8139 Network Card driver
 * Use this as a template. Add code in areas matching string "CODE HERE".  
 * In this phase of the project we will be writing PCI routines. 
 * Net Device and ISR routines will be implemented in PHASE 2.
 * Compile the driver as module. Use the makefile provided earlier
 * Unload the production module or blacklist it: 
 * # lspci -v  -- This will show any module or driver bound to this card
 * # rmmod 8139too -- Unload the production driver
 * # lsmod - list loaded modules
 * Load the driver after adding required code: 
 * # insmod pci.ko
 * Run "ifconfig -a", You should see: 
 * - MAC Address read from the device memory
 * - IRQ number read from the device memory
 * - Device IO memory base address read from the device memory
 *
 * Guidelines are provided to assist you with writing the pci portion of the 
 * driver. Do not limit yourself to it. You are encouraged to review source 
 * code of production driver. 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/cache.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/uaccess.h>


#define DRV_NAME "LDDA_PCI"  /* Use it to change name of interface from eth */

/**
  * PCI DEVICE REGISTRATION.  
  * pci_device_id describes to the PCI layer the devices that this driver can handle
  * PCI_ANY_ID means anything matches (Wild card), PCI layer automatically
  * calls your probe function for any matching device. 
  * You can find the device and vendor ID using lspci -n
  * Array is zero-terminated
  */

static struct pci_device_id rtl8139_table[] __devinitdata = {
        { /* CODE HERE */ },
        { 0, }
};

/** 
  * This marks the pci_device_id table in the module image. This 
  * information loads the module on demand when the PCI card is 
  * hot plugged into the PCMCIA slot. It is part of module autoload 
  * mechanism supported as part of Linux Device Model 
  *
  * Production ready Network modules are located in direcotory:
  * /lib/modules/2.6.../kernel/drivers/net
  * rtl8139 production driver: 8139cp.ko, 8139too.ko
  *
  * File where driver and device mapping is defined: 
  * /lib/modules/2.6../modules.pcimap
  * 
  * Entry for rtl8139 production driver: 
  * 
  *   pci module         vendor     device     subvendor  subdevice .. 
  *    8139cp            0x000010ec 0x00008139 0xffffffff 0xffffffff ..
  *    8139too           0x000010ec 0x00008139 0xffffffff 0xffffffff ..
  * 
  * module.alias file identifies what driver to use for the device. 
  * File is generated by depmod utility: 
  * alias pci:v000010ECd00008139sv*sd*bc*sc*i* 8139cp
  * alias pci:v000010ECd00008129sv*sd*bc*sc*i* 8139too
  *
  * modules.dep file - Module dependencies and tells where the 
  * required binary is located 
  * rtl8139 production driver:  8139cp.ko and 8139too.ko 
  * depend on mii.ko module
  * kernel/drivers/net/8139cp.ko: kernel/drivers/net/mii.ko
  * kernel/drivers/net/8139too.ko: kernel/drivers/net/mii.ko
  * 
  * See discussion in lecture: Linux Device Model - Module 9
  */

MODULE_DEVICE_TABLE(pci, rtl8139_table);


/**
  * rtl8139 private structure for keeping device specific 
  * information. You need to puplate it for later reference.
  */

struct rtl8139
{
	struct pci_dev *pci_dev;  	/* PCI device */
        void *mmio_addr; 		/* memory mapped I/O addr */
        unsigned long regs_len; 	/* length of IOMEM region */
        struct net_device_stats stats;  /* Net device stats */
	spinlock_t lock;  		/* Spin lock */
	
        /* Add rtl8139 device specific stuff later */
};

static int __devinit 
rtl8139_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void __devexit rtl8139_remove( struct pci_dev *pdev );
static int rtl8139_open(struct net_device *dev);
static int rtl8139_stop(struct net_device *dev);
static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats* rtl8139_get_stats(struct net_device *dev);

/** 
  * PCI driver hooks and supported devices table 
  * pci_register_driver and pci_unregister_driver use struct pci_driver
  * as arguments to register and unregister the pci driver.
  */

static struct pci_driver rtl8139_driver = {
        .name           = DRV_NAME,
        .id_table       = rtl8139_table,
        .probe          = rtl8139_probe,
 	.remove         = __devexit_p (rtl8139_remove),

	/* We won't be implementing PCI suspend and resume routines  */
      //.suspent	= rtl8139_suspend,  
      //.resume		= rtl8139_resume, 
};

/**
  *  2.6.29 and above: 
  *  netdevice functions are moved out from netdevice structure 
  *  into net_device_ops 
  */

#ifdef HAVE_NET_DEVICE_OPS
static struct net_device_ops rtl8139_netdev_ops = {
        .ndo_open               = rtl8139_open,
        .ndo_stop               = rtl8139_stop,
        .ndo_get_stats          = rtl8139_get_stats,
        .ndo_start_xmit         = rtl8139_start_xmit
};
#endif

/***************** PCI ROUTINES*********************/
/**
  * 1- Enable the device
  * 2- Extract the physical address where the device IO memory 
  *    is mapped from the config space 
  * 3- Claim the device IO memory region. It takes start address and 
  *    length of IOMEM region
  * 4- Remap the device IO Memory region. Routine takes start  and length 
  *    of IOMEM region
  * 5- Enable DMA processing engine
  * 6- Allocate net_device structure of type ether
  * 7- sysfs hooks 
  * 8- save IO memory region address into the device private and 
  *    netdevice struct 
  */

static int __devinit rtl8139_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *dev;
        struct rtl8139 *priv;
	int i;
	unsigned long mmio_start, mmio_end, mmio_len, mmio_flags;
        void *ioaddr;
	
	/* Enable the device first. This wakes up the device if suspended. */
	
        /* CODE HERE */

        /** 
	  * Enable bus mastering of the device. This will set bus master bit 
	  * in the PCI_COMMAND register. Device now can act as a master on 
	  * the address bus 
	  */

         /* CODE HERE */ 

	/**
	  * PCI has API to access PCI configuration space such as:
	  *   - pci_resource_start
          *   - pci_resource_end
          *   - pci_resource_len  
          *   - pci_resource_flag
	  *
          * These routines can help you to extract physical address 
          * where device MMIO region mapped: start, end, length of the 
          * region and flag. Flag provides a hint to driver about the 
          * resource being cachable or not. 
	  * Start and size of device MMIO region is required in 
          * order for ioremap() to create page table entries 
          * (virtual addresses) to access device IOMEM area.
	  * 
	  * Once the device memory is mapped, drivers can read and 
          * write to PCI  device's  MMIO region using bus independent 
	  * IO API read(b|w|l), write(b|w|l) or wrapper routines: 
	  * ioread/iowrite.
          * PCI BAR: PCI BASE ADDRESS REGISTER.
          * Where: BAR 0 is IOAR, BAR 1 is MEMAR. 
          * Since we will be using memory-mapped I/O (MMIO), we will pass 
	  * the second  argument as 1 to pci routines mentioned above.  
	  */

	  /* CODE HERE */

        /**
	  * Test if pci BAR 1 is really device MMIO region 
	  * otherwise, goto disable the device
	  */

	/* CODE HERE */
        
        /* claim or take ownership of the IO Memory region. If fail goto 
 	 * disable the device
 	 * */

	/* CODE HERE */

        /** 
	  * ioremap Device MMIO region: ioremap (address, size) function must 
	  * be called to map the device memory into a virtual memory address. 
	  * This function does not allocate any memory, it sets up the 
	  * page tables. Very simply, it adds the needed entries into the page 
	  * tables for device MMIO range (size). If you forget to do it and 
	  * access the memory, then MMU will indicate a request for an invalid 
	  * memory address, causing an invalid page fault and kernel may panic.
	  * Once the memory is remapped with ioremap, a bit of care should be 
	  * taken and avoid the temptation of de-referencing the address.
          * Use ioread/iowrite|b|w|l() instead.
          * goto release in case of ioremap() failure to provide valid 
	  * virtual address
          */

	 /* CODE HERE */

	/**
          * Check if 32-bit DMA capability is supported on this platform
          * Use for declaring any device with more (or less) than 32-bit 
          * bus master capability
	  * goto unmap if unable to set DMA 32 bit mask
          */

	/* CODE HERE/*

	/** 
	  * Linux Network Stack works with network device not the PCI device. 
	  * We need to allocate an ethernet network device. alloc_etherdev() 
	  * allocates net device structure with memory allocated for 
	  * device private structure. You can access rtl839 private struct 
	  * by using netdev_priv(dev). unmap, in case of failure
	  */

	 /* CODE HERE */

	/** 
 	 * Set the device name to DRV_NAME instead of eth via memcpy 
 	 */ 

	/* CODE HERE */

 	/* sysfs stuff. Sets up device link in /sys/class/net/interface_name */
	SET_NETDEV_DEV(dev, &pdev->dev);

	/**
	  *  Set up information in the device private structure such as 
	  *  mmio_addr, regs_len, pci_dev and initialize spinlock
	  */

	/* CODE HERE */

	/**
	  * Fill in the net device with MMIO address and irq obtained 
	  * from the PCI configuration space 
          * These fields are base_addr and irq field in net_device structure. 
	  * Ifconfig reads these values from net device structure and print it
	  * PCI device gets IRQ assigned automatically - No poking, probing 
	  * guessing needed. 
	  * 
          * Assigned IRQ number is placed in pci pdev structure (pdev->irq) 
          * passed as argument to PCI device probe.
          */

	/* CODE HERE */

	/**
          * You can stuff net device structure into pci_driver structure
          * using pci_set_drvdata( struct pci_driver *, void *)  That can 
          * be retrieved later using pci_get_drvdata(struct pci_driver *)
          * for example, in remove or other pci functions
          */

	/* CODE HERE */	

	/**
          * Interface address: MAC and Broadcast Address
          * RealTek8139 datasheet states that the first 6 bytes of ioaddr 
          * (offset 0x0) contain the hardware address of the device. We need to 
          * fill the net device with MAC and broadcast address. We need to 
          * fill net device dev->dev_addr[6] and dev->broadcast[6] arrays.
          * For broadcast address fill all octets with 0xff. Use readb (or 
          * ioreadb) to read from IO memory address, ioaddr
	  */

	/* CODE HERE */

        /* Length of Ethernet frame. It is a "hardware header length", number 
 	 * of octets that lead the transmitted packet before IP header, or 
 	 * other protocol information.  Value is 14 for Ethernet interfaces.
         */

        dev->hard_header_len = 14;

        /** 
	 *  fill in the net device with our device methods that we will write 
	 *  for our driver in phase 2. 
	 *  Required methods are: open, stop, hard_start_xmit, getstats
         *  2.6.29 and above: netdev_ops in netdevice structure points to 
	 *  net_device_ops containing netdevice methods
	 */

#ifdef HAVE_NET_DEVICE_OPS
	dev->netdev_ops = &rtl8139_netdev_ops;
#else
	dev->open = rtl8139_open;
	dev->stop = rtl8139_stop;
	dev->hard_start_xmit = rtl8139_start_xmit;
	dev->get_stats = rtl8139_get_stats;
#endif

	/* Finally register the net device. An unused ethernet interface 
         * is alloted
         */

        /* CODE HERE */

        return 0;

	/**
	 * cleanup on failure. goto is a better way to deal with serious 
	 * error conditions
	 */

freedev:
	free_netdev(dev);

unmap:
	pci_iounmap(pdev, ioaddr);

release:
	pci_release_regions(pdev);	

disable:
	pci_disable_device(pdev);
	return (-ENODEV);
}


/**************** Net device routines ******************************/

static int rtl8139_open(struct net_device *dev) 
{ 
	printk("rtl8139_open: Add code later\n"); 
	netif_start_queue(dev); /* transmission queue start */
	return 0; 
}

static int rtl8139_stop(struct net_device *dev) 
{
        printk("rtl8139_stop: Add code later \n");
	netif_stop_queue(dev); /* transmission queue stop */
        return 0;
}

static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *dev) 
{
        printk("rtl8139_start_xmit: Add code later\n");
	dev_kfree_skb(skb); /* Just free it for now */

        return 0;
}

static struct net_device_stats * rtl8139_get_stats(struct net_device *dev) 
{
        printk("rtl8139_get_stats: Add code later\n");

	/**
	 * You cannot return NULL, make sure to return the address 
	 * of net_dev_stat that is in device private structure
	 */
	/* CODE HERE */
}

/* PCI remove routine - required else can't rmmod */
static void __devexit rtl8139_remove( struct pci_dev *pdev )
{
   struct net_device *dev;
   struct rtl8139 *priv;
   /**
     * Get address of netdevice, device private structures and ioaddr 
     * Unregister and free netdevice
     * Unmap the device MMIO region. Also set: priv->mmio_addr = NULL 
     * Release the ownership of IO memory region
     * call: pci_set_drvdata(pdev, NULL)
     * Disable PCI device
     */

	/* CODE HERE */
}

/**************** PCI init and exit routines ***********************/
static int __init pci_rtl8139_init(void)
{
	/* CODE HERE */
}

static void __exit pci_rtl8139_exit(void)
{
	/* CODE HERE */
}

module_init(pci_rtl8139_init);
module_exit(pci_rtl8139_exit);

MODULE_AUTHOR(" ");
MODULE_DESCRIPTION("PCI Driver for Realtek rtl8139 PCI Ethernet Wired card");
MODULE_LICENSE("Dual BSD/GPL");