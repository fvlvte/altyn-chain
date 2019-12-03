#pragma once
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#include "ax_node.h"
#include "ax_mempool.h"

enum
{
	KERNEL_FLAG_MINE = 0b1,
	KERNEL_FLAG_DISABLE_P2P = 0b11
};

struct ax_kernel
{
	uint64_t work;
	uint64_t flags;

	uint64_t currentHeight;

	ax_node self;
	altyn_mempool* mpm;

	pthread_mutex_t lock;

	uint64_t tick;
	uint64_t delta;
	struct timespec last;
};
typedef struct ax_kernel ax_kernel;

ax_kernel* ax_kernel_alloc(void);
ax_kernel* ax_kernel_get(void);
unsigned int ax_kernel_getCurrentHeight(void);
void ax_kernel_getNodePubkey(char* out);
void ax_kernel_free(ax_kernel** kernel);
int ax_kernel_processArgs(ax_kernel* kernel, unsigned int count, char** args);
int ax_kernel_worker(ax_kernel* instance);
ax_node* ax_kernel_getSelfNode(void);
altyn_mempool* ax_kernel_getMempool(void);
void hexfmt(uint8_t* in, unsigned int len);
void hextobuf(const uint8_t* hex, void* out);