#pragma once
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>

struct ax_p2p_peer
{
	int id;
	int socket;
	uint8_t packetBuffer[1024 * 1024 * 16];
	int packetBufferSize;
	int incomingSize;
	int incomingType;
	struct sockaddr_in info;
	socklen_t infoLen;
};
typedef struct ax_p2p_peer ax_p2p_peer;

struct ax_p2p
{
	int work;
	int acceptorSocket;
	pthread_t acceptor;
	int epollQueue;

	pthread_mutex_t peerLock;
	ax_p2p_peer** peerTable;
	unsigned int peerCountMax;
};
typedef struct ax_p2p ax_p2p;

int ax_p2p_init(ax_p2p* in);