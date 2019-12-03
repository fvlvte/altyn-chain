/**
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/*
* Original author: https://github.com/rxi/map
*/

#include <stdlib.h>
#include <string.h>

#include "ax_hmap.h"

struct map_node_t {
	unsigned hash;
	void* value;
	map_node_t* next;
};


static unsigned map_hash(const char* str) {
	unsigned hash = 5381;
	while (*str) {
		hash = ((hash << 5) + hash) ^ (unsigned int)(*str++);
	}
	return hash;
}


static map_node_t* map_newnode(const char* key, void* value, int vsize) {
	map_node_t* node;
	int ksize = (int)strlen(key) + 1;
	int voffset = ksize + (((int)sizeof(void*) - ksize) % (int)sizeof(void*));
	node = malloc((size_t)((int)sizeof(*node) + voffset + vsize));
	if (!node) return NULL;
	memcpy(node + 1, key, (size_t)ksize);
	node->hash = map_hash(key);
	node->value = ((char*)(node + 1)) + voffset;
	memcpy(node->value, value, (size_t)vsize);
	return node;
}


static int map_bucketidx(map_base_t* m, unsigned hash) {
	return (int)(hash & (m->nbuckets - 1));
}


static void map_addnode(map_base_t* m, map_node_t* node) {
	int n = map_bucketidx(m, node->hash);
	node->next = m->buckets[n];
	m->buckets[n] = node;
}


static int map_resize(map_base_t* m, int nbuckets) {
	map_node_t* nodes, * node, * next;
	map_node_t** buckets;
	int i;

	nodes = NULL;
	i = (int)m->nbuckets;
	while (i--) {
		node = (m->buckets)[i];
		while (node) {
			next = node->next;
			node->next = nodes;
			nodes = node;
			node = next;
		}
	}

	buckets = realloc(m->buckets, (size_t)(sizeof(*m->buckets) * (long unsigned int)nbuckets));
	if (buckets != NULL) {
		m->buckets = buckets;
		m->nbuckets = (unsigned int)nbuckets;
	}
	if (m->buckets) {
		memset(m->buckets, 0, sizeof(*m->buckets) * m->nbuckets);

		node = nodes;
		while (node) {
			next = node->next;
			map_addnode(m, node);
			node = next;
		}
	}

	return (buckets == NULL) ? -1 : 0;
}


static map_node_t** map_getref(map_base_t* m, const char* key) {
	unsigned hash = map_hash(key);
	map_node_t** next;
	if (m->nbuckets > 0) {
		next = &m->buckets[map_bucketidx(m, hash)];
		while (*next) {
			if ((*next)->hash == hash && !strcmp((char*)(*next + 1), key)) {
				return next;
			}
			next = &(*next)->next;
		}
	}
	return NULL;
}


void map_deinit_(map_base_t* m) {
	map_node_t* next, * node;
	int i;
	i = (int)m->nbuckets;
	while (i--) {
		node = m->buckets[i];
		while (node) {
			next = node->next;
			free(node);
			node = next;
		}
	}
	free(m->buckets);
}


void* map_get_(map_base_t* m, const char* key) {
	map_node_t** next = map_getref(m, key);
	return next ? (*next)->value : NULL;
}


int map_set_(map_base_t* m, const char* key, void* value, int vsize) {
	int n, err;
	map_node_t** next, * node;

	next = map_getref(m, key);
	if (next) {
		memcpy((*next)->value, value, (size_t)vsize);
		return 0;
	}

	node = map_newnode(key, value, vsize);
	if (node == NULL) goto fail;
	if (m->nnodes >= m->nbuckets) {
		n =(m->nbuckets > 0) ? (int) (m->nbuckets << 1) : 1;
		err = map_resize(m, n);
		if (err) goto fail;
	}
	map_addnode(m, node);
	m->nnodes++;
	return 0;
fail:
	if (node) free(node);
	return -1;
}


void map_remove_(map_base_t* m, const char* key) {
	map_node_t* node;
	map_node_t** next = map_getref(m, key);
	if (next) {
		node = *next;
		*next = (*next)->next;
		free(node);
		m->nnodes--;
	}
}


map_iter_t map_iter_(void) {
	map_iter_t iter;
	iter.bucketidx = (unsigned int)-1;
	iter.node = NULL;
	return iter;
}


const char* map_next_(map_base_t* m, map_iter_t* iter) {
	if (iter->node) {
		iter->node = iter->node->next;
		if (iter->node == NULL) goto nextBucket;
	}
	else {
	nextBucket:
		do {
			if (++iter->bucketidx >= m->nbuckets) {
				return NULL;
			}
			iter->node = m->buckets[iter->bucketidx];
		} while (iter->node == NULL);
	}
	return (char*)(iter->node + 1);
}