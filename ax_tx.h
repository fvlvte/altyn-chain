#pragma once
#include <stdint.h>
#include <openssl/ecdsa.h>

enum
{
	TX_BALANCE_IN = 0,
	TX_BALANCE_OUT,
	TX_PAIR,
	TX_MASTER
};

struct ax_tx
{
	uint16_t type;
	uint64_t lock;
	uint32_t nonce; // Order.
} __attribute__((__packed__));
typedef struct ax_tx ax_tx;

struct ax_tx_mac
{
	uint8_t sig[64];
} __attribute__((__packed__));

struct ax_tx_master
{
	ax_tx header;

	uint8_t pubkey[65];

	struct ax_tx_mac mac;

};

struct ax_tx_balance_mod
{
	struct ax_tx header;

	uint32_t sequence;

	uint32_t user_id;
	uint16_t currency_id;
	uint64_t balance_low;
	uint32_t balance_high;

	struct ax_tx_mac mac;
} __attribute__((__packed__));
typedef struct ax_tx_balance_mod ax_tx_balance_mod;

struct ax_tx_pkmod
{
	struct ax_tx header;
	uint32_t user_id;
	uint8_t pk[64];
	struct ax_tx_mac mac;
} __attribute__((__packed__));
typedef struct ax_tx_balance_mod ax_tx_balance_mod;

struct ax_tx_pair
{
	struct ax_tx header;

	uint32_t taker_sequence;
	uint32_t taker_user_id;
	uint16_t taker_currency_id;
	uint64_t taker_balance_low;
	uint32_t taker_balance_high;

	uint32_t maker_sequence;
	uint32_t maker_user_id;
	uint16_t maker_currency_id;
	uint64_t maker_balance_low;
	uint32_t maker_balance_high;

	uint64_t taker_fee_low;
	uint32_t taker_fee_high;

	uint64_t maker_fee_low;
	uint32_t maker_fee_high;

	struct ax_tx_mac mac;
} __attribute__((__packed__));
typedef struct ax_tx_pair ax_tx_pair;

void ax_txmgr_init();

int ax_txmgr_put(const ax_tx* in);
ax_tx* ax_txmgr_get(void* hash);

int ax_tx_getSize(const ax_tx* in);
void ax_tx_build(ax_tx* tx);
ax_tx* ax_txmgr_new(int type);
int ax_tx_sign(const ax_tx* in, uint8_t* pk);
int ax_tx_verify(ax_tx* in);
void ax_txmgr_free(ax_tx* tx);
int ax_tx_getHash(const ax_tx* in, void* out);