#include "ax_api.h"
#include "ax_kernel.h"
#include "ax_log.h"

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
#include "ax_tx.h"

#include <string.h>

struct ax_api_client
{
	pthread_t t;
	int socket;
	struct sockaddr_in info;
};

pthread_t th;

static void _ax_api_sendResponse(struct ax_api_client* client, const uint8_t* res, unsigned int length)
{
	const char* header = "HTTP/1.1 200 OK\r\n"
		"Server: Altyn 1.0\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %u\r\n"
		"\r\n";
	char headerBuffer[256];
	
	sprintf(headerBuffer, header, length);

	send(client->socket, headerBuffer, strlen(headerBuffer), 0);
	send(client->socket, res, length, 0);
}

static void _ax_api_sendResponseString(struct ax_api_client* client, const char* str)
{
	_ax_api_sendResponse(client, str, (unsigned int)strlen(str));
}

static void _ax_apiGetCurrentHeight(struct ax_api_client* client, uint8_t* path, uint8_t* param)
{
	char responseBuffer[256];
	sprintf(responseBuffer, "{\"height\":%u}", ax_kernel_getCurrentHeight());
	_ax_api_sendResponseString(client, responseBuffer);
}

static void _ax_apiGetNodePublicKey(struct ax_api_client* client, uint8_t* path, uint8_t* param)
{
	char responseBuffer[256];
	char pubKey[256];

	memset(responseBuffer, 0, 256);
	memset(pubKey, 0, 256);

	ax_kernel_getNodePubkey(pubKey);

	sprintf(responseBuffer, "{\"key\":\"0x%s\"}", pubKey);

	_ax_api_sendResponseString(client, responseBuffer);
}

static void _ax_apiBroadcastTx(struct ax_api_client* client, uint8_t* path, uint8_t* param)
{
	if (param == NULL)
	{
		return _ax_api_sendResponseString(client, "Invalid transaction.");
	}
	int txLen = (int)strlen(param) / 2;
	char txbuff[512];

	hextobuf(param, txbuff);

	void* txentry = malloc((size_t)txLen);
	memcpy(txentry, txbuff, (size_t)txLen);

	if (txLen != ax_tx_getSize((ax_tx*)txbuff))
	{
		return _ax_api_sendResponseString(client, "Invalid transaction.");
	}
	int ret = altyn_mmp_push(ax_kernel_getMempool(), (ax_tx*)txentry);

	if (ret == -1)
	{
		return _ax_api_sendResponseString(client, "Already in pool.");
	}

	if (ret != 0)
	{
		return _ax_api_sendResponseString(client, "Invalid MAC.");
	}
	
	return _ax_api_sendResponseString(client, "{\"status\":0}");
}

static void _ax_apiGetSignedTx(struct ax_api_client* client, uint8_t* path, uint8_t* param)
{
	uint8_t* currncy = param;
	uint8_t* user = NULL;
	uint8_t* value = NULL;

	char *out = malloc(256);

	int iuser, ivalue, icurrency;

	while (*param != '$')
		param++;

	*param++ = 0;
	user = param;

	while (*param != '$')
		param++;

	*param++ = 0;
	value = param;

	iuser = atoi(user);
	icurrency = atoi(currncy);
	ivalue = atoi(value);

	ax_tx_balance_mod* mod = malloc(sizeof(*mod));
	mod->header.lock = (uint64_t)(time(NULL));
	mod->header.nonce = (uint16_t)(rand() % 0xFFFF);
	mod->header.type = TX_BALANCE_IN;
	mod->balance_high = 0;
	mod->balance_low = (uint64_t)ivalue;
	mod->currency_id = (uint16_t)icurrency;
	mod->user_id = (uint32_t)iuser;
	ax_node* node = ax_kernel_getSelfNode();
	ax_tx_sign((ax_tx*)mod, node->secret);
	memcpy(out, mod, sizeof(*mod));
	hexfmt(out, sizeof(ax_tx_balance_mod));

	AX_LOG_TRAC("%d, %d, %d", iuser, ivalue, icurrency);

	altyn_mmp_push(ax_kernel_getMempool(), (ax_tx*)mod);

	_ax_api_sendResponseString(client, out);
	free(out);
}

static void _ax_apiGetMempool(struct ax_api_client* client, uint8_t* path, uint8_t* param)
{
	char* out = malloc(1024 * 1024 * 16);
	int sz;

	sz = altyn_mmp_get(ax_kernel_getMempool(), out, 1024 * 1024 * 16);
	out[sz] = 0x00;
	hexfmt(out, (unsigned int)sz);

	_ax_api_sendResponseString(client, out);

	free(out);
}

static void _ax_api_dispatcher(struct ax_api_client* client, uint8_t* buffer, int len)
{
	uint8_t* copy;
	uint8_t* param = NULL;

	while (*buffer != ' ' && --len > 0)
		buffer++;
	buffer++;

	if (len < 1)
		return;

	copy = buffer;

	while ((*copy != ' ' && *copy != '$') && --len > 0)
		copy++;

	if (len < 1)
		return;

	if (*copy == '$')
	{
		*copy++ = 0x00;
		param = copy;
	}

	while ((*copy != ' ') && --len > 0)
		copy++;

	*copy = 0x00;

	AX_LOG_TRAC("Path %s", buffer);
	if(param != NULL)
		AX_LOG_TRAC("Parma \"%s\"", param);

	if (strcmp(buffer, "/height") == 0)
		return _ax_apiGetCurrentHeight(client, buffer, param);
	else if (strcmp(buffer, "/node/info") == 0)
		return _ax_apiGetNodePublicKey(client, buffer, param);
	else if (strcmp(buffer, "/tx/balance/add") == 0)
		return _ax_apiGetSignedTx(client, buffer, param);
	else if (strcmp(buffer, "/mempool") == 0)
		return _ax_apiGetMempool(client, buffer, param);
	else if (strcmp(buffer, "/tx/broadcast") == 0)
		return _ax_apiBroadcastTx(client, buffer, param);
	else
	{
		_ax_api_sendResponseString(client, "pizdziec");
	}
}


static void _ax_api_receiver(struct ax_api_client* client)
{
	uint8_t buffer[1024 * 64];
	ssize_t received;

	while (1 == 1)
	{
		received = recv(client->socket, buffer, (ssize_t)1024 * 64, 0);

		if (received < 1)
		{
			shutdown(client->socket, SHUT_RDWR);
			close(client->socket);

			free(client);

			return;
		}

		buffer[received] = 0x00;

		_ax_api_dispatcher(client, buffer, (int)received);
	}
}

void _ax_api_acceptor()
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

	getaddrinfo("0.0.0.0", "8080", &hints, &si);

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
			pthread_create(&param->t, NULL, (void* (*)(void*) )&_ax_api_receiver, param);
		}
		else
		{
			AX_LOG_ERRO("ERROR %d\n", errno);
		}
	}
}

void ax_api_init()
{
	pthread_create(&th, NULL, (void* (*)(void*))&_ax_api_acceptor, NULL);
}