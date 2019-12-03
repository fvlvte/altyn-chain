#include "ax_p2p.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "ax_log.h"

static int _ax_p2p_addPeer(ax_p2p* in, ax_p2p_peer* peer)
{
	int i;
	struct epoll_event ev;

	ev.data.ptr = peer;
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;

	pthread_mutex_lock(&in->peerLock);

	for (i = 0; i < in->peerCountMax; i++)
	{
		if (in->peerTable[i] == NULL)
		{
			in->peerTable[i] = peer;
			peer->id = i;
			pthread_mutex_unlock(&in->peerLock);
			return 0;
		}
	}

	epoll_ctl(in->epollQueue, EPOLL_CTL_ADD, peer->socket, &ev);

	pthread_mutex_unlock(&in->peerLock);

	AX_LOG_ERRO("Maximum number of peers already connected (%d).", in->peerCountMax);

	return __LINE__;
}

static void _ax_p2p_removePeer(ax_p2p* in, ax_p2p_peer* p)
{
	struct epoll_event ev;

	ev.data.ptr = p;
	ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;

	pthread_mutex_lock(&in->peerLock);

	in->peerTable[p->id] = NULL;

	epoll_ctl(in->epollQueue, EPOLL_CTL_DEL, p->socket, &ev);

	pthread_mutex_unlock(&in->peerLock);

	p->id = -1;
}

static void _ax_p2p_acceptor(ax_p2p* in)
{
	ax_p2p_peer* peer;
	int result;

	while (in->work == 1)
	{
		peer = malloc(sizeof(*peer));
		peer->infoLen = sizeof(peer->info);
		if ((peer->socket = accept(in->acceptorSocket, (struct sockaddr*)&peer->info, &peer->infoLen)) == -1)
		{
			AX_LOG_ERRO("Failed to accept connection for reason %d.", errno);
			goto CLEANUP;
		}

		if ((result = _ax_p2p_addPeer(in, peer)) != 0)
		{
			shutdown(peer->socket, SHUT_RDWR);
			close(peer->socket);
			AX_LOG_ERRO("Failed to add peer for reason %d.", result);
			goto CLEANUP;
		}

		continue;

	CLEANUP:
		free(peer);
	}
}

static int _ax_p2p_dispatch(ax_p2p* in, ax_p2p_peer* peer)
{
	return 0;
}

static void _ax_p2p_worker(ax_p2p* in)
{
	struct epoll_event ev;
	int r;
	ax_p2p_peer* p;

	while (in->work == 1)
	{
		if ((r = epoll_wait(in->epollQueue, &ev, 1, -1) == 1))
		{
			p = (ax_p2p_peer*)ev.data.ptr;
			
			if (ev.events & EPOLLIN)
			{
				if (p->incomingSize == 0)
				{
					if (recv(p->socket, &p->incomingSize, 8, 0) != 8 || p->incomingSize > 1024 * 1024 * 16)
					{
						AX_LOG_ERRO("Peer (%d) header mismatch.", p->id);
						_ax_p2p_removePeer(in, p);
						free(p);
						continue;
					}
				}
				else
				{
					if ((r = (int)recv(p->socket, p->packetBuffer, (size_t)p->incomingSize, 0)) != p->incomingSize)
					{
						p->incomingSize -= r;
						continue;
					}
					else
					{
						if ((r = _ax_p2p_dispatch(in, p)) != 0)
						{
							AX_LOG_ERRO("Peer (%d) dispatch failure (%d).", p->id, r);
							_ax_p2p_removePeer(in, p);
							free(p);
							continue;
						}
					}
				}
				
			}
			else
			{
				AX_LOG_ERRO("Peer (%d) disconnected.", p->id);
				_ax_p2p_removePeer(in, p);
				free(p);
				continue;
			}
		}
	}
}

int ax_p2p_init(ax_p2p* in)
{
	struct addrinfo* si = NULL;
	struct addrinfo hints;
	int result;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if((result = getaddrinfo("0.0.0.0", "16996", &hints, &si)) != 0)
	{
		AX_LOG_ERRO("Failed to resolve address info for reason %d.", result);
		return __LINE__;
	}

	if (setsockopt(in->acceptorSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1)
	{
		AX_LOG_ERRO("Failed to set socket options for reason %d.", errno);
		return __LINE__;
	}

	if (bind(in->acceptorSocket, si->ai_addr, si->ai_addrlen) == -1)
	{
		AX_LOG_ERRO("Failed to bind acceptor socket to %d.", errno);
		return __LINE__;
	}

	if ((result = in->acceptorSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		AX_LOG_ERRO("Failed to create socket for reason %d.", errno);
		return __LINE__;
	}

	if (pthread_mutex_init(&in->peerLock, NULL) != 0)
	{
		AX_LOG_ERRO("Failed to initialize mutex for reason %d.", errno);
		close(in->acceptorSocket);
		return __LINE__;
	}

	in->peerTable = calloc(1024, sizeof(void*));
	in->peerCountMax = 1024;

	if (pthread_create(&in->acceptor, NULL, (void*(*)(void*))&_ax_p2p_acceptor, in) != 0)
	{
		AX_LOG_ERRO("Failed to start acceptor thread for reason %d.", errno);
		pthread_mutex_destroy(&in->peerLock);
		close(in->acceptorSocket);
		free(in->peerTable);
		return __LINE__;
	}

	if (pthread_create(&in->acceptor, NULL, (void* (*)(void*)) &_ax_p2p_worker, in) != 0)
	{
		AX_LOG_ERRO("Failed to start acceptor thread for reason %d.", errno);
		pthread_mutex_destroy(&in->peerLock);
		close(in->acceptorSocket);
		free(in->peerTable);
		return __LINE__;
	}

	return 0;
}