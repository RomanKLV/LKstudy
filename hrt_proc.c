#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/types.h>

#include <linux/workqueue.h>

MODULE_LICENSE("GPL");


/* HR timer */
#define MS_TO_NS(x)     (x * 1000000L)

static struct hrtimer hr_timer;

static int restart = 5;
static int loop = 1;
static unsigned long delay_in_ms = 200L;

ktime_t ktime;

#define WR_BUF_SIZE 1024
char wr_buf[WR_BUF_SIZE];

/* ProcFS */
struct proc_dir_entry *proc;
int temp;
char *msg;

/* WQ */
static struct workqueue_struct *my_wq;

typedef struct {
	struct delayed_work my_work;
	int x;
} my_work_t;

my_work_t *work;

/* Functions */
static void my_wq_function(struct work_struct *work)
{
	my_work_t *my_work = (my_work_t *)work;

	pr_info("my_work.x %d called (%lld)\n",
my_work->x, ktime_to_ms(hr_timer.base->get_time()));

	my_work->x = my_work->x + 1;
//	kfree((void *)work);

}

void WQ_add(void)
{
	int ret;

	/* Queue some work (item 1) */
	work = (my_work_t *)kmalloc(sizeof(my_work_t), GFP_KERNEL);

	if (work) {
		INIT_DELAYED_WORK((struct delayed_work *)work, my_wq_function);
		pr_info("worker exec sheduled %lld.\n",
ktime_to_ms(hr_timer.base->get_time()));

		ret = schedule_delayed_work((struct delayed_work *)work, delay_in_ms);
	}
}

void WQ_clean(void)
{
	flush_workqueue(my_wq);
	destroy_workqueue(my_wq);
}

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
	restart--;

	pr_info("%s called %lld\n",
 __func__, ktime_to_ms(timer->base->get_time()));

	if (restart > 0) {
		hrtimer_forward_now(timer, ns_to_ktime(MS_TO_NS(delay_in_ms)));
		return HRTIMER_RESTART;

	} else { /* for fix restart timer with max times repeat */
		restart = 0;
		return HRTIMER_NORESTART;
	}

}

/* ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);*/
ssize_t write_proc(struct file *filp, const char __user *buf,
size_t count, loff_t *offp)
{
	char *data;

	data = PDE_DATA(file_inode(filp));

	if (!(data)) {
		pr_info("Null data");
		return 0;
	}

	if (count > temp)
		count = temp;

	temp = temp - count;

	memset(data, 0, WR_BUF_SIZE);
	if (copy_from_user(data, buf, count)) {
		pr_info("Data copy error\n");
		return -EFAULT;
	}

	if (count == 0)
		temp = WR_BUF_SIZE;

	/* write from buffer to variable */
	sscanf(wr_buf, "%lu %d", &delay_in_ms, &loop);

	/* debug output */
	pr_info("delay = %ld ; loop = %d\n", delay_in_ms, loop);

	return count;
}

ssize_t read_proc(struct file *filp, char *buf, size_t count, loff_t *offp)
{
	char *data;

	pr_info("--- Start %s ---\n", __func__);

	data = PDE_DATA(file_inode(filp));
	if (!(data)) {
		pr_info("Null data");
		return 0;
	}

	if (count > temp)
		count = temp;

	temp = temp - count;

	if (copy_to_user(buf, data, count)) {
		pr_info("Data copy error\n");
		return -EFAULT;
	}

	if (count == 0)
		temp = WR_BUF_SIZE;

	restart = loop;
	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

	pr_info("delay = %ld ; loop = %d\n", delay_in_ms, loop);

	WQ_add();

	pr_info("Starting timer to fire in %lld ms (%ld)\n",
ktime_to_ms(hr_timer.base->get_time()) + delay_in_ms, jiffies);

	if (hrtimer_active(&hr_timer)) {
		;  // <-----delay
	};


	pr_info("--- End %s ---\n", __func__);

	return count;
}

struct file_operations proc_fops = {
	.read = read_proc,
	.write = write_proc,
};


void create_new_proc_entry(void)
{
	pr_info("--- Start %s ---\n", __func__);
	sprintf(wr_buf, "Delay = %lu ; loop = %d\n", delay_in_ms, restart);
	pr_info("Delay = %lu ; loop = %d\n", delay_in_ms, restart);

	msg = wr_buf;

	proc = proc_create_data("hrt", 0000, NULL, &proc_fops, msg);

	temp = WR_BUF_SIZE;

	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer.function = &my_hrtimer_callback;

	pr_info("Starting timer %lld ms\n",
ktime_to_ms(hr_timer.base->get_time()) + delay_in_ms);

	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

	pr_info("--- End %s ---\n", __func__);
}

int proc_init(void)
{

	create_new_proc_entry();
	pr_info("HR Timer module installing\n");
	my_wq = create_workqueue("my_queue");

	pr_info("start wq\n");


//	if (!my_wq)
//		return -1;

	pr_info("add to wq\n");
	WQ_add();


//	WQ_init();

	return 0;
}

void proc_cleanup(void)
{
	int ret;

	ret = hrtimer_cancel(&hr_timer);
	if (ret)
		pr_info("The timer was still in use...\n");

	remove_proc_entry("hrt", NULL);

	WQ_clean();
	pr_info("HR Timer module uninstalling\n");
}

module_init(proc_init);
module_exit(proc_cleanup);

