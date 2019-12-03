#pragma once
#include "ax_tx.h"
#include "ax_crypto.h"

#include <stdint.h>

#define BLOCK_TX_MAX 1024*1024
#define UPDATE_MERKLE 0b1
#define SELFSIGN 0b11

struct ax_block
{
	uint64_t height;
	uint64_t timestamp;
	uint32_t version;

	uint32_t nonce;

	uint8_t hash[32];

	uint8_t prevHash[32];

	uint8_t merkle[32];

	uint8_t miner[65];
	uint8_t signature[65];

	ax_tx** txs;
	unsigned int txc;

	ax_cryptoctx_sha256 ctx;

	int flags;
};
typedef struct ax_block ax_block;

ax_block* ax_block_alloc();
void ax_block_putTx(ax_block* block, ax_tx* tx);
void ax_block_free(ax_block* block);
void ax_block_save(ax_block* block);
void ax_block_putTx(ax_block* block, ax_tx* tx);