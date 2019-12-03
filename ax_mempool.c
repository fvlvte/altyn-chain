#include "ax_mempool.h"
#include "ax_log.h"
#include "ax_hmap.h"
#include "ax_kernel.h"

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
	//mpm->dict = ax_dict_new(1024 * 1024);
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
	AX_LOG_TRAC("%d TEST", count);
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

	const char* key = map_next(&mpm->mvp, &iter);
	while (key != NULL) {
		ax_tx* tx = *(ax_tx**)map_get(&mpm->mvp, key);
		int sz = ax_tx_getSize(tx);

		memcpy(out + outlen, tx, (size_t)sz);
		outlen += sz;

		key = map_next(&mpm->mvp, &iter);
	}

	return outlen;
}

int altyn_mmp_push(altyn_mempool* pool, ax_tx* tx)
{
	_altyn_mempool* mpm = (_altyn_mempool*)pool;
	uint8_t hash[65];
	


	if (ax_tx_verify(tx) != 0)
	{
		AX_LOG_INFO("Invalid signature for tx.");
		return __LINE__;
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
	return 0;
}