#include <stdio.h>

#include "ax_log.h"
#include "ax_version.h"
#include "ax_api.h"
#include "ax_kernel.h"

int main(int argc, char* argv[])
{
	ax_kernel* kernel = NULL;
	int ret = 0;

	ax_log_init();
	ax_api_init();

	AX_LOG_INFO(
		"Altyn Chain %d.%d.%d | %s %s",
		ALTYN_VERSION_MAJOR,
		ALTYN_VERSION_MINOR,
		ALTYN_VERSION_PATCH,
		__DATE__,
		__TIME__
	);

	if (argc < 1)
	{
		AX_LOG_CRIT("Invalid startup.");
		ret = -1;
		goto CLEAN;
	}

	kernel = ax_kernel_alloc();

	if (kernel == NULL)
	{
		AX_LOG_CRIT("Failed to allocate kernel.");
		ret = -2;
		goto CLEAN;
	}
	
	if ((ret = ax_kernel_processArgs(kernel, (unsigned int)argc - 1 , argv + 1)) != 0)
	{
		if (ret == -1)
		{
			AX_LOG_CRIT("Failed to parse arguments.");
		}

		ret = -3;
		goto CLEAN;
	}

	if (ax_kernel_worker(kernel) != 0)
	{
		AX_LOG_CRIT("Kernel worker exited with error.");
		ret = -4;
		goto CLEAN;
	}

CLEAN:
	if (kernel != NULL) ax_kernel_free(&kernel);

	ax_log_shut();
	
	return ret;
}