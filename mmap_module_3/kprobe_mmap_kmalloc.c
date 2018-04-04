#include <linux/module.h>
#include <linux/init.h>
#include <linux/kprobes.h>

static int my_kprobe_pre(struct file * filp, struct vm_area_struct * vma)
{
	printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
	return 0;
}

static void my_kprobe_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
}

static void my_kprobe_fault(struct kprobe *kp, struct pt_regs *regs, int trapnr)
{
	printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
}

static struct kprobe kp = {
	.pre_handler = my_kprobe_pre,
	.post_handler = my_kprobe_post,
	.fault_handler = my_kprobe_fault,
};

static int my_jprobe_entry(struct inode *inode, struct file *file)
{
	printk(KERN_ALERT "\n\n\ninside %s function\n", __FUNCTION__);
	jprobe_return();
	return 0;
}

static struct jprobe jp = {
	.entry = (kprobe_opcode_t *)my_jprobe_entry,
};

static int __init kprobe_init(void)
{
	printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
	kp.addr =  (kprobe_opcode_t *)kallsyms_lookup_name("mmap_kmalloc");
	jp.kp.addr =  (kprobe_opcode_t *)kallsyms_lookup_name("test_mmap_open");
	if(!kp.addr || !jp.kp.addr) {
		printk(KERN_ALERT "didnt find address to mmap_kmalloc()");
		return -1;
	}
	printk("kp.addr which means mmap_kmalloc() address = %p\n", kp.addr);
	register_kprobe(&kp);
	register_jprobe(&jp);
	return 0;
}

void __exit kprobe_exit(void)
{
	printk(KERN_ALERT "inside %s function\n", __FUNCTION__);
	unregister_kprobe(&kp);
	unregister_kprobe(&jp);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vikas MANOCHA");
module_init(kprobe_init);
module_exit(kprobe_exit);
