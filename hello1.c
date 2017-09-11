
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <asm-generic/bug.h>

#include "inc/hello1.h"

//static void print_hello(void);
//EXPORT_SYMBOL(print_hello);

static void print_hello(void)
{
	pr_emerg("Print Hello.\n");
}

static int __init hello_init(void)
{
	return 0;
}

static void __exit hello_exit(void)
{
	pr_emerg("Moule hello1 exit.\n");
}

module_init(hello_init);
module_exit(hello_exit);


MODULE_AUTHOR("Roman Kolisnyk <kolisnykroman@gmail.com>");
MODULE_DESCRIPTION("Hello, world in Linux Kernel Training");
MODULE_LICENSE("Dual BSD/GPL");