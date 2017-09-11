#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <asm-generic/bug.h>
#include <linux/types.h>
#include <linux/kallsyms.h>

static uint hello_counter = 1;

module_param(hello_counter, uint, 0644);

MODULE_PARM_DESC(hello_counter, "Counter for print Hello string");

extern void print_hello(void);
extern void * __kmalloc(size_t size, gfp_t flags);

static void print_goodbye(void)
{
	pr_emerg("Good bye!\n");
}

static int __init hello_init(void)
{
	int x = hello_counter;
	WARN_ON( x==0 );
	BUG_ON( x >= 10);
	if (x==5) return -EINVAL;

	//Disable unload, increment counter
	if (x==2) try_module_get(THIS_MODULE);

	while (x>0)
	{
	print_hello();
	x--;
	} ;

	return 0;
}

static void __exit hello_exit(void)
{
//-----------------------------------------------------
// Allocate memmory and write some value
	u8* someptr;
	someptr = (u8*)__kmalloc( sizeof(u8), GFP_KERNEL);
	*someptr=0x90;

// Get address of __kmalloc function and try write some
// value on that address.
// We have memmory exeption (memmory protect/Read only)
//	someptr= (u8*)kallsyms_lookup_name("__kmalloc");
//	*someptr=0x90;
//-----------------------------------------------------
	print_goodbye();
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Roman Kolisnyk <kolisnykroman@gmail.com>");
MODULE_DESCRIPTION("Hello, world in Linux Kernel Training");
MODULE_LICENSE("Dual BSD/GPL");