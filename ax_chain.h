#pragma once
#include "ax_block.h"

typedef void* ax_chain;

void ax_chain_init(void);

void ax_chain_put(ax_block* blk);