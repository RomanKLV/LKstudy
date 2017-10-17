#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roman Kolisnyk <kolisnykroman@gmail.com");
MODULE_DESCRIPTION("Simple kmem_cache test module");

struct kmem_cache *test13_cachep;

struct b13 {
	char data[13];
};

struct b13 *testa, *testb;

struct b13 charset = {
	.data[0] = '0',
	.data[1] = '1',
	.data[2] = '2',
	.data[3] = '3',
	.data[4] = '4',
	.data[5] = '5',
	.data[6] = '6',
	.data[7] = '7',
	.data[8] = '8',
	.data[9] = '9',
	.data[10] = 'A',
	.data[11] = 'B',
	.data[12] = 0x0,
};

/* constructor for kmem_cache */
void b13_alloc(void *instruct)
{
	struct b13 *tmp = instruct;

	*tmp = charset;
};

int init_module(void)
{
	pr_info("Load kmem_cache test module\n");

	test13_cachep = kmem_cache_create("test_13bytes", sizeof(struct b13), 0, SLAB_CACHE_DMA, (void *)&b13_alloc);

	pr_info("Cache object size is %d\n", kmem_cache_size(test13_cachep));

	testa = kmem_cache_alloc(test13_cachep, GFP_KERNEL);
	if (testa)
		pr_info("testa allocated\n");
	else
		return NULL;

	testb = kmem_cache_alloc(test13_cachep, GFP_KERNEL);
	if (testb)
		pr_info("testb allocated\n");
	else
		return NULL;

/* insert debug print addres of struct   */
	*testa = charset;
	pr_info("testa addr:%ul :%s\n", testa, testa->data);

	pr_info("testb addr:%ul :%s\n", testb, testb->data);

	return 0;
}

void cleanup_module(void)
{
	pr_info("start unloading kmem_cache test module\n");

	if (testa) {
		kmem_cache_free(test13_cachep, testa);
		pr_info("testa deallocated\n");
	}

	if (testb) {
		kmem_cache_free(test13_cachep, testb);
		pr_info("testb deallocated\n");
	}

	kmem_cache_destroy(test13_cachep);

	pr_info("Unloaded kmem_cache test module\n");
}
