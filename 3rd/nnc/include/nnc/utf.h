/** \file  utf.h
 *  \brief UTF conversion.
 */
#ifndef inc_nnc_utf_h
#define inc_nnc_utf_h

#include <nnc/base.h>
NNC_BEGIN

/** \brief         Converts a UTF16 string to a UTF8 one.
 *  \note          The NULL terminator is never placed.
 *  \param out     Output UTF8 string. Something like (\p inlen * 4) will always fit the entire output.
 *  \param outlen  Length of \p out.
 *  \param in      Input UTF16 string.
 *  \param inlen   Length of \p in.
 *  \returns       Total size of the converted string, even if buffer was too small.
 */
int nnc_utf16_to_utf8(nnc_u8 *out, int outlen, const nnc_u16 *in, int inlen);

/** \brief         Converts a UTF8 string to a UTF16 one.
 *  \note          The NULL terminator is never placed.
 *  \param out     Output UTF16 string. Something like (\p inlen * 4) will always fit the entire output.
 *  \param outlen  Length of \p out.
 *  \param in      Input UTF8 string.
 *  \param inlen   Length of \p in.
 *  \returns       Total size of the converted string, even if buffer was too small.
 */
int nnc_utf8_to_utf16(nnc_u16 *out, int outlen, const nnc_u8 *in, int inlen);

NNC_END
#endif

