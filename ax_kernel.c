#include "ax_kernel.h"

#include "ax_log.h"
#include "ax_user.h"
#include "ax_crypto.h"
#include "ax_block.h"
#include "ax_sbs.h"

#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

ax_kernel GLOBAL;

uint8_t tbl[] = 
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

void hexfmt(uint8_t* in, unsigned int len)
{
	uint8_t* tmp = malloc(1024 * 1024 * 16);
	uint8_t tmp1, tmp2;

	unsigned int i;

	for (i = 0; i < len; i++)
	{
		tmp[i * 2] = tbl[in[i] / 16];
		tmp[i * 2 + 1] = tbl[in[i] % 16];
	}

	if (len == 16)
	{
		for (i = 0; i < 4; i++)
		{
			tmp1 = tmp[i*2];
			tmp2 = tmp[i * 2 + 1];

			tmp[i*2] = tmp[15 - i * 2 - 1];
			tmp[i * 2 + 1] = tmp[15 - i * 2];

			tmp[15 - i * 2 - 1] = tmp1;
			tmp[15 - i * 2] = tmp2;
		}

		for (i = 0; i < 4; i++)
		{
			tmp1 = tmp[16 + i * 2];
			tmp2 = tmp[16 + i * 2 + 1];

			tmp[16 + i * 2] = tmp[16 + 15 - (i * 2) - 1];
			tmp[16 + i * 2 + 1] = tmp[16 + 15 - (i * 2)];

			tmp[16 + 15 - (i * 2) - 1] = tmp1;
			tmp[16 + 15 - (i * 2)] = tmp2;
		}
	}
	
	tmp[len * 2] = 0;
	memcpy(in, tmp, len * 2 + 1);
	free(tmp);
}

static inline uint8_t decodeHex(uint8_t h)
{
	if (h - (uint8_t)'0' < 10)
		return(uint8_t)( h - (uint8_t)'0');
	else
		return (uint8_t)(h - (uint8_t)'a' + 10);
}

void hextobuf(const uint8_t* hex, void* out)
{
	size_t i;
	uint8_t* o = (uint8_t*)out;
	size_t len = strlen(hex);

	for (i = 0; i < len / 2; i++)
	{
		o[i] = (uint8_t)((int)decodeHex(hex[i * 2]) << 4 | (int)decodeHex(hex[i * 2 + 1]));
	}
}

const uint8_t TEST_PK[] = "12345678901234567890123456789012";

ax_kernel* ax_kernel_alloc(void)
{
	memset(&GLOBAL, 0, sizeof(GLOBAL));
	pthread_mutex_init(&GLOBAL.lock, NULL);
	GLOBAL.work = 1;
	GLOBAL.mpm = altyn_mmp_new();
	return &GLOBAL;
}

unsigned int ax_kernel_getCurrentHeight(void)
{
	return (unsigned int)GLOBAL.currentHeight;
}

altyn_mempool* ax_kernel_getMempool(void)
{
	return GLOBAL.mpm;
}

ax_kernel* ax_kernel_get(void)
{
	return &GLOBAL;
}

ax_node* ax_kernel_getSelfNode(void)
{
	return &GLOBAL.self;
}

void ax_kernel_free(ax_kernel** kernel)
{
	//free(*kernel);
	*kernel = NULL;
}

static void _ax_kernel_printHelp(void)
{
	printf(
		"Altyn Blockchain\n"
		"\n"
		"\n"
		"Options:\n"
		"\n"
		"-m / --mine - enable mining\n"
		"-n / --no-p2p - disable p2p networking\n"
		"-h / --help - print this help menu\n"
	);
}

int ax_kernel_processArgs(ax_kernel* kernel, unsigned int count, char** args)
{
	unsigned int i;

	for (i = 0; i < count; i++)
	{
		if (strcmp(args[i], "-n") == 0 || strcmp(args[i], "--no-p2p") == 0)
		{
			AX_LOG_INFO("Disabled P2P networking.");
			kernel->flags = kernel->flags & KERNEL_FLAG_DISABLE_P2P;
		}
		else if (strcmp(args[i], "-m") == 0 || strcmp(args[i], "--mine") == 0)
		{
			AX_LOG_INFO("Enabled mining.");
			kernel->flags = kernel->flags & KERNEL_FLAG_MINE;
		}
		else if (strcmp(args[i], "-h") == 0 || strcmp(args[i], "--help") == 0)
		{
			_ax_kernel_printHelp();
			return -2;
		}
		else
		{
			AX_LOG_CRIT("Unresolved parameter \"%s\".", args[i]);
			return -1;
		}
	}

	return 0;
}

void ax_kernel_getNodePubkey(char* out)
{
	ax_getpub(GLOBAL.self.secret, 32, out);
	char copy[256];
	memcpy(copy, out, 65);
	hexfmt(out, 65);

	uint8_t test[256];
	hextobuf((uint8_t*)out, test);
	AX_LOG_INFO("RESULT %d", memcmp(test, copy, 65));
}

int ax_kernel_worker(ax_kernel* instance)
{
	struct timespec current;
	uint64_t delta;

	ax_node_init(&GLOBAL.self);
	ax_useridx_init();

	ax_sbs_init();

	ax_block_getGenesis();

	if (clock_gettime(0, &instance->last) != 0)
	{
		AX_LOG_CRIT("Failed to fetch system time.");
		return -1 * __LINE__;
	}

	while (instance->work == 1)
	{
		if(clock_gettime(0, &current) != 0)
		{
			AX_LOG_CRIT("Failed to fetch system time.");
			return -1 * __LINE__;
		}
	
		delta = ((uint64_t)current.tv_sec - (uint64_t)instance->last.tv_sec) * 1000000000ULL;
		AX_LOG_TRAC("Delta (%d).", delta);

		if (current.tv_nsec < instance->last.tv_nsec)
		{
			delta -= 1000000000ULL;
			delta = delta + (((uint64_t)current.tv_nsec - (uint64_t)instance->last.tv_nsec));
		}
		else
		{
			delta += (uint64_t)current.tv_nsec - (uint64_t)instance->last.tv_nsec;
		}

		instance->last.tv_sec = current.tv_sec;
		instance->last.tv_nsec = current.tv_nsec;
		
		usleep(1);
	}

	return 0;
}