#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/typecheck.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/rbtree.h>

#include <linux/rbtree_augmented.h>

MODULE_AUTHOR("Roman Kolisnyk <KolisnykRoman@gmail.com>");
MODULE_DESCRIPTION("Test (rbtree intervals) in Linux Kernel Training");
MODULE_LICENSE("GPL");

/*---------------------------
interval_tree_first_match() - for search in tree
interval_tree_remove2() - for remove intervals from tree
interval_tree_insert2() - for insertion interva in tree
flush_tree() - for clean tree
------------------------------*/

/* Define rbtree root node globally */
static struct rb_root root = RB_ROOT;

struct interval_tree_node {
	struct rb_node rb;
	u32 start;
	u32 last;
	u32 __subtree_last;
};

static void print_tree_intervals(struct rb_root *root)
{
	struct interval_tree_node *val, *tmp;

	rbtree_postorder_for_each_entry_safe(val, tmp, root, rb) {
		pr_info("[ %i ... %i ]\n", val->start, val->last);
	}
}

struct interval_tree_node *interval_tree_first_match(struct rb_root *root, u32 start, u32 last)
{
	struct interval_tree_node *node;

	if (!root->rb_node)
		return NULL;
	node = rb_entry(root->rb_node, struct interval_tree_node, rb);

	while (true) {
		if (node->rb.rb_left) {
			struct interval_tree_node *left =
				rb_entry(node->rb.rb_left,
					 struct interval_tree_node, rb);
			if (left->__subtree_last >= start) {
				/*
				 * Some nodes in left subtree satisfy Cond2.
				 * Iterate to find the leftmost such node N.
				 * If it also satisfies Cond1, that's the match
				 * we are looking for. Otherwise, there is no
				 * matching interval as nodes to the right of N
				 * can't satisfy Cond1 either.
				 */
				node = left;
				continue;
			}
		}
		if (node->start <= last) {		/* Cond1 */
			if (node->last >= start)	/* Cond2 */
				return node;	/* node is leftmost match */
			if (node->rb.rb_right) {
				node = rb_entry(node->rb.rb_right,
					struct interval_tree_node, rb);
				if (node->__subtree_last >= start)
					continue;
			}
		}
		return NULL;	/* No match */
	}
}
EXPORT_SYMBOL(interval_tree_first_match);

//Insertion/removal are defined using the following augmented callbacks::

static inline u32 compute_subtree_last(struct interval_tree_node *node)
{
	u32 max = node->last, subtree_last;

	if (node->rb.rb_left) {
		subtree_last = rb_entry(node->rb.rb_left,
			struct interval_tree_node, rb)->__subtree_last;
		if (max < subtree_last)
			max = subtree_last;
	}
	if (node->rb.rb_right) {
		subtree_last = rb_entry(node->rb.rb_right,
			struct interval_tree_node, rb)->__subtree_last;
		if (max < subtree_last)
			max = subtree_last;
	}
	return max;
}

static void augment_propagate(struct rb_node *rb, struct rb_node *stop)
{
	while (rb != stop) {
		struct interval_tree_node *node =
			rb_entry(rb, struct interval_tree_node, rb);

		u32 subtree_last = compute_subtree_last(node);

		if (node->__subtree_last == subtree_last)
			break;
		node->__subtree_last = subtree_last;
		rb = rb_parent(&node->rb);
	}
}

static void augment_copy(struct rb_node *rb_old, struct rb_node *rb_new)
{
	struct interval_tree_node *old =
		rb_entry(rb_old, struct interval_tree_node, rb);
	struct interval_tree_node *new =
		rb_entry(rb_new, struct interval_tree_node, rb);

	new->__subtree_last = old->__subtree_last;
}

static void augment_rotate(struct rb_node *rb_old, struct rb_node *rb_new)
{
	struct interval_tree_node *old =
		rb_entry(rb_old, struct interval_tree_node, rb);
	struct interval_tree_node *new =
		rb_entry(rb_new, struct interval_tree_node, rb);

	new->__subtree_last = old->__subtree_last;
	old->__subtree_last = compute_subtree_last(old);
}

static const struct rb_augment_callbacks augment_callbacks = {
	augment_propagate, augment_copy, augment_rotate
};

void interval_tree_insert(struct interval_tree_node *node, struct rb_root *root)
{
	struct rb_node **link = &root->rb_node, *rb_parent = NULL;
	u32 start = node->start, last = node->last;
	struct interval_tree_node *parent;

	while (*link) {
		rb_parent = *link;
		parent = rb_entry(rb_parent, struct interval_tree_node, rb);
		if (parent->__subtree_last < last)
			parent->__subtree_last = last;
		if (start < parent->start)
			link = &parent->rb.rb_left;
		else
			link = &parent->rb.rb_right;
	}

	node->__subtree_last = last;
	rb_link_node(&node->rb, rb_parent, link);
	rb_insert_augmented(&node->rb, root, &augment_callbacks);
}

/* need rewrite or write new */
void interval_tree_remove(struct interval_tree_node *node, struct rb_root *root)
{
	rb_erase_augmented(&node->rb, root, &augment_callbacks);
}

/*  For delete interval from tree*/
void interval_tree_remove2(struct interval_tree_node *node, struct rb_root *root)
{
	struct interval_tree_node *tnode = NULL;
	tnode = interval_tree_first_match(root, node->start, node->last);

	if (tnode)
		rb_erase_augmented(&node->rb, root, &augment_callbacks);
}
EXPORT_SYMBOL(interval_tree_remove2);

/* Insertion with merge intervals */
void interval_tree_insert2(struct interval_tree_node *node, struct rb_root *root)
{
	struct interval_tree_node *tnode = NULL;

	tnode = interval_tree_first_match(root, node->start-1, node->last+1);
	if (tnode) {
		interval_tree_remove(tnode, root);
		if (node->start < tnode->start)
			tnode->start = node->start;
		if (node->last > tnode->last) {
			tnode->last = node->last;
//			tnode->__subtree_last = tnode->last; //   ???? maybee not need
		}
		interval_tree_insert2(tnode, root);
	} else
		interval_tree_insert(node, root);
}
EXPORT_SYMBOL(interval_tree_insert2);

void flush_tree(struct rb_root *root)
{
	struct interval_tree_node *val, *tmp;

	rbtree_postorder_for_each_entry_safe(val, tmp, root, rb) {
		kfree(val);
	}

	*root = RB_ROOT;
}
EXPORT_SYMBOL(flush_tree);

static int __init test7_init(void)
{
	struct interval_tree_node *sn;
	struct interval_tree_node *v1, *v2, *v3, *v4, *v5, *v6, *v7;
	struct interval_tree_node *v8;

/* init values for insertion to tree */
	v1 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v2 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v3 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v4 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v5 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v6 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v7 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v8 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
	v1->start = 2;
	v1->last = 5;
	v2->start = 4;
	v2->last = 7;
	v3->start = 10;
	v3->last = 20;
	v4->start = 22;
	v4->last = 30;
	v5->start = 40;
	v5->last = 50;
	v6->start = 8;
	v6->last = 9;
	v7->start = 51;
	v7->last = 55;
	v8->start = 60;
	v8->last = 65;

/* insert values to tree */
	interval_tree_insert2(v1, &root);
	interval_tree_insert2(v2, &root);
	interval_tree_insert2(v3, &root);
	interval_tree_insert2(v4, &root);
	interval_tree_insert2(v5, &root);
	interval_tree_insert2(v6, &root);
	interval_tree_insert2(v7, &root);
	pr_info("---------------------");
	print_tree_intervals(&root);

/* try search some interva in tree */
	sn = interval_tree_first_match(&root, 4, 9);
	if (sn)
		pr_info("---> [ %i ... %i ]\n", sn->start, sn->last);
//	pr_info("----- del 1 element v2 ------");
/* try remove some intervals from tree, with not exist in tree */
	interval_tree_remove2(v8, &root);

	interval_tree_insert2(v8, &root);

	print_tree_intervals(&root);
	return 0;
}

static void __exit test7_exit(void)
{
	struct interval_tree_node *v1, *sn;

	pr_info("Exiting from module rbi_tree\n");

/* clean tree */
	flush_tree(&root);

	v1 = kmalloc(sizeof(struct interval_tree_node), GFP_KERNEL);

	v1->start = 2;
	v1->last = 5;

/* search in enpty tree */
	sn = interval_tree_first_match(&root, v1->start, v1->last);
	BUG_ON(sn);

/* insertion in tree */
	interval_tree_insert2(v1, &root);

	sn = interval_tree_first_match(&root, v1->start, v1->last);
	BUG_ON(!sn);

/* clean tree again */
	flush_tree(&root);
	pr_info("Exit from module rbi_tree\n");
}

module_init(test7_init);
module_exit(test7_exit);