
#include "./internal.h"
#include <stdio.h>

// https://en.wikipedia.org/wiki/UTF-8
// https://en.wikipedia.org/wiki/UTF-16
//  i hate utf

static void write_utf8(u8 *out, int outlen, int *outptr, u32 cp)
{
	if(cp < 0x80)
	{
		int n = *outptr + 1;
		if(n <= outlen)
			out[*outptr] = cp;
		*outptr = n;
	}
	else if(cp < 0x800)
	{
		int n = *outptr + 2;
		if(n <= outlen)
		{
			out[*outptr + 0] = (cp >> 6)   | 0xC0;
			out[*outptr + 1] = (cp & 0x3F) | 0x80;
		}
		*outptr = n;
	}
	else if(cp < 0x10000)
	{
		int n = *outptr + 3;
		if(n <= outlen)
		{
			out[*outptr + 0] = (cp >> 12)         | 0xE0;
			out[*outptr + 1] = ((cp >> 6) & 0x3F) | 0x80;
			out[*outptr + 2] = (cp & 0x3F)        | 0x80;
		}
		*outptr = n;
	}
	else if(cp < 0x110000)
	{
		int n = *outptr + 4;
		if(n <= outlen)
		{
			out[*outptr + 0] = (cp >> 18)          | 0xF0;
			out[*outptr + 1] = ((cp >> 12) & 0x3F) | 0x80;
			out[*outptr + 2] = ((cp >> 6) & 0x3F)  | 0x80;
			out[*outptr + 3] = (cp & 0x3F)         | 0x80;
		}
		*outptr = n;
	}
	else { } /* invalid codepoint */
}

static void write_utf16(u16 *out, int outlen, int *outptr, u32 cp)
{
	/* contains invalid codepoints as well, we'll just ignore them */
	if(cp < 0x10000)
	{
		int n = *outptr + 1;
		if(n <= outlen)
			out[*outptr] = cp;
		*outptr = n;
	}
	else if(cp < 0x11000)
	{
		cp &= ~0x10000;
		int n = *outptr + 2;
		if(n <= outlen)
		{
			out[*outptr + 0] = (cp >> 10)   | 0xD800;
			out[*outptr + 1] = (cp & 0x3FF) | 0xDC00;
		}
		*outptr = n;
	}
	else { } /* invalid codepoint */
}

/* should there be a BE version of this? */
int nnc_utf16_to_utf8(u8 *out, int outlen, const u16 *in, int inlen)
{
	int outptr = 0;
	for(int i = 0; i < inlen; ++i)
	{
		u16 p1 = LE16(in[i]);
		if(p1 == '\0')
			break; /* finished */
		else if(p1 < 0xD800 || p1 > 0xE000)
			write_utf8(out, outlen, &outptr, p1);
		/* surrogate pair */
		else
		{
			u16 p2 = LE16(in[i + 1]);
			u16 w1 = p1 & 0x3FF;
			u16 w2 = p2 & 0x3FF;
			u32 cp = 0x10000 | (w1 << 10) | w2;
			write_utf8(out, outlen, &outptr, cp);
			++i; /* since we moved ahead 2 for the pair */
		}
	}
	return outptr;
}

int nnc_utf8_to_utf16(u16 *out, int outlen, const u8 *in, int inlen)
{
	int outptr = 0;
	for(int i = 0; i < inlen; ++i)
	{
		u8 p1 = in[i];
		if(p1 == '\0')
			break; /* finished */
		else if(p1 < 0x80)
		{
			write_utf16(out, outlen, &outptr, in[i]);
			continue;
		}
#define INCCHK(n) if(!((i + n) < inlen)) break
		INCCHK(1);
		if(p1 < 0xE0)
		{
			u32 cp = ((p1 & 0x1F) << 6)
			       | (in[i + 1] & 0x3F);
			write_utf16(out, outlen, &outptr, cp);
			continue;
		}
		INCCHK(2);
		if(p1 < 0xF0)
		{
			u32 cp = ((p1 & 0xF) << 12)
			       | ((in[i + 1] & 0x3F) << 6)
			       | (in[i + 2] & 0x3F);
			write_utf16(out, outlen, &outptr, cp);
			continue;
		}
		INCCHK(3);
		if(p1 < 0xF5)
		{
			u32 cp = ((p1 & 0x7) << 18)
			       | ((in[i + 1] & 0x3F) << 12)
			       | ((in[i + 2] & 0x3F) << 6)
			       | (in[i + 3] & 0x3F);
			write_utf16(out, outlen, &outptr, cp);
			continue;
		}
#undef INCCHK
		/* invalid... */
	}
	return outptr;
}

