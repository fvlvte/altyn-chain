#include "ax_crypto.h"
#include "ax_log.h"

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/rand.h>
#include <string.h>
#include <stdint.h>

void ax_makehash_ctx_init(ax_cryptoctx_sha256* ctx)
{
	SHA256_Init(ctx);
}

void ax_makehash_ctx_update(ax_cryptoctx_sha256* ctx, const void* data, unsigned int len)
{
	SHA256_Update(ctx, data, (size_t)len);
}

void ax_makehash_ctx_final(ax_cryptoctx_sha256* ctx, void* out)
{
	SHA256_Final((unsigned char*)out, ctx);
}

void ax_makehash(const void* buffer, unsigned int length, void* buff)
{
	SHA256_CTX sha256;

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buffer, length);
	SHA256_Final((unsigned char*)buff, &sha256);
}

int ax_makesig
	(
		unsigned char* hash, unsigned int hashLen,
		const uint8_t* priv, unsigned int privLen, uint8_t* sig, unsigned int sigLen
	)
{
	EC_KEY* eckey = NULL;
	BIGNUM* pv = NULL;
	EC_GROUP* ecgroup = NULL;
	ECDSA_SIG* signature = NULL;
	int ret = 0;

	pv = BN_new();

	if (pv == NULL)
	{
		AX_LOG_ERRO("Failed to alloc BN.");
		ret = __LINE__;
		goto CLEANUP;
	}

	eckey = EC_KEY_new();

	if (eckey == NULL)
	{
		AX_LOG_ERRO("Failed to create EC key.");
		ret = __LINE__;
		goto CLEANUP;
	}

	ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);

	if (ecgroup == NULL)
	{
		AX_LOG_ERRO("Failed to create EC group.");
		ret = __LINE__;
		goto CLEANUP;
	}

	if (EC_KEY_set_group(eckey, ecgroup) != 1)
	{
		AX_LOG_ERRO("Failed to bind group to the key.");
		ret = __LINE__;
		goto CLEANUP;
	}

	pv = BN_bin2bn(priv, (int)privLen, pv);
	
	if (EC_KEY_set_private_key(eckey, pv) == 0)
	{
		AX_LOG_ERRO("Failed to bind private key.");
		ret = __LINE__;
		goto CLEANUP;
	}

	signature = ECDSA_do_sign(hash, (int)hashLen, eckey);

	if (signature == NULL)
	{
		AX_LOG_ERRO("Failed to sign message.");
		ret = __LINE__;
		goto CLEANUP;
	}

	BN_bn2bin(signature->r, sig);
	BN_bn2bin(signature->s, sig + 32);

	CLEANUP:

	if(signature != NULL) ECDSA_SIG_free(signature);
	if(ecgroup != NULL) EC_GROUP_free(ecgroup);
	if(eckey != NULL) EC_KEY_free(eckey);
	if(pv != NULL) BN_free(pv);

	return ret;
}

int ax_getpub(const uint8_t* priv, unsigned int privlen, uint8_t* pubout)
{
	EC_POINT* pub = NULL;
	BIGNUM* pv = NULL;
	EC_GROUP* ecgroup = NULL;
	int ret = 0;

	pv = BN_new();

	if (pv == NULL)
	{
		AX_LOG_ERRO("Failed to alloc BN.");
		ret = __LINE__;
		goto CLEANUP;
	}

	ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);

	if (ecgroup == NULL)
	{
		AX_LOG_ERRO("Failed to create EC group.");
		ret = __LINE__;
		goto CLEANUP;
	}

	pv = BN_bin2bn(priv, 32, pv);

	pub = EC_POINT_new(ecgroup);
	EC_POINT_mul(ecgroup, pub, pv, NULL, NULL, NULL);
	if (EC_POINT_point2oct(ecgroup, pub, 4, pubout, 65, NULL) == 0)
	{
		AX_LOG_ERRO("Failed to convert point.");
		ret = __LINE__;
		goto CLEANUP;
	}

CLEANUP:

	if (ecgroup != NULL) EC_GROUP_free(ecgroup);
	if (pv != NULL) BN_free(pv);

	return ret;
}

int ax_verisig(unsigned char* data, unsigned int dataLen, const uint8_t* pubkey, uint8_t* sig)
{
	EC_KEY* eckey = NULL;
	EC_GROUP* ecgroup = NULL;
	ECDSA_SIG* signature = NULL;
	EC_POINT* pub = NULL;
	
	int ret = 0;

	eckey = EC_KEY_new();

	if (eckey == NULL)
	{
		AX_LOG_ERRO("Failed to create EC key.");
		ret = __LINE__;
		goto CLEANUP;
	}

	signature = ECDSA_SIG_new();

	ecgroup = EC_GROUP_new_by_curve_name(NID_secp256k1);

	if (ecgroup == NULL)
	{
		AX_LOG_ERRO("Failed to create EC group.");
		ret = __LINE__;
		goto CLEANUP;
	}

	if (EC_KEY_set_group(eckey, ecgroup) != 1)
	{
		AX_LOG_ERRO("Failed to bind group to the key.");
		ret = __LINE__;
		goto CLEANUP;
	}

	pub = EC_POINT_new(ecgroup);
	if(EC_POINT_oct2point(ecgroup, pub, pubkey, 65, NULL) == 0)
	{
		AX_LOG_ERRO("Failed to convert out point.");
		ret = __LINE__;
		goto CLEANUP;
	}

	EC_KEY_set_public_key(eckey, pub);

	signature->r = BN_bin2bn(sig, 32, NULL);
	signature->s = BN_bin2bn(sig+32, 32, NULL);

	if (ECDSA_do_verify(data, (int)dataLen, signature, eckey) != 1)
	{
		AX_LOG_ERRO("Failed to verify signature.");
		ret = __LINE__;
		goto CLEANUP;
	}

CLEANUP:

	if (ecgroup != NULL) EC_GROUP_free(ecgroup);
	if (eckey != NULL) EC_KEY_free(eckey);

	return ret;
}

int ax_genrand(uint8_t* out, unsigned int size)
{
	if (RAND_bytes(out, (int)size) != 1)
		return __LINE__;

	return 0;
}