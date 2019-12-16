#include "ax_node.h"
#include "ax_log.h"
#include "ax_crypto.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void ax_node_save(const ax_node* node)
{
	FILE* f = fopen("/node/node.bin", "wb");

	if (f == NULL || fwrite(node, sizeof(*node), 1, f) != 1)
	{
		AX_LOG_CRIT("Failed to save node config for reason %d.", errno);
		abort();
	}

	if (fclose(f) == EOF)
	{
		AX_LOG_ERRO("Failed to close file handle for reason %d.", errno);
	}

	AX_LOG_INFO("Successfully saved node config.");
}

void ax_node_gen(ax_node* out)
{
	if (ax_genrand(out->secret, SECRET_LENGTH) != 0)
	{
		AX_LOG_CRIT("Failed to generate private key for reason %d.", errno);
		abort();
	}

	AX_LOG_INFO("Successfully generated node config.");

	ax_node_save(out);
}

void ax_node_init(ax_node* out)
{
	FILE* f = fopen("/node/node.bin", "rb");

	if (f == NULL)
	{
		AX_LOG_INFO("Could not find node config, generating new.");
		return ax_node_gen(out);
	}

	if (fread(out, sizeof(*out), 1, f) != 1)
	{
		AX_LOG_CRIT("Failed to read node config for reason %d.", errno);
		abort();
	}

	if (fclose(f) == EOF)
	{
		AX_LOG_ERRO("Failed to close file handle for reason %d.", errno);
	}

	AX_LOG_INFO("Successfully loaded node config.");
}