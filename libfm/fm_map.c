/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_map.c
 */

#include "fm_core.h"
#include "fm_map.h"

struct fm__map {
	struct node {
		int depth;
		char *key; /* copied */
		void *val; /* borrowed */
		struct node *left;
		struct node *right;
	} *root;
};

static int
delta(const struct node *node)
{
	return node ? node->depth : -1;
}

static int
balance(const struct node *node)
{
	return delta(node->left) - delta(node->right);
}

static int
depth(const struct node *a, const struct node *b)
{
	return (delta(a) > delta(b)) ? (delta(a) + 1) : (delta(b) + 1);
}

static struct node *
rotate_right(struct node *node)
{
	struct node *root;

	root = node->left;
	node->left = root->right;
	root->right = node;
	node->depth = depth(node->left, node->right);
	root->depth = depth(root->left, root->right);
	return root;
}

static struct node *
rotate_left(struct node *node)
{
	struct node *root;

	root = node->right;
	node->right = root->left;
	root->left = node;
	node->depth = depth(node->left, node->right);
	root->depth = depth(root->left, root->right);
	return root;
}

static struct node *
rotate_left_right(struct node *node)
{
	node->left = rotate_left(node->left);
	return rotate_right(node);
}

static struct node *
rotate_right_left(struct node *node)
{
	node->right = rotate_right(node->right);
	return rotate_left(node);
}

static void
destroy(struct node *root)
{
	if (root) {
		destroy(root->left);
		destroy(root->right);
		free(root->key);
		memset(root, 0, sizeof (struct node));
		free(root);
	}
}

static struct node *
update(struct node *root, const char *key, void *val)
{
	struct node *node;
	int d;

	if (!root) {
		if (!(root = fm__malloc(sizeof (struct node)))) {
			FM__TRACE(0);
			return NULL;
		}
		memset(root, 0, sizeof (struct node));
		root->left = NULL;
		root->right = NULL;
		if (!(root->key = fm__strdup(key))) {
			free(root);
			FM__TRACE(0);
			return NULL;
		}
		root->val = val;
		return root;
	}
	if (!(d = strcmp(key, root->key))) {
		root->val = val;
		return root;
	}
	if (0 > d) {
		if (!(node = update(root->left, key, val))) {
			FM__TRACE(0);
			return NULL;
		}
		root->left = node;
		if (1 < balance(root)) {
			if (0 <= balance(root->left)) {
				root = rotate_right(root);
			}
			else {
				root = rotate_left_right(root);
			}
		}
	}
	else {
		if (!(node = update(root->right, key, val))) {
			FM__TRACE(0);
			return NULL;
		}
		root->right = node;
		if (-1 > balance(root)) {
			if (0 >= balance(root->right)) {
				root = rotate_left(root);
			}
			else {
				root = rotate_right_left(root);
			}
		}
	}
	root->depth = depth(root->left, root->right);
	return root;
}

fm__map_t
fm__map_open(void)
{
	struct fm__map *map;

	if (!(map = fm__malloc(sizeof (struct fm__map)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(map, 0, sizeof (struct fm__map));
	map->root = NULL;
	return map;
}

void
fm__map_close(fm__map_t map)
{
	if (map) {
		destroy(map->root);
		free(map);
	}
}

int
fm__map_update(fm__map_t map, const char *key, void *val)
{
	struct node *root;

	assert( map && key && (*key) && val );

	if (!(root = update(map->root, key, val))) {
		FM__TRACE(0);
		return -1;
	}
	map->root = root;
	return 0;
}

void *
fm__map_lookup(fm__map_t map, const char *key)
{
	const struct node *node;
	int d;

	assert( map && key && (*key) );

	node = map->root;
	while (node) {
		if (!(d = strcmp(key, node->key))) {
			return node->val;
		}
		node = (0 > d) ? node->left : node->right;
	}
	return NULL;
}
