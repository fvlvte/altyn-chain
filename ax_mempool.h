#pragma once
#include "ax_tx.h"
#include "ax_block.h"

typedef void* altyn_mempool;

altyn_mempool* altyn_mmp_new(void);
void altyn_mmp_free(altyn_mempool* pool);

int altyn_mmp_push(altyn_mempool* pool, ax_tx* tx);
int altyn_mmp_get(altyn_mempool* pool, void* out, unsigned int outLen);
void altyn_mmp_flush(altyn_mempool* pool, ax_block* blk);
int altyn_mmp_makeBlockCandidate(altyn_mempool* pool, ax_block** out_candidate);
