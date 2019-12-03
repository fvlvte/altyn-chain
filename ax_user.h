#pragma once
#include <stdint.h>
#include "ax_tx.h"

#define WALLET_MAX 16
#define USER_MAX 1024*1024

struct ax_user_wallet
{
	uint64_t currency_id;
	uint64_t balance_high;
	uint64_t balance_low;
};
typedef struct ax_user_wallet ax_user_wallet;

struct ax_user
{
	uint32_t id;
	uint16_t wallet_count;
	uint8_t auth[64];
	ax_user_wallet wallets[WALLET_MAX];
};
typedef struct ax_user ax_user;

int ax_user_commitTran(ax_tx* tran);
int ax_useridx_getMacPub(void* out, uint32_t index);
void ax_useridx_init(void);
ax_user* ax_useridx_alloc(unsigned int index);
ax_user* ax_useridx_get(unsigned int index);

