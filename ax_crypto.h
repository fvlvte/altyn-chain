#pragma once

#include <stdint.h>
#include <openssl/sha.h>

typedef SHA256_CTX ax_cryptoctx_sha256;

void ax_makehash_ctx_init(ax_cryptoctx_sha256* ctx);
void ax_makehash_ctx_update(ax_cryptoctx_sha256* ctx, const void* data, unsigned int len);
void ax_makehash_ctx_final(ax_cryptoctx_sha256* ctx, void* out);

void ax_makehash(const void* buffer, unsigned int length, void* buff);
int ax_makesig
(
	unsigned char* hash, unsigned int hashLen,
	const uint8_t* priv, unsigned int privLen, uint8_t* sig, unsigned int sigLen
);
int ax_verisig(unsigned char* data, unsigned int dataLen, const uint8_t* pubkey, uint8_t* sig);
int ax_getpub(const uint8_t* priv, unsigned int privlen, uint8_t* pubout);
int ax_genrand(uint8_t* out, unsigned int size);