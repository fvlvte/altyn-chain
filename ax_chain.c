#include "ax_chain.h"
#include "ax_hmap.h"
#include "ax_log.h"
#include "ax_kernel.h"

#include <stdio.h>
#include <stdint.h>

#define CHAIN_MAX 1024*1024

struct _ax_chain
{
	map_void_t blocks;
	ax_block** blko;
	uint32_t blkn;
};

static struct _ax_chain GLOBAL;

static void _ax_chain_importGenesis()
{
	char key[65];
	ax_block* gblk = ax_block_getGenesis();
	GLOBAL.blko[GLOBAL.blkn++] = gblk;
	ax_block_getHash(gblk, key);

	hexfmt(key, 32);
	map_set(&GLOBAL.blocks, key, gblk);
}

void ax_chain_init(void)
{
	GLOBAL.blkn = 0;
	GLOBAL.blko = malloc(sizeof(ax_block*) * CHAIN_MAX);

	map_init(&GLOBAL.blocks);

	if (GLOBAL.blko == NULL)
	{
		AX_LOG_ERRO("Failed to allocate chain index table.");
		abort();
	}
}