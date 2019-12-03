#pragma once
#include <stdint.h>

#define SECRET_LENGTH SECURITY_BITS / 8

struct ax_node
{
	uint8_t secret[SECRET_LENGTH];
};
typedef struct ax_node ax_node;

void ax_node_init(ax_node* out);