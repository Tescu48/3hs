
#include <nnc/sigcert.h>
#include "./internal.h"

#define SIGN_MAX 5

static u16 size_lut[0x6] = { 0x200, 0x100, 0x3C, 0x200, 0x100, 0x3C };
static u16 pad_lut[0x6]  = { 0x3C,  0x3C,  0x40, 0x3C,  0x3C,  0x40 };


u16 nnc_sig_size(enum nnc_sigtype sig)
{
	if(sig > SIGN_MAX) return 0;
	return size_lut[sig] + pad_lut[sig] + 0x04;
}

result nnc_read_sig(rstream *rs, nnc_signature *sig)
{
	result ret;
	u8 signum[4];
	TRY(read_exact(rs, signum, sizeof(u32)));
	if(signum[0] != 0x00 || signum[1] != 0x01 || signum[2] != 0x00 || signum[3] > SIGN_MAX)
		return NNC_R_INVALID_SIG;
	sig->type = signum[3];
	TRY(read_exact(rs, sig->data, size_lut[sig->type]));
	TRY(NNC_RS_PCALL(rs, seek_rel, pad_lut[sig->type]));
	TRY(read_exact(rs, (u8 *) sig->issuer, 0x40));
	sig->issuer[0x40] = '\0';
	return NNC_R_OK;
}

const char *nnc_sigstr(enum nnc_sigtype sig)
{
	switch(sig)
	{
	case NNC_SIG_RSA_4096_SHA1:
		return "RSA 4096 - SHA1";
	case NNC_SIG_RSA_2048_SHA1:
		return "RSA 2048 - SHA1";
	case NNC_SIG_ECDSA_SHA1:
		return "Elliptic Curve - SHA1";
	case NNC_SIG_RSA_4096_SHA256:
		return "RSA 4096 - SHA256";
	case NNC_SIG_RSA_2048_SHA256:
		return "RSA 2048 - SHA256";
	case NNC_SIG_ECDSA_SHA256:
		return "Elliptic Curve - SHA256";
	}
	return NULL;
}

