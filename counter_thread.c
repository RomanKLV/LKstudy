#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/atomic.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Kolisnyk <kolisnykroman@gmail.com");
MODULE_DESCRIPTION("Simple threaded counter");

#define ENTRY_NAME "counter"
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;
static struct task_struct *kthread1, *kthread2;
static struct task_struct *kthreada1, *kthreada2;
static int counter;  // counter without lock
static int counter2;  /// counter with lock
static char *message;
static int read_p;
static atomic_t ll = ATOMIC_INIT(0);  // var for lock (atomic operation)

/* Lock/Unlock and counter with them */
void lock(atomic_t *m)
{
	while (atomic_xchg(m, 1)) {
		;
	};
}

void unlock(atomic_t *m)
{
	// *m = 0;
	atomic_xchg(m, 0);
}

int counter_runa(void *data)
{
	int i = 0;

	for (i; i < 1000000; i++) {
		ndelay(1);
		lock(&ll);
		counter2++;
		unlock(&ll);
	}

	while (!kthread_should_stop())
		ssleep(1);

	pr_info("The counter thread has terminated\n");
	return counter;
}

/* ---------------- */

int counter_run(void *data)
{
	int i = 0;

	for (i; i < 1000000; i++) {
		ndelay(1);
		counter++;
	}

	while (!kthread_should_stop())
		ssleep(1);

	pr_info("The counter thread has terminated\n");
	return counter;
}
int counter_proc_open(struct inode *sp_inode, struct file *sp_file)
{
	pr_info("proc called open\n");

	read_p = 1;
	message = kmalloc(sizeof(char) * 20, __GFP_IO | __GFP_FS);
	if (message == NULL) {
		pr_info("ERROR, counter_proc_open");
		return -ENOMEM;
	}
	sprintf(message, "counter = %d and with lock counter2 = %d\n", counter, counter2);
	return 0;
}

ssize_t counter_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset)
{
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	pr_info("proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int counter_proc_release(struct inode *sp_inode, struct file *sp_file)
{
	pr_info("proc called release\n");
	kfree(message);
	return 0;
}

static int counter_init(void)
{

	pr_info("/proc/%s create\n", ENTRY_NAME);

	kthread1 = kthread_run(counter_run, NULL, "counter");
	if (IS_ERR(kthread1)) {
		pr_info("ERROR! kthread_run\n");
		return PTR_ERR(kthread1);
	}

	kthread2 = kthread_run(counter_run, NULL, "counter");
	if (IS_ERR(kthread2)) {
		pr_info("ERROR! kthread_run\n");
		return PTR_ERR(kthread2);
	}

/* thread with lock/unlock */
	kthreada1 = kthread_run(counter_runa, NULL, "countera");
	if (IS_ERR(kthreada1)) {
		pr_info("ERROR! kthread_run\n");
		return PTR_ERR(kthreada1);
	}

	kthreada2 = kthread_run(counter_runa, NULL, "countera");
	if (IS_ERR(kthreada2)) {
		pr_info("ERROR! kthread_run\n");
		return PTR_ERR(kthreada2);
	}


	fops.open = counter_proc_open;
	fops.read = counter_proc_read;
	fops.release = counter_proc_release;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		pr_info("ERROR! proc_create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}
module_init(counter_init);

static void counter_exit(void)
{
	int ret = kthread_stop(kthread1);

	if (ret != -EINTR)
		pr_info("Counter thread has stopped\n");

	ret = 0;
	ret = kthread_stop(kthread2);
	if (ret != -EINTR)
		pr_info("Counter thread has stopped\n");

	ret = 0;
	ret = kthread_stop(kthreada1);
	if (ret != -EINTR)
		pr_info("Counter thread has stopped\n");

	ret = 0;
	ret = kthread_stop(kthreada2);
	if (ret != -EINTR)
		pr_info("Counter thread has stopped\n");

	remove_proc_entry(ENTRY_NAME, NULL);

	pr_info("Removing /proc/%s.\n", ENTRY_NAME);
}
module_exit(counter_exit);
