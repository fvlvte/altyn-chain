#include "ax_log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

const char* LEVEL_TABLE[] = {
	"TRAC",
	"INFO",
	"WARN",
	"ERRO",
	"CRIT"
};

pthread_mutex_t LOG_LOCK;

int ax_log_init(void)
{
	if (pthread_mutex_init(&LOG_LOCK, NULL) == 0)
		return 0;
	return __LINE__;
}

void ax_log(int level, unsigned int line, const char* file, const char* fmt, ...)
{
	if (level < LOG_SEVERITY)
		return;

	char buffer[26];
	int millisec;
	struct tm* tm_info;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	millisec = (int)lrint((double)tv.tv_usec / 1000.0);
	if (millisec >= 1000)
	{
		millisec -= 1000;
		tv.tv_sec++;
	}

	tm_info = localtime(&tv.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
	va_list args;
	va_start(args, fmt);

	pthread_mutex_lock(&LOG_LOCK);
	printf("[%s.%03d] <%s %s %d> ", buffer, millisec, LEVEL_TABLE[level], file, line);
	vprintf(fmt, args);
	pthread_mutex_unlock(&LOG_LOCK);

	va_end(args);
}

void ax_log_shut(void)
{
	pthread_mutex_destroy(&LOG_LOCK);
}