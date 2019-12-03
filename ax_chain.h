#pragma once
#include <stdint.h>
#include "ax_tx.h"

struct ax_block_header
{
	uint8_t		previous[32];	// Previous block hash.
	uint8_t		root[32];		// Merkle root.
	uint16_t	version;		// Version.
	uint64_t	lock;			// Lock time.
	uint32_t	tx_count;		// Count of transactions.
	uint64_t	nonce;			// Nonce.
	uint8_t		miner[32];		// Miner hash.
};
typedef struct ax_block_header ax_block_header;

struct ax_block
{
	ax_block_header header;
	ax_tx** tx_table;
	unsigned int table_max;
	unsigned int table_count;
};
typedef struct ax_block ax_block;

void ax_block_create(ax_block* blk);
