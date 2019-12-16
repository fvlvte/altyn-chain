#include "ax_block.h"
#include "ax_log.h"
#include "ax_kernel.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "ax_master.h"

ax_block* ax_block_alloc()
{
	ax_block* block = malloc(sizeof(ax_block));

	if (block == NULL)
	{
		AX_LOG_CRIT("Failed to allocate memory for block for reason %d.", errno);
		abort();
	}

	block->version = 1;
	block->timestamp = (uint64_t)time(NULL);
	block->height = 69;
	block->nonce = 0x0d0d;
	block->txc = 0;
	block->txs = malloc(sizeof(ax_tx*) * BLOCK_TX_MAX);
	block->txs[0] = NULL;

	ax_makehash_ctx_init(&block->ctx);

	return block;
}

void ax_block_setMerkle(ax_block* block, void* merkle)
{
	memcpy(block->merkle, merkle, 32);
}

void ax_block_setPrevious(ax_block* block)
{
	//uint8_t hash;
}

void ax_block_putTx(ax_block* block, ax_tx* tx)
{
	block->txs[block->txc++] = tx;
	block->txs[block->txc] = NULL;
}

int ax_block_getHash(ax_block* blk, void* out)
{
	ax_cryptoctx_sha256 hash;
	ax_makehash_ctx_init(&hash);

	ax_makehash_ctx_update(&hash, &blk->version, sizeof(blk->version));
	ax_makehash_ctx_update(&hash, &blk->timestamp, sizeof(blk->timestamp));
	ax_makehash_ctx_update(&hash, &blk->height, sizeof(blk->prevHash));
	ax_makehash_ctx_update(&hash, &blk->nonce, sizeof(blk->nonce));
	ax_makehash_ctx_update(&hash, &blk->prevHash, sizeof(blk->prevHash));
	ax_makehash_ctx_update(&hash, &blk->merkle, sizeof(blk->merkle));
	ax_makehash_ctx_update(&hash, &blk->miner, sizeof(blk->miner));

	ax_tx** ar = blk->txs;
	while (*ar != NULL)
	{
		unsigned int size = 0;
		switch ((*ar)->type)
		{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT:
			size = sizeof(ax_tx_balance_mod);
		case TX_PAIR:
			size = sizeof(ax_tx_pair);
		}

		ax_makehash_ctx_update(&hash, *ar, size);

		ar++;
	}

	ax_makehash_ctx_update(&hash, blk->signature, sizeof(blk->signature));
	ax_makehash_ctx_final(&hash, out);

	return 0;
}

void ax_block_save(ax_block* block)
{
	unsigned char name[128];

	sprintf(name, "%d.blk.bin", block->height);

	FILE* f = fopen(name, "wb");

	// Calculate merkle root.
	ax_cryptoctx_sha256 merkle;
	ax_makehash_ctx_init(&merkle);

	ax_makehash_ctx_update(&merkle, block->merkle, sizeof(block->merkle));

	ax_tx** ar = block->txs;
	while (*ar != NULL)
	{
		unsigned int size = 0;

		switch ((*ar)->type)
		{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT:
			size = sizeof(ax_tx_balance_mod);
		case TX_PAIR:
			size = sizeof(ax_tx_pair);
		}

		ax_makehash_ctx_update(&merkle, *ar, size);

		ar++;
	}
	ax_makehash_ctx_final(&merkle, block->merkle);

	fwrite(&block->version, sizeof(block->version), 1, f);
	ax_makehash_ctx_update(&block->ctx, &block->version, sizeof(block->version));
	fwrite(&block->timestamp, sizeof(block->timestamp), 1, f);
	ax_makehash_ctx_update(&block->ctx, &block->timestamp, sizeof(block->timestamp));
	fwrite(&block->height, sizeof(block->height), 1, f);
	ax_makehash_ctx_update(&block->ctx, &block->height, sizeof(block->height));
	fwrite(&block->nonce, sizeof(block->nonce), 1, f);
	ax_makehash_ctx_update(&block->ctx, &block->version, sizeof(block->nonce));
	fwrite(block->prevHash, sizeof(block->prevHash), 1, f);
	ax_makehash_ctx_update(&block->ctx, block->prevHash, sizeof(block->prevHash));
	fwrite(block->merkle, sizeof(block->merkle), 1, f);
	ax_makehash_ctx_update(&block->ctx, block->merkle, sizeof(block->merkle));
	fwrite(block->miner, sizeof(block->miner), 1, f);
	ax_makehash_ctx_update(&block->ctx, &block->miner, sizeof(block->miner));

	ar = block->txs;
	while (*ar != NULL)
	{
		unsigned int size = 0;
		switch ((*ar)->type)
		{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT:
			size = sizeof(ax_tx_balance_mod);
		case TX_PAIR:
			size = sizeof(ax_tx_pair);
		case TX_MASTER:
			size = sizeof(struct ax_tx_master);
		}

		fwrite(*ar, (size_t)size, 1, f);
		ax_makehash_ctx_update(&block->ctx, *ar, size);

		ar++;
	}

	unsigned char blkhash[32];
	ax_makehash_ctx_final(&block->ctx, blkhash);

	unsigned char sig[64];
	ax_makesig(blkhash, 32, "11", 32, sig, 64);
	fwrite(sig, 64, 1, f);

	fclose(f);
}

ax_block* ax_block_getGenesis(void)
{
	ax_block* blk = ax_block_alloc();
	ax_node* n = ax_kernel_getSelfNode();

	blk->height = 0;
	memset(blk->prevHash, 0, 32);
	blk->flags = 0;
	memset(blk->miner, 0, 65);
	blk->nonce = 0x534B414D;
	blk->timestamp = 1575989947;
	
	struct ax_tx_master* tx = (struct ax_tx_master*)ax_txmgr_new(TX_MASTER);

	tx->header.lock = 1575989947;
	tx->header.nonce = 0x4A554843;
	tx->header.type = TX_MASTER;

	hextobuf(MASTER_PUBKEY + 2, tx->pubkey);

	ax_tx_sign((ax_tx*)tx, n->secret);

	ax_block_putTx(blk, (ax_tx*)tx);

	ax_block_save(blk);
}

void ax_block_free(ax_block* block)
{
	free(block->txs);
	free(block);
}