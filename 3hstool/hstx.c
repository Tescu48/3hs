
/** HSTX writer reference implementation
 *   for formal definition see doc/hstx.html
 */

#include <stdbool.h> /* bool is a built-in type in C++ ... */
#include "../include/update.hh" /* contains information for the target_version field */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "./stb_image.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#ifndef __BYTE_ORDER__
	#error "__BYTE_ORDER__ is not defined!"
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define U16(a) (__builtin_bswap16(a))
	#define U32(a) (__builtin_bswap32(a))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#define U16(a) (a)
	#define U32(a) (a)
#else
	#error "unknown __BYTE_ORDER__ value!"
#endif

#define VERSION_INT ((VERSION_MAJOR << 20) | (VERSION_MINOR << 10) | (VERSION_PATCH))
#define FORMAT_INT  (0)

#define DESCRIPTOR_MAX (128)

static void trim(char **val)
{
	while(**val == ' ')
		++*val;
	char *end = *val + strlen(*val);
	while(--end != *val)
	{
		if(*end == ' ')
			*end = '\0';
		else break;
	}
}

static bool getcolor(const char *str, u32 *out)
{
	int base = 0;
	if(*str == '#') { base = 16; ++str; }
	char *end;
	errno = 0;
	unsigned long res = strtoul(str, &end, base);
	*out = U32(res & 0xFFFFFFFF);
	return end == str + strlen(str) && errno != ERANGE;
}

struct dynbuf {
	u8 *data;
	u32 size;
	u32 pos;
};

bool dynbuf_cpy_alloc(struct dynbuf *blob, void *data, u32 len)
{
	u32 npos = blob->pos + len;
	if(npos >= blob->size)
	{
		blob->size += npos;
		blob->size *= 2;
		blob->data = realloc(blob->data, blob->size);
		if(!blob->data) return false;
	}
	memcpy(&blob->data[blob->pos], data, len);
	blob->pos = npos;
	return true;
}

static int make_hstx_impl(FILE *out, char *in_buf, struct dynbuf *blob, const char *inpath)
{
	struct hstx_header {
		char magic[4];
		u32 format_version;
		u32 target_version;
		u32 num_descriptors;
		u32 blob_size;
		u32 name_offset;
		u32 author_offset;
		u32 reserved0[5];
	} header;
	assert(sizeof(header) == 0x30 && "header not properly packed");
	struct hstx_descriptor {
		u32 ident;
		union hstx_descriptor_data {
			struct hstx_descriptor_data_color {
				u32 value;
				u32 reserved1[2];
			} color;
			struct hstx_descriptor_data_image {
				u32 img_ptr;
				u16 w;
				u16 h;
				u32 reserved2;
			} image;
		} data;
	} descriptors[DESCRIPTOR_MAX];
	assert(sizeof(descriptors[0]) == 0x10 && "descriptor not properly packed");
	enum hstx_ident {
		ID_BG_CLR            = 0x1001,
		ID_TEXT_CLR          = 0x1002,
		ID_BTN_BG_CLR        = 0x1003,
		ID_BTN_BORDER_CLR    = 0x1004,
		ID_BATTERY_GREEN_CLR = 0x1005,
		ID_BATTERY_RED_CLR   = 0x1006,
		ID_TOGGLE_GREEN_CLR  = 0x1007,
		ID_TOGGLE_RED_CLR    = 0x1008,
		ID_TOGGLE_SLID_CLR   = 0x1009,
		ID_PROGBAR_FG_CLR    = 0x1010,
		ID_PROGBAR_BG_CLR    = 0x1011,
		ID_SCROLLBAR_CLR     = 0x1012,
		ID_LED_GREEN_CLR     = 0x1013,
		ID_LED_RED_CLR       = 0x1014,
		ID_SMDH_BORDER_CLR   = 0x1015,

		ID_MORE_IMG          = 0x2001,
		ID_BATTERY_IMG       = 0x2002,
		ID_SEARCH_IMG        = 0x2003,
		ID_SETTINGS_IMG      = 0x2004,
		ID_SPINNER_IMG       = 0x2005,
		ID_RANDOM_IMG        = 0x2006,
	};


	memset(descriptors, 0, sizeof(descriptors));

	header.name_offset = U32(0);
	header.author_offset = U32(0);

	u32 num_descriptors = 0;
	char *next_nl = NULL;
	for(int i = 0; i < DESCRIPTOR_MAX && in_buf; ++i)
	{
		while(*in_buf == '\n' || *in_buf == ' ')
			++in_buf;
		if(!*in_buf) break; /* done */
		if((next_nl = strchr(in_buf, '\n')))
			*next_nl = '\0';
		if(*in_buf == '#') goto next;

		char *key = in_buf;
		char *value = strchr(in_buf, '=');
		if(!value)
		{
			fprintf(stderr, "trailing data in configuration file!\n");
			return 1;
		}
		*value = '\0';
		++value;
		trim(&key);
		trim(&value);

		if(strcmp(key, "name") == 0)
		{
			header.name_offset = U32(blob->pos);
			if(!dynbuf_cpy_alloc(blob, value, strlen(value) + 1))
			{
				fprintf(stderr, "failed to allocate additional blob data for name!\n");
				return 1;
			}
		}
		else if(strcmp(key, "author") == 0)
		{
			header.author_offset = U32(blob->pos);
			if(!dynbuf_cpy_alloc(blob, value, strlen(value) + 1))
			{
				fprintf(stderr, "failed to allocate additional blob data for author!\n");
				return 1;
			}
		}
#define DEFIDESC(desc_key, desc_ident) \
		else if(strcmp(key, desc_key) == 0) \
		{ \
			int w, h; \
			char *path = malloc(strlen(inpath) + strlen(value) + 1); \
			if(!path) \
			{ \
				fprintf(stderr, "failed to allocate memory for path!\n"); \
				return 1; \
			} \
			char *slash = strrchr(inpath, '/'); \
			int len = 1; \
			if(slash) len = slash - inpath; \
			else      inpath = "./"; \
			sprintf(path, "%.*s/%s", len, inpath, value); \
			u32 *data = (u32 *) stbi_load(path, &w, &h, NULL, 4); \
			if(!data) \
			{ \
				fprintf(stderr, "failed to open image %s: %s\n", path, stbi_failure_reason()); \
				free(path); \
				return 1; \
			} \
			if(w > 400 || h > 360) \
			{ \
				fprintf(stderr, "image too large %s\n", path); \
				free(path); \
				return 1; \
			} \
			free(path); \
			descriptors[num_descriptors].data.image.img_ptr = U32(blob->pos); \
			if(!dynbuf_cpy_alloc(blob, data, w * h * 4)) \
			{ \
				fprintf(stderr, "failed to allocate additional blob data for image!\n"); \
				free(data); \
				return 1; \
			} \
			descriptors[num_descriptors].ident = U32(desc_ident); \
			descriptors[num_descriptors].data.image.w = U16(w); \
			descriptors[num_descriptors].data.image.h = U16(h); \
			++num_descriptors; \
			free(data); \
		}
#define DEFCDESC(desc_key, desc_ident) \
	else if(strcmp(key, desc_key) == 0) \
	{ \
		if(num_descriptors == DESCRIPTOR_MAX) \
		{ \
			fprintf(stderr, "too many descriptors!\n"); \
			return 1; \
		} \
		descriptors[num_descriptors].ident = U32(desc_ident); \
		if(!getcolor(value, &descriptors[num_descriptors].data.color.value)) \
		{ \
			fprintf(stderr, desc_key ": failed to parse color '%s'\n", value); \
			return 1; \
		} \
		++num_descriptors; \
	}
		DEFCDESC("background_color", ID_BG_CLR)
		DEFCDESC("text_color", ID_TEXT_CLR)
		DEFCDESC("button_background_color", ID_BTN_BG_CLR)
		DEFCDESC("button_border_color", ID_BTN_BORDER_CLR)
		DEFCDESC("battery_good_color", ID_BATTERY_GREEN_CLR)
		DEFCDESC("battery_bad_color", ID_BATTERY_RED_CLR)
		DEFCDESC("toggle_enabled_color", ID_TOGGLE_GREEN_CLR)
		DEFCDESC("toggle_disabled_color", ID_TOGGLE_RED_CLR)
		DEFCDESC("toggle_slider_color", ID_TOGGLE_SLID_CLR)
		DEFCDESC("progress_bar_foreground_color", ID_PROGBAR_FG_CLR)
		DEFCDESC("progress_bar_background_color", ID_PROGBAR_BG_CLR)
		DEFCDESC("scrollbar_color", ID_SCROLLBAR_CLR)
		DEFCDESC("led_success", ID_LED_GREEN_CLR)
		DEFCDESC("led_failure", ID_LED_RED_CLR)
		DEFCDESC("smdh_icon_border_color", ID_SMDH_BORDER_CLR)
		DEFIDESC("more_image", ID_MORE_IMG)
		DEFIDESC("battery_image", ID_BATTERY_IMG)
		DEFIDESC("search_image", ID_SEARCH_IMG)
		DEFIDESC("settings_image", ID_SETTINGS_IMG)
		DEFIDESC("spinner_image", ID_SPINNER_IMG)
		DEFIDESC("random_image", ID_RANDOM_IMG)
#undef DEFCDESC
#undef DEFIDESC
	else
	{
		fprintf(stderr, "unrecognised key: %s (`%s')\n", key, value);
		return 1;
	}

next:
		in_buf = next_nl ? next_nl + 1 : NULL;
	}

	memcpy(header.magic, "HSTX", 4);
	header.format_version = U32(FORMAT_INT);
	header.target_version = U32(VERSION_INT);
	header.num_descriptors = U32(num_descriptors);
	header.blob_size = U32(blob->pos);
	memset(header.reserved0, 0, sizeof(header.reserved0));

	puts("HSTX generator for " VVERSION " \"" VERSION_DESC "\", format " INT_TO_STR(FORMAT_INT));

	if(fwrite(&header, 0x30, 1, out) != 1)
	{
		fprintf(stderr, "failed to write header to output file!\n");
		return 1;
	}
	if(fwrite(descriptors, 0x10 * num_descriptors, 1, out) != 1)
	{
		fprintf(stderr, "failed to write descriptors to output file!\n");
		return 1;
	}
	if(fwrite(blob->data, 1, blob->pos, out) != blob->pos)
	{
		fprintf(stderr, "failed to write descriptors to output file!\n");
		return 1;
	}

	return 0;
}

int make_hstx(const char *output, const char *cfgfile)
{
	FILE *out = NULL, *in = NULL;
	char *in_buf = NULL;
	int ret = 1;
	struct dynbuf blob = {
		.data = malloc(1000),
		.size = 1000,
		.pos  = 1,
	};
	if(!blob.data)
		goto out;
	blob.data[0] = 0;
	out = fopen(output, "w");
	if(!out)
	{
		fprintf(stderr, "%s: failed to open\n", output);
		goto out;
	}
	in = fopen(cfgfile, "r");
	if(!in)
	{
		fprintf(stderr, "%s: failed to open\n", cfgfile);
		goto out;
	}
	fseek(in, 0, SEEK_END);
	long in_size = ftell(in);
	fseek(in, 0, SEEK_SET);
	in_buf = malloc(in_size + 1);
	if(!in_buf)
	{
		fprintf(stderr, "failed to allocate for input buffer!\n");
		goto out;
	}
	if(fread(in_buf, 1, in_size, in) != in_size)
	{
		fprintf(stderr, "failed to read %ld bytes from input file!\n", in_size);
		goto out;
	}
	in_buf[in_size] = '\0';
	ret = make_hstx_impl(out, in_buf, &blob, cfgfile);
out:
	free(blob.data);
	free(in_buf);
	fclose(out);
	fclose(in);
	return ret;
}

