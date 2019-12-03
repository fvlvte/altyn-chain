#include "ax_user.h"
#include "ax_log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum
{
	TYPE_ADD = 0,
	TYPE_SUB
};

struct ax_user_index
{
	uint64_t	max;
	ax_user**	idx;
};
typedef struct ax_user_index ax_user_index;
ax_user_index GLOBAL_INDEX;

const __uint128_t ONE_HIGH = ((__uint128_t)0x0000000000000001 << 64) | 0x00000000;

static inline ax_user_wallet* _ax_user_getWallet(uint16_t currency_id, ax_user* user)
{
	unsigned int i;

	for (i = 0; i < user->wallet_count; i++)
	{
		if (user->wallets[i].currency_id == currency_id)
			return &user->wallets[i];
	}

	if (user->wallet_count >= WALLET_MAX)
	{
		AX_LOG_CRIT("!!! User (%d) wallet limit exceded. !!!", user->id);
		abort();
	}

	memset(&user->wallets[user->wallet_count], 0, sizeof(ax_user_wallet));
	user->wallets[user->wallet_count].currency_id = currency_id;

	return &user->wallets[user->wallet_count++];
}

static inline int _ax_user_hasFunds
	(ax_user* user, uint16_t currency_id, uint32_t high, uint64_t low, uint32_t fee_high, uint64_t fee_low)
{
	__uint128_t tmp;
	ax_user_wallet* wallet;
	uint32_t rhigh;
	uint64_t rlow;

	wallet = _ax_user_getWallet(currency_id, user);

	rhigh = high + fee_high;
	tmp = low + fee_low;

	if (tmp > 0xFFFFFFFFFFFFFFFFUL)
	{
		rhigh++;
		tmp -= ONE_HIGH;
		rlow = (uint64_t)tmp;
	}
	else
		rlow = (uint64_t)tmp;
	
	return ((wallet->balance_high - 1 >= rhigh) || (wallet->balance_high >= rhigh && wallet->balance_low >= rlow)) ? 1 : 0;
}

static inline int _ax_user_alterWallet(uint16_t currency_id, ax_user* user, int type, uint32_t high, uint64_t low)
{
	__uint128_t temp;
	ax_user_wallet* wallet = _ax_user_getWallet(currency_id, user);
	
	if (type == TYPE_ADD)
	{
		wallet->balance_high += high;
		temp = low + wallet->balance_low;
		if (temp > 0xFFFFFFFFFFFFFFFFUL)
		{
			wallet->balance_high++;
			temp -= ONE_HIGH;
			wallet->balance_low = (uint64_t)temp;
		}
		else
		{
			wallet->balance_low = (uint64_t)temp;
		}
	}
	else
	{
		if (wallet->balance_high < high)
			return -1;
		if (wallet->balance_low < low && wallet->balance_high - 1 < high)
			return -1;

		wallet->balance_high -= high;
		if (wallet->balance_low < low)
		{
			wallet->balance_high--;
			temp = ONE_HIGH + wallet->balance_low - low;
			wallet->balance_low = (uint64_t)temp;
		}
		else
			wallet->balance_low -= low;
	}

	return 0;
}

int ax_user_commitTran(ax_tx* tran)
{
	switch (tran->type)
	{
		case TX_BALANCE_IN:
		{
			ax_tx_balance_mod* bm = (ax_tx_balance_mod*)tran;

			ax_user* user = ax_useridx_get(bm->user_id);
			if (user == NULL)
				user = ax_useridx_alloc(bm->user_id);

			return _ax_user_alterWallet(bm->currency_id, user, TYPE_ADD, bm->balance_high, bm->balance_low);	
		}
		case TX_BALANCE_OUT:
		{
			ax_tx_balance_mod* bm = (ax_tx_balance_mod*)tran;

			ax_user* user = ax_useridx_get(bm->user_id);
			if (user == NULL)
			{
				AX_LOG_ERRO("User (%d) balance does not exist.", bm->user_id);
				return __LINE__;
			}

			return _ax_user_alterWallet(bm->currency_id, user, TYPE_SUB, bm->balance_high, bm->balance_low);
		}
		case TX_PAIR:
		{
			ax_tx_pair* bm = (ax_tx_pair*)tran;

			ax_user* taker = ax_useridx_get(bm->taker_user_id);
			ax_user* maker = ax_useridx_get(bm->maker_user_id);

			if (taker == NULL || maker == NULL)
			{
				AX_LOG_ERRO("Pair does not exist.");
				return __LINE__;
			}

			if(
				_ax_user_hasFunds(
					taker,
					bm->taker_currency_id,
					bm->taker_balance_high,
					bm->taker_balance_low,
					bm->taker_fee_high,
					bm->taker_fee_high
				) != 1 ||
				_ax_user_hasFunds(
					maker,
					bm->maker_currency_id,
					bm->maker_balance_high,
					bm->maker_balance_low,
					bm->maker_fee_high,
					bm->maker_fee_low
				) != 1
			)
			{
				AX_LOG_ERRO("Insufficient balance.");
				return __LINE__;
			}

			_ax_user_alterWallet(bm->taker_currency_id, taker, TYPE_SUB, bm->taker_balance_high, bm->taker_balance_low);
			_ax_user_alterWallet(bm->taker_currency_id, taker, TYPE_SUB, bm->taker_fee_high, bm->taker_fee_low);
			_ax_user_alterWallet(bm->maker_currency_id, taker, TYPE_ADD, bm->maker_balance_high, bm->maker_balance_low);

			_ax_user_alterWallet(bm->maker_currency_id, maker, TYPE_SUB, bm->maker_balance_high, bm->maker_balance_low);
			_ax_user_alterWallet(bm->maker_currency_id, maker, TYPE_SUB, bm->maker_fee_high, bm->maker_fee_low);
			_ax_user_alterWallet(bm->taker_currency_id, maker, TYPE_ADD, bm->taker_balance_high, bm->taker_balance_low);

			return 0;
		}
	}

	AX_LOG_ERRO("Unsupported TX.");
	return __LINE__;
}

void ax_useridx_init(void)
{
	GLOBAL_INDEX.max = USER_MAX;
	GLOBAL_INDEX.idx = calloc(1, USER_MAX * sizeof(ax_user*));

	if (GLOBAL_INDEX.idx == NULL)
	{
		AX_LOG_CRIT("!!! Failed to allocate user index. !!!");
		abort();
	}
}

ax_user* ax_useridx_get(uint32_t index)
{
	if (index >= GLOBAL_INDEX.max)
	{
		AX_LOG_ERRO("!!! User index too high (%d). !!!", index);
		return NULL;
	}

	return GLOBAL_INDEX.idx[index];
}

int ax_useridx_getMacPub(void* out, uint32_t index)
{
	if (GLOBAL_INDEX.idx[index] != NULL)
	{
		memcpy(out, GLOBAL_INDEX.idx[index]->auth, sizeof(GLOBAL_INDEX.idx[index]->auth));
		return 0;
	}

	return __LINE__;
}

ax_user* ax_useridx_alloc(uint32_t index)
{
	assert(GLOBAL_INDEX.max > index && GLOBAL_INDEX.idx[index] == NULL);

	GLOBAL_INDEX.idx[index] = calloc(sizeof(ax_user), 1);

	if (GLOBAL_INDEX.idx[index] == NULL)
	{
		AX_LOG_ERRO("!!! User index alloc failure. !!!", index);
		return NULL;
	}

	GLOBAL_INDEX.idx[index]->id = index;

	return GLOBAL_INDEX.idx[index];
}

