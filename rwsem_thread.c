#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/rwsem.h>
#include <asm/barrier.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Kolisnyk <kolisnykroman@gmail.com");
MODULE_DESCRIPTION("Simple threaded counter");

static struct hrtimer hr_timer;

#define RNUM 5
#define WNUM 1

static struct task_struct *rthreads[RNUM];
static struct task_struct *wthreads[WNUM];

static struct rwdata{
	ulong counter;
} mydata;

DECLARE_RWSEM(mysm);

static char *sj(void)
{	// time label
	static char s[40];

	sprintf(s, "%12ld ns:", (ulong)(hr_timer.base->get_time()));
	return s;
}

static char *st(void)
{	// kthread label
	static char s[40];

	sprintf(s, "%s kthread [%05d]", sj(), current->pid);
	return s;
}

int counter_wr(void *data)
{
	struct rwdata *tmp;

	tmp = (struct rwdata *)data;
	while (!kthread_should_stop()) {
		down_write(&mysm);
		tmp->counter++;
		up_write(&mysm);
	}

	pr_info("The counter writer thread %s has terminated\n", st());
	return tmp->counter;
}


int counter_rd(void *data)
{
	ulong i;
	struct rwdata *tmp;

	i = 0;
	tmp = (struct rwdata *)data;

	while (!kthread_should_stop()) {
		down_read(&mysm);

		i = tmp->counter;
		pr_info("-> %s; read: %lu", st(), i);
		ndelay(5000);// max 5000
		up_read(&mysm);
	}

	pr_info("The counter reader thread %s has terminated\n", st());
	return tmp->counter;
}

void mb_test(void)
{
	ulong x;

	rmb();/* test*/
	mydata.counter = 0;
	wmb();/* test*/
	x = mydata.counter;
}

static int test_thread(void)
{
	int i;
	int ret;

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	/* start kthreads */
	pr_info("all start at %s\n", sj());

	for (i = 0; i < WNUM; i++) {
		wthreads[i] = kthread_run(counter_wr, &mydata, "counter_wr_%i", i);
		if (IS_ERR(wthreads[i])) {
			pr_info("ERROR! wthreads[%i] run\n", i);
			return PTR_ERR(wthreads[i]);
		}
	}

	for (i = 0; i < RNUM; i++) {
		rthreads[i] = kthread_run(counter_rd, &mydata, "counter_rd_%i", i);
		if (IS_ERR(rthreads[i])) {
			pr_info("ERROR! rthreads[%i] run\n", i);
			return PTR_ERR(rthreads[i]);
		}
	}

/* wait before stop threads */
	msleep(50);

	/* stop kthreads */

	for (i = 0; i < RNUM; i++) {
		ret = kthread_stop(rthreads[i]);

		if (ret != -EINTR)
			pr_info("readers [%i] threads has stopped\n", i);

		ret = 0;
	}

	for (i = 0; i < WNUM; i++) {
		ret = kthread_stop(wthreads[i]);

		if (ret != -EINTR)
			pr_info("writers [%i] threads has stopped\n", i);

		ret = 0;
	}

	pr_info("all stoped at %s\n", sj());
	return 0;
}

static void counter_exit(void)
{
	pr_info("Exit from module\n");
}

module_init(test_thread);
module_exit(counter_exit);
