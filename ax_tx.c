#include "ax_tx.h"
#include "ax_log.h"
#include "ax_crypto.h"
#include "ax_user.h"
#include "ax_master.h"
#include "ax_kernel.h"
#include "ax_hmap.h"

#include <stdlib.h>
#include <time.h>

struct ax_txmgr
{
	map_void_t txdict;
};
static struct ax_txmgr GLOBAL;

void ax_txmgr_init()
{
	map_init(&GLOBAL.txdict);
}

int ax_tx_getSizeNoMac(const ax_tx* in)
{
	return ax_tx_getSize(in) - (int)sizeof(struct ax_tx_mac);
}

ax_tx* ax_txmgr_new(int type)
{
	ax_tx* ret = NULL;

	switch (type)
	{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT:
		{
			ret = malloc(sizeof(ax_tx_balance_mod));
			break;
		}
		case TX_PAIR:
		{
			ret = malloc(sizeof(ax_tx_pair));
			break;
		}
		case TX_MASTER:
		{
			ret = malloc(sizeof(struct ax_tx_master));
			break;
		}
		default:
		{
			AX_LOG_CRIT("Invalid transaction type.");
			abort();
		}
	}

	ret->type = (uint32_t)type;
	return ret;
}

ax_tx* ax_txmgr_get(void* hash)
{
	void** k = map_get(&GLOBAL.txdict, hash);
	if (k == NULL)
		return NULL;
	return (ax_tx*)*k;
}

int ax_tx_getHash(const ax_tx* in, void* out)
{
	int size = ax_tx_getSizeNoMac(in);

	if (size == -1)
		return -1;

	ax_makehash(in, (unsigned int)size, out);

	return 0;
}

static struct ax_tx_mac* _ax_tx_getMac(ax_tx* tx)
{
	switch (tx->type)
	{
	case TX_BALANCE_IN:
	case TX_BALANCE_OUT:
		return &((ax_tx_balance_mod*)tx)->mac;
	case TX_PAIR:
		return &((ax_tx_pair*)tx)->mac;
	case TX_MASTER:
		return &((struct ax_tx_master*)tx)->mac;
	default:
		AX_LOG_CRIT("Unsupported tx type %d.", tx->type);
		abort();
	}
}

static int _ax_tx_getAuthorizedKeys(ax_tx* tx, int* outtype, void* buffer, int* outcount)
{
	switch (tx->type)
	{
	case TX_BALANCE_IN:
	case TX_BALANCE_OUT:
		*outcount = 1;
		*outtype = 1;
		return ax_useridx_getMacPub(buffer, ((ax_tx_balance_mod*)tx)->user_id);
	case TX_PAIR:
		*outtype = 2;
		*outcount = 2;
		ax_useridx_getMacPub(buffer, ((ax_tx_pair*)tx)->maker_user_id);
		ax_useridx_getMacPub(buffer + 64, ((ax_tx_pair*)tx)->maker_user_id);
		return 0;
	case TX_MASTER:
		*outtype = 1;
		*outcount = 1;
		hextobuf(MASTER_PUBKEY + 2, 65);
		return 0;
	default:
		AX_LOG_CRIT("Unsupported tx type %d.", tx->type);
		abort();
	}

	return __LINE__;
}

int ax_tx_verify(ax_tx* in)
{
	int type;
	int count;
	uint8_t buffer[1024];
	uint8_t hash[32];

	ax_makehash(in, (unsigned int)ax_tx_getSizeNoMac(in), hash);

	struct ax_tx_mac* mac = _ax_tx_getMac(in);
	if (_ax_tx_getAuthorizedKeys(in, &type, buffer, &count) == 0)
	{
		if (ax_verisig(hash, 32, buffer, mac->sig) == 0)
			return 0;
	}

	char nodepub[65];
	ax_node* n = ax_kernel_getSelfNode();
	int offset = ax_tx_getSizeNoMac(in);
	ax_getpub(n->secret, 32, nodepub);

	return ax_verisig(hash, 32, nodepub, ((uint8_t*)in) + offset);
}

void ax_tx_build(ax_tx* tx)
{
	tx->lock = (uint32_t)time(NULL);
	tx->nonce = (uint16_t)(rand() % 0x10000);
}

int ax_tx_getSize(const ax_tx* in)
{
	switch (in->type)
	{
		case TX_BALANCE_IN:
		case TX_BALANCE_OUT:
			return sizeof(ax_tx_balance_mod);
		case TX_PAIR:
			return sizeof(ax_tx_pair);
		case TX_MASTER:
			return sizeof(struct ax_tx_master);
		default:
			return -1;
	}
}

int ax_txmgr_put(const ax_tx* in)
{
	uint8_t buffer[32];

	ax_tx_getHash(in, buffer);
	map_set(&GLOBAL.txdict, buffer, in);

	return 0;
}

int ax_tx_sign(const ax_tx* in, uint8_t* pk)
{
	uint8_t hash[32];
	if (in->type == TX_BALANCE_IN || in->type == TX_BALANCE_OUT)
	{
		ax_tx_balance_mod* tx = (ax_tx_balance_mod*)in;
		//ax_getpub(pk, 32, tx->mac.pub);
		ax_tx_getHash(in, hash);
		ax_makesig(hash, 32, pk, 32, tx->mac.sig, 65);
	}
	else if (in->type == TX_PAIR)
	{
		ax_tx_pair* tx = (ax_tx_pair*)in;
		//ax_getpub(pk, 32, tx->mac.pub);
		ax_tx_getHash(in, hash);
		ax_makesig(hash, 32, pk, 32, tx->mac.sig, 65);
	}
	else if (in->type == TX_MASTER)
	{
		struct ax_tx_master* tx = (struct ax_tx_master*)in;
		ax_tx_getHash(in, hash);
		ax_makesig(hash, 32, pk, 32, tx->mac.sig, 65);
	}
	else
	{
		AX_LOG_CRIT("Invalid tx type.");
		return 1;
	}

	return 0;
}

void ax_txmgr_free(ax_tx* tx)
{
	free(tx);
}