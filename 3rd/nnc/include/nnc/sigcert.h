/** \file  sigcert.h
 *  \brief Signature & certificate chain parsing code that is re-used throughout several 3ds formats.
 */
#ifndef inc_nnc_sigcert_h
#define inc_nnc_sigcert_h
#include <nnc/read-stream.h>
#include <nnc/base.h>
NNC_BEGIN

enum nnc_sigtype {
	NNC_SIG_RSA_4096_SHA1   = 0, ///< RSA 4096 with SHA1 (unused).
	NNC_SIG_RSA_2048_SHA1   = 1, ///< RSA 2048 with SHA1 (unused).
	NNC_SIG_ECDSA_SHA1      = 2, ///< Elliptic Curve with SHA1 (unused).
	NNC_SIG_RSA_4096_SHA256 = 3, ///< RSA 4096 with SHA256.
	NNC_SIG_RSA_2048_SHA256 = 4, ///< RSA 2048 with SHA256.
	NNC_SIG_ECDSA_SHA256    = 5, ///< Elliptic Curve with SHA256.
};

typedef struct nnc_signature {
	enum nnc_sigtype type; ///< Signature type.
	nnc_u8 data[0x200];    ///< Signature data (not always fully in use).
	char issuer[0x41];     ///< Signature issuer (NULL-terminated).
} nnc_signature;

/** \brief      Gets the signature size from the type.
 *  \return     Returns total signature size (<u>including</u> the 4 identifying bytes) or 0 if invalid.
 *  \param sig  Signature type.
 */
nnc_u16 nnc_sig_size(enum nnc_sigtype sig);

/** \brief      Reads entire signature and seeks to end
 *  \param rs   Stream to read signature from.
 *  \param sig  Output signature.
 */
nnc_result nnc_read_sig(nnc_rstream *rs, nnc_signature *sig);

/** \brief      Signature type to string
 *  \return     String version of signature type or NULL if invalid.
 *  \param sig  Signature type.
 */
const char *nnc_sigstr(enum nnc_sigtype sig);

NNC_END
#endif

