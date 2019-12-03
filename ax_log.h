#pragma once

#include <stdio.h>

enum
{
	AX_LOG_TRACE = 0,
	AX_LOG_INFO,
	AX_LOG_WARN,
	AX_LOG_ERROR,
	AX_LOG_CRITICAL
};

int ax_log_init(void);
void ax_log_shut(void);
void ax_log(int level, unsigned int line, const char* file, const char* fmt, ...);

#define AX_LOG(level, fmt, ...) ax_log(level, __LINE__, __FILE__, fmt "\n", ##__VA_ARGS__)

#define AX_LOG_TRAC(fmt, ...) AX_LOG(AX_LOG_TRACE, fmt, ##__VA_ARGS__)
#define AX_LOG_INFO(fmt, ...) AX_LOG(AX_LOG_INFO, fmt, ##__VA_ARGS__)
#define AX_LOG_WARN(fmt, ...) AX_LOG(AX_LOG_WARN, fmt, ##__VA_ARGS__)
#define AX_LOG_ERRO(fmt, ...) AX_LOG(AX_LOG_ERROR, fmt, ##__VA_ARGS__)
#define AX_LOG_CRIT(fmt, ...) AX_LOG(AX_LOG_CRITICAL, fmt, ##__VA_ARGS__)