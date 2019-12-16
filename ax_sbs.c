#include "ax_api.h"
#include "ax_kernel.h"
#include "ax_log.h"
#include "ax_tx.h"
#include "ax_user.h"
#include "altyn_constants.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>

#include <string.h>

enum
{
	USER_INFO_FETCH = 0xde617,
	USER_BALANCE_MOD = 0xde69,
};

enum
{
	TYPE_ADD = 1,
	TYPE_SUB = 2
};

struct ax_api_client
{
	pthread_t t;
	int socket;
	struct sockaddr_in info;
};

struct _ax_sbs_userInfoFetch
{
	uint64_t zukiewiczoszust;
};

struct _ax_sbs_header
{
	uint32_t size;
	uint32_t type;
};

struct _ax_sbs_userInfoFetchResponse
{
	uint64_t walletCount;
	uint64_t walletIdent[WALLET_MAX];
	uint64_t lowPart[WALLET_MAX];
	uint64_t highPart[WALLET_MAX];
};

struct _ax_sbs_balanceMod
{
	uint64_t user;
	uint64_t wallet_id;
	uint64_t type;
	uint64_t value_high;
	uint64_t value_low;
};

struct _ax_sbs_balanceModResponse
{
	uint64_t result;
};

pthread_t th;

static void _ax_sbs_kazachuj(struct ax_api_client* client, uint8_t* buffer, int len)
{
	int fultyncham;
	struct _ax_sbs_userInfoFetch* dupson = (struct _ax_sbs_userInfoFetch*)buffer;
	ax_user* piramida = ax_useridx_get(dupson->zukiewiczoszust);

	struct _ax_sbs_header h;
	struct _ax_sbs_userInfoFetchResponse r;

	h.size = sizeof(r);
	h.type = USER_INFO_FETCH;

	if (piramida == NULL)
	{
		r.walletCount = 0;

		goto SEND;
	}

	r.walletCount = piramida->wallet_count;

	for (fultyncham = 0; fultyncham < piramida->wallet_count; fultyncham++)
	{
		r.walletIdent[fultyncham] = piramida->wallets[fultyncham].currency_id;
		r.lowPart[fultyncham] = piramida->wallets[fultyncham].balance_low;
		r.highPart[fultyncham] = piramida->wallets[fultyncham].balance_high;
	}

SEND:

	send(client->socket, &h, sizeof(h), 0);
	send(client->socket, &r, sizeof(r), 0);
}

static void _ax_sbs_kazaluj(struct ax_api_client* client, struct _ax_sbs_header* h, uint8_t* buffer)
{
	int ret;
	struct _ax_sbs_balanceMod * dupson = (struct _ax_sbs_balanceMod*)buffer;
	ax_tx_balance_mod* mod = malloc(sizeof(*mod));
	mod->header.lock = (uint64_t)(time(NULL));
	mod->header.nonce = (uint16_t)(rand() % 0xFFFF);
	mod->header.type = dupson->type == TYPE_ADD ? TX_BALANCE_IN : TX_BALANCE_OUT;
	mod->balance_high = dupson->value_high;
	mod->balance_low = dupson->value_low;
	mod->currency_id = dupson->wallet_id;
	mod->user_id = dupson->user;
	ax_node* node = ax_kernel_getSelfNode();
	ax_tx_sign((ax_tx*)mod, node->secret);
	ret = ax_user_commitTran((ax_tx*)mod);

	struct _ax_sbs_header header;
	header.size = sizeof(struct _ax_sbs_balanceModResponse);
	header.type = USER_BALANCE_MOD;
	
	send(client->socket, &header, sizeof(header), 0);

	struct _ax_sbs_balanceModResponse r;
	r.result = ret;
	send(client->socket, &r, sizeof(r), 0);
}

static void _ax_sbs_dispatcher(struct ax_api_client* client, struct _ax_sbs_header* h, uint8_t* buffer)
{
	switch (h->type)
	{
	case USER_INFO_FETCH:
		return _ax_sbs_kazachuj(client, buffer, h->size);
	case USER_BALANCE_MOD:
		return _ax_sbs_kazaluj(client, buffer, h->size);
	default:
		AX_LOG_ERRO("Unsupported operation.");
		abort();
	}

}

static void _ax_sbs_handler(struct ax_api_client* client)
{
	uint8_t buffer[1024 * 64];
	struct _ax_sbs_header header;
	ssize_t received;

	while (1 == 1)
	{
		received = recv(client->socket, &header, sizeof(header), MSG_WAITALL);

		if (received != sizeof(header))
		{
			AX_LOG_WARN("Failed to read packet size header.");

			shutdown(client->socket, SHUT_RDWR);
			close(client->socket);

			free(client);

			return;
		}

		received = recv(client->socket, buffer, header.size, MSG_WAITALL);

		if (received != header.size)
		{
			AX_LOG_WARN("Failed to read packet body.");

			shutdown(client->socket, SHUT_RDWR);
			close(client->socket);

			free(client);

			return;
		}

		_ax_sbs_dispatcher(client, &header, buffer);
	}
}

static void _ax_sbs_acceptor()
{
	struct addrinfo* si = NULL;
	struct addrinfo hints;
	int s;
	int ir;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo("0.0.0.0", "8888", &hints, &si);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
		printf("setsockopt(SO_REUSEADDR) failed");

	ir = bind(s, (struct sockaddr*)si->ai_addr, si->ai_addrlen);
	if (ir != 0)
	{
		AX_LOG_ERRO("Failed to bind.");
		return;
	}

	ir = listen(s, 1024);

	struct sockaddr_in si2;
	int addrlen;

	while (1 == 1)
	{
		addrlen = sizeof(si2);
		int r = accept(s, (struct sockaddr*) & si2, &addrlen);

		if (r != -1)
		{
			struct ax_api_client* param = malloc(sizeof(*param));
			memcpy(&param->info, &si2, sizeof(si2));
			param->socket = r;
			pthread_create(&param->t, NULL, (void* (*)(void*)) &_ax_sbs_handler, param);
		}
		else
		{
			AX_LOG_ERRO("ERROR %d\n", errno);
		}
	}
}

void ax_sbs_init()
{
	pthread_create(&th, NULL, (void* (*)(void*)) & _ax_sbs_acceptor, NULL);
}