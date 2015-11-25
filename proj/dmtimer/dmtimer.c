#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/sched.h>
#include<linux/fs.h>
#include<linux/device.h>
#include<linux/platform_device.h>
#include <linux/of.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DeathFire");
MODULE_DESCRIPTION("ARCH TIMER");

#define DEVICE_NAME "ARM DMTIMER"
#define TINT7 95
#define DEBUG 0
#if 1

struct dmtimer {

struct platform_device *pdev;
void __iomem * enable_base_addr;
void __iomem *timer7_base;
u32 addr_len;
u32 start,end;
u8 flag;
struct task_struct * my_thread;
} dmtimer_drv;

#endif

static int dmtimer_probe (struct platform_device *);
static int dmtimer_remove (struct platform_device *); 
static int dmtimer_irq_handler (unsigned int irq, void * dev_id, struct pt_regs *reg);

/** Thread function prototype **/

static int read_timer (void * data);



static const struct of_device_id dmtimer_match[]={
	{.compatible = "sk,timer-dev", },
	{},
};

MODULE_DEVICE_TABLE(of, dmtimer_match);

static struct platform_driver dmtimer = {
	.probe = dmtimer_probe,
	.remove = dmtimer_remove,
	.driver = {
			.name = DEVICE_NAME,
			.of_match_table = of_match_ptr(dmtimer_match),
		},
};

static int __init dmtimer_init(void) {

int ret=0;
ret=platform_driver_register(&dmtimer);
return ret;
}

static void __exit dmtimer_exit(void) {

if(dmtimer_drv.my_thread) 
	kthread_stop(dmtimer_drv.my_thread);
	free_irq(95, NULL);
platform_driver_unregister(&dmtimer);
}

static int dmtimer_probe(struct platform_device *pdev) {

	struct resource *r_mem;
	void __iomem * io = NULL;
	int ret=0;

	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if(!r_mem) {
		printk(KERN_ALERT "Cannot find DMTIMER Base address\n");
		return -1;
	}
	dmtimer_drv.start = 0x44E00000; // Required fpr CM_PER TIMER 7 enable
	dmtimer_drv.end =   0X44E003FF;

	pr_emerg( "Start: %x END:%x , len:%x \n", (u32)r_mem->start, (u32)r_mem->end, \
	(u32)r_mem->end-r_mem->start +1);	

	if((request_mem_region(0x44E00000, 0x400 , "CM_PERIPHERAL")) == NULL) {
		printk(KERN_ERR "CM PER Memory region already occupied\n");
		return -EBUSY;
	}
	else 
		printk(KERN_ALERT "CM PER Memory Region available\n");

	if((request_mem_region(0x4804A000, 0x400 , DEVICE_NAME)) == NULL) {
		printk(KERN_ERR "DMTIMER Memory region already occupied\n");
		return -EBUSY;
	}
	else 
		printk(KERN_ALERT "DMIMER Memory Region available\n");


	io = ioremap_nocache(0x44E00000, 0x400);
	dmtimer_drv.timer7_base = ioremap_nocache(0x4804A000, 0x400);
	
	#ifdef DEBUG
	pr_alert("VMEM 1 : %x\n", (unsigned int)dmtimer_drv.timer7_base);
	pr_alert("VMEM 2 : %x\n", (unsigned int)io);
	#endif
	ret = request_irq(TINT7, (irq_handler_t)dmtimer_irq_handler,__IRQF_TIMER, "DMTIMER7_IRQ_HANDLER", NULL);	

	dmtimer_drv.enable_base_addr=io;

	iowrite32(30002, (io + 0x7c)); //enable timer 7 block
	iowrite32(0x02, dmtimer_drv.timer7_base + 0x2c); // enable timer 7 over flow intrpt
	iowrite32(0x0FFF0000, dmtimer_drv.timer7_base + 0x3c); // write init value to tclr reg
	iowrite32(0x0FFF0000, dmtimer_drv.timer7_base + 0x40); // put auto reload value 0x00 in TLDR reg
	iowrite32(0x03, dmtimer_drv.timer7_base + 0x38); // auto reload mode, enable timer 7
	
	//pr_alert("CM_PER REF %x\n", ioread32(dmtimer_drv.enable_base_addr + 0x7c));

	dmtimer_drv.pdev = pdev;
	
	#if 1 
	dmtimer_drv.my_thread = kthread_run( read_timer, (void *)NULL, "TIMER7-Read");
	dmtimer_drv.flag=1;
	#endif

	return 0;
}


/* DMTIMER Remove Function */
static int dmtimer_remove (struct platform_device *pdev) {


printk(KERN_INFO "Removed DMTIMER");
iounmap(dmtimer_drv.enable_base_addr);
iounmap(dmtimer_drv.timer7_base);
release_mem_region(0x44E00000, 0x400);
release_mem_region(0x4804A000, 0x400);
pr_alert("T7 Addr Release %x\n", (unsigned int)dmtimer_drv.timer7_base);

return 0;
}


/* DMTIMER_IRQ_HANDLER */

static int dmtimer_irq_handler (unsigned int irq, void *dev_id, struct pt_regs *regs) {
iowrite32(0x00, dmtimer_drv.timer7_base + 0x30); // disable timer 7 interrupt

//iowrite32(0x00, dmtimer_drv.timer7_base + 0x38); // disable timer 7
iowrite32(0x02, dmtimer_drv.timer7_base + 0x28); // clear timer 7 int pending bit by writing 1
//pr_emerg("Timer 7 OVF INT\n");
iowrite32(0x02, dmtimer_drv.timer7_base + 0x2c); // enable timer 7 over flow intrpt
//iowrite32(0x02, dmtimer_drv.timer7_base + 0x38); // enable timer 7 auto reload mode
return (irq_handler_t) IRQ_HANDLED;
}


/* Kernel Thread Function Implementation to Read Timer value */

static int read_timer (void * data) {


	while(dmtimer_drv.enable_base_addr && dmtimer_drv.timer7_base && dmtimer_drv.flag) {
	if(kthread_should_stop()) 
		break;
	
	pr_alert("TCLR:%x\n", ioread32(dmtimer_drv.timer7_base +0x38));
	pr_alert("TCRR:%x\n", ioread32(dmtimer_drv.timer7_base +0x3C));
	pr_alert("TINT7-EN:%x\n", ioread32(dmtimer_drv.timer7_base +0x2c));
	pr_alert("TINT7-CLR:%x\n", ioread32(dmtimer_drv.timer7_base +0x30));
	pr_alert("TLDR:%x\n", ioread32(dmtimer_drv.timer7_base +0x40));
	pr_alert("TINT STAT:%x\n", ioread32(dmtimer_drv.timer7_base +0x28));
//	pr_alert("TIRQ_EN_SET:%x\n", ioread32(dmtimer_drv.timer7_base +0x30));	
	
	msleep(1000);
	}

return 0;
}


module_init(dmtimer_init);
module_exit(dmtimer_exit);