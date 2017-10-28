#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_device.h>



#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
*	1) 4K of memory at address defined by dts - used for data transfer;
*	2) Two 32-bit registers at address (defined by dts)
*	2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
*2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
*/

#define MEM_SIZE	(4096)
#define REG_SIZE	(8)
#define DEVICE_POOLING_TIME_MS (500) /*500 ms*/
/**/
#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */
#define PLAT_IO_DATA_WR	(2) /*data writen to dev */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */


struct plat_dummy_device {
	void __iomem *mem;
	void __iomem *regs;
	struct delayed_work     dwork;
	struct workqueue_struct *data_read_wq;
	struct workqueue_struct *data_write_wq;
	struct delayed_work     dwork2;
	u64 js_pool_time;
	u8 msg[64]; /* for message witch been writen to dev */
};

static u32 plat_dummy_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->mem + offset);
}

static u32 plat_dummy_reg_read32(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs + offset);
}

static void plat_dummy_reg_write32(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs + offset);
}
/* for write message in dev by bytes */
static void plat_dummy_mem_write8(struct plat_dummy_device *my_dev, u32 offset, u8 val)
{
	iowrite8(val, my_dev->mem + offset);
}

static void plat_dummy_work(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status;
	u8 data;

	pr_info("++%s(%u)\n", __func__, jiffies_to_msecs(jiffies));

	my_device = container_of(work, struct plat_dummy_device, dwork.work);
	status = plat_dummy_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_IO_DATA_READY) {
		size = plat_dummy_reg_read32(my_device, PLAT_IO_SIZE_REG);
		pr_info("%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		for (i = 0; i < size; i++) {
			data = plat_dummy_mem_read8(my_device, i);
			pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__,  i, data, data);
		}
		/* for complite all reads */
		rmb();
		status &= ~PLAT_IO_DATA_READY;
		plat_dummy_reg_write32(my_device, PLAT_IO_FLAG_REG, status);

	}
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork, my_device->js_pool_time);
}

/* function for write message in dev */
static void plat_dummy_work_wr2(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status;
	u8 data;

	pr_info("--%s(%x)\n", __func__, jiffies_to_msecs(jiffies));
	my_device = container_of(work, struct plat_dummy_device, dwork2.work);

	size = 0;
	status = plat_dummy_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (~(status & PLAT_IO_DATA_READY)) {

		size = sprintf((char *)my_device->msg, "%u ms\n", jiffies_to_msecs(jiffies));
		pr_info("%s jiffies = %s\n", __func__, (char *)my_device->msg);

		if (size < 0)
			pr_info("Some error in %s", __func__);

		for (i = 0; i < size; i++) {
			data = my_device->msg[i];

			plat_dummy_mem_write8(my_device, i, data);
			//pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__,  i, data, data);
		}
		/* for complite all writes */
		wmb();

		status = PLAT_IO_DATA_WR;
		plat_dummy_reg_write32(my_device, PLAT_IO_SIZE_REG, size);
		plat_dummy_reg_write32(my_device, PLAT_IO_FLAG_REG, status);

	}

	queue_delayed_work(my_device->data_write_wq, &my_device->dwork2, my_device->js_pool_time);
}
/*
static void plat_dummy_work_wr(struct plat_dummy_device *my_device)
{
	u32 i, size, status;
	u8 data;

	pr_info("--%s(%d)\n", __func__, jiffies_to_msecs(jiffies));

	size = 0;
	status = plat_dummy_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (~(status & PLAT_IO_DATA_READY)) {
		size = sprintf((char *)my_device->msg, "Driver_Ready");

		if (size < 0)
			pr_info("Some error in %s", __func__);

		for (i = 0; i < size; i++) {
			data = my_device->msg[i];

			plat_dummy_mem_write8(my_device, i, data);
			pr_info("%s: mem[%d] = 0x%x ('%c')\n", __func__,  i, data, data);
		}

		wmb();

		status = PLAT_IO_DATA_WR;
		plat_dummy_reg_write32(my_device, PLAT_IO_SIZE_REG, size);
		plat_dummy_reg_write32(my_device, PLAT_IO_FLAG_REG, status);
	}
	pr_info("%s exit\n", __func__);
}
*/
static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "test,plat_dummy",
	}, {
	},
};

static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	pr_info("++%s\n", __func__);

	if (!np) {
		pr_info("No device node found!\n");
		return -ENOMEM;
	}
	pr_info("Device name: %s\n", np->name);
	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("res 0 = %llx..%llx\n", res->start, res->end);


	my_device->mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->mem))
		return PTR_ERR(my_device->mem);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("res 1 = %llx..%llx\n", res->start, res->end);

	my_device->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs))
		return PTR_ERR(my_device->regs);

	platform_set_drvdata(pdev, my_device);

	pr_info("Memory mapped to %p\n", my_device->regs);
	pr_info("Registers mapped to %p\n", my_device->mem);


//	plat_dummy_work_wr(my_device);

	/*Init data read WQ*/
	my_device->data_read_wq = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_read_wq)
		return -ENOMEM;

	my_device->data_write_wq = alloc_workqueue("plat_dummy_write",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_write_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork, plat_dummy_work);
	INIT_DELAYED_WORK(&my_device->dwork2, plat_dummy_work_wr2);

	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);

	queue_delayed_work(my_device->data_read_wq, &my_device->dwork, 0);
	queue_delayed_work(my_device->data_write_wq, &my_device->dwork2, 0);

	return PTR_ERR_OR_ZERO(my_device->mem);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s\n", __func__);

	if (my_device->data_write_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork2);
		destroy_workqueue(my_device->data_write_wq);
	}

	if (my_device->data_read_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork);
		destroy_workqueue(my_device->data_read_wq);
	}
	return 0;
}

static struct platform_driver plat_dummy_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = plat_dummy_of_match,
	},
	.probe		= plat_dummy_probe,
	.remove		= plat_dummy_remove,
};

MODULE_DEVICE_TABLE(of, plat_dummy_of_match);

module_platform_driver(plat_dummy_driver);

MODULE_AUTHOR("Vitaliy Vasylskyy <vitaliy.vasylskyy@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");
