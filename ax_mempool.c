#include "ax_mempool.h"
#include "ax_log.h"
#include "ax_hmap.h"
#include "ax_kernel.h"
#include "ax_user.h"

struct _altyn_mempool
{
	map_void_t mvp;
};
typedef struct _altyn_mempool _altyn_mempool;

struct _altyn_mmp_out
{
	uint8_t* buffer;
	int offset;
};
typedef struct _altyn_mmp_out _altyn_mmp_out;

altyn_mempool* altyn_mmp_new(void)
{
	_altyn_mempool* mpm = (_altyn_mempool*)malloc(sizeof(_altyn_mempool));
	map_init(&mpm->mvp);
	return (void*)mpm;
}

void altyn_mmp_free(altyn_mempool* pool)
{
	_altyn_mempool* mpm = (_altyn_mempool*)pool;
	map_deinit(&mpm->mvp);
	free(pool);
}

int _altyn_mmp_set(void* key, int count, void** value, void* user)
{

	int sz;
	_altyn_mmp_out* data = (_altyn_mmp_out*)user;

	ax_tx* tx = (ax_tx*)*value;
	sz = ax_tx_getSize(tx);

	memcpy(data->buffer + data->offset, tx, (size_t)sz);
	data->offset += sz;

	return 1;
}

int altyn_mmp_get(altyn_mempool* pool, void* out, unsigned int outLen)
{
	_altyn_mempool* mpm = (_altyn_mempool*)pool;
	map_iter_t iter = map_iter(&mpm->mvp);
	int outlen = 0;

	char kc[65];
	const char* key = map_next(&mpm->mvp, &iter);

	while (key != NULL) {
		memcpy(kc, key, 64);
		kc[64] = 0;
		ax_tx* tx = *(ax_tx**)map_get(&mpm->mvp, kc);

		if (tx != NULL)
		{
			int sz = ax_tx_getSize(tx);

			memcpy(out + outlen, tx, (size_t)sz);
			outlen += sz;

			
		}
		key = map_next(&mpm->mvp, &iter);
	}

	return outlen;
}

int altyn_mmp_push(altyn_mempool* pool, ax_tx* tx)
{
	_altyn_mempool* mpm = (_altyn_mempool*)pool;
	char hash[65];

	if (ax_tx_verify(tx) != 0)
	{
		AX_LOG_INFO("Invalid signature for tx.");
		return __LINE__;
	}

	switch (tx->type)
	{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT: {
			ax_tx_balance_mod* bm = (ax_tx_balance_mod*)tx;
			int seq = ax_user_getSequenceId(bm->user_id);
			if (seq != bm->sequence)
				return -3;
			break;
		}
		case TX_PAIR: {
			ax_tx_pair* pair = (ax_tx_pair*)tx;
			int s1 = ax_user_getSequenceId(pair->taker_user_id), s2 = ax_user_getSequenceId(pair->maker_user_id);
			if (s1 != pair->taker_sequence || s2 != pair->maker_sequence)
				return -3;
			break;
		}
	}

	ax_tx_getHash(tx, hash);
	hexfmt(hash, 32);
	if (map_get(&mpm->mvp, hash) != NULL)
	{
		return -1;
	}

	map_set(&mpm->mvp, hash, tx);

	return 0;
}

void altyn_mmp_flush(altyn_mempool* pool, ax_block* blk)
{

}

int altyn_mmp_makeBlockCandidate(altyn_mempool* pool, ax_block** out_candidate)
{
	ax_block* blk = ax_block_alloc();
	_altyn_mempool* mpm = (_altyn_mempool*)pool;
	map_iter_t iter = map_iter(&mpm->mvp);

	const char* key = map_next(&mpm->mvp, &iter);

	while (key != NULL) {
		ax_tx* tx = *(ax_tx**)map_get(&mpm->mvp, key);

		if (tx != NULL)
		{
			ax_block_putTx(blk, tx);
		}
		key = map_next(&mpm->mvp, &iter);
	}

	return 0;
}