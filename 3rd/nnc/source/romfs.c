
#include <nnc/romfs.h>
#include <nnc/utf.h>
#include <string.h>
#include <stdlib.h>
#include "./internal.h"

#define INVAL 0xFFFFFFFF /* aka UINT32_MAX */
#define MAX_PATH 1024    /* no good rationale for this specific limit except it looking nice */


result nnc_read_romfs_header(rstream *rs, nnc_romfs_header *romfs)
{
	/* in reality this is only 0x5C & 0x28 bytes, but we need to align reads */
	u8 ivfc_header[0x60], l3_header[0x30];
	result ret;
	TRY(read_exact(rs, ivfc_header, sizeof(ivfc_header)));

	if(memcmp(ivfc_header, "IVFC\x00\x00\x01\x00", 8) != 0)
		return NNC_R_CORRUPT;
	u32 master_hash_size = LE32P(&ivfc_header[0x08]);
	/* u32 block_size = pow(2, LE32P(&ivfc_header[0x4C])) */
	u32 block_size = 1 << LE32P(&ivfc_header[0x4C]);
	/* for some reason the 0x4 padding after the ivfc_header is
	 * not documented on 3dbrew but it's definitely there */
	nnc_u32 l3_offset = 0x60 + master_hash_size;
	l3_offset = ALIGN(l3_offset, block_size);

	TRY(read_at_exact(rs, l3_offset, l3_header, sizeof(l3_header)));
	/* header size should always be 0x28 LE */
	if(memcmp(l3_header, "\x28\x00\x00\x00", 4) != 0)
		return NNC_R_CORRUPT;

#define OFLEN_PAIR(st, of) st.offset = ((u64) U32P(&l3_header[of])) + l3_offset; st.length = U32P(&l3_header[of + 0x04])
	OFLEN_PAIR(romfs->dir_hash, 0x04);
	OFLEN_PAIR(romfs->dir_meta, 0x0C);
	OFLEN_PAIR(romfs->file_hash, 0x14);
	OFLEN_PAIR(romfs->file_meta, 0x1C);
#undef OFLEN_PAIR
	romfs->data_offset = ((u64) U32P(&l3_header[0x24])) + l3_offset;

	return NNC_R_OK;
}

static u32 hash_func(u16 *name, u32 len, u32 parent)
{
	/* ninty tm */
	u32 ret = parent ^ 123456789;
	for(u32 i = 0; i < len; ++i)
	{
		ret = (ret >> 5) | (ret << 27);
		ret ^= LE16(name[i]);
	}
	return ret;
}

#define UTF16(str, len, conv, rlen, ...) \
	u16 conv[MAX_PATH]; \
	if(len >= sizeof(conv)) \
	{ __VA_ARGS__ } \
	u32 rlen; \
	if((rlen = nnc_utf8_to_utf16(conv, MAX_PATH, (const u8 *) str, len)) >= MAX_PATH) \
	{ __VA_ARGS__ }

#define DIR_PARENT(buf) LE32P(&(buf)[0x00])
#define DIR_SIBLING(buf) LE32P(&(buf)[0x04])
#define DIR_DCHILDREN(buf) LE32P(&(buf)[0x08])
#define DIR_FCHILDREN(buf) LE32P(&(buf)[0x0C])
#define DIR_NEXTHASH(buf) LE32P(&(buf)[0x10])
#define DIR_NAMELEN(buf) LE32P(&(buf)[0x14])
#define DIR_NAME(buf) ((const u16 *) (&(buf)[0x18]))
#define DIR_META(ctx, offset) (ctx->dir_meta_data + offset)

#define FILE_PARENT(buf) LE32P(&(buf)[0x0])
#define FILE_SIBLING(buf) LE32P(&(buf)[0x04])
#define FILE_OFFSET(buf) LE64P(&(buf)[0x08])
#define FILE_SIZE(buf) LE64P(&(buf)[0x10])
#define FILE_NEXTHASH(buf) LE32P(&(buf)[0x18])
#define FILE_NAMELEN(buf) LE32P(&(buf)[0x1C])
#define FILE_NAME(buf) ((const u16 *) (&(buf)[0x20]))
#define FILE_META(ctx, offset) (ctx->file_meta_data + offset)

static u32 get_dir_single_offset(nnc_romfs_ctx *ctx, const char *path, u32 len, u32 parent_offset)
{
	UTF16(path, len, conv, rlen, return INVAL;)
	u32 tab_len = ctx->header.dir_hash.length / sizeof(u32);
	u32 i = hash_func(conv, rlen, parent_offset) % tab_len;

	u32 namelen, offset = ctx->dir_hash_tab[i];
	u8 *dir;

	do {
		if(offset == INVAL)
			break; /* bucket is unused; fail */
		dir = DIR_META(ctx, offset);
		namelen = DIR_NAMELEN(dir);
		if(namelen != rlen * 2 || memcmp(DIR_NAME(dir), conv, namelen))
		{
			offset = DIR_NEXTHASH(dir);
			continue;
		}
		return offset;
	} while(1);

	/* failed to find correct bucket */
	return INVAL;
}

static u32 get_file_single_offset(nnc_romfs_ctx *ctx, const char *path, u32 len, u32 parent_offset)
{
	UTF16(path, len, conv, rlen, return INVAL;)
	u32 tab_len = ctx->header.file_hash.length / sizeof(u32);
	u32 i = hash_func(conv, rlen, parent_offset) % tab_len;

	u32 namelen, offset = ctx->file_hash_tab[i];
	u8 *file;

	do {
		if(offset == INVAL)
			break; /* bucket is unused; fail */
		file = FILE_META(ctx, offset);
		namelen = FILE_NAMELEN(file);
		if(namelen != rlen * 2 || memcmp(FILE_NAME(file), conv, namelen))
		{
			offset = FILE_NEXTHASH(file);
			continue;
		}
		return offset;
	} while(1);

	/* failed to find correct bucket */
	return INVAL;
}

static u32 get_dir_offset_nofile(nnc_romfs_ctx *ctx, const char *path, const char **file_name)
{
	if(*path == '/') ++path;
	if(*path == '\0')
	{
		*file_name = NULL;
		return 0; /* root is always at offset 0 */
	}
	u32 of = 0;

	const char *slash = path;
	const char *prev = path;
	while((slash = strchr(slash, '/')))
	{
		int len = slash - prev;
		++slash;
		if(len == 0)
		{
			/* "somedirname//// "*/
			while(*slash == '/')
				++slash;
			/* "somedirname/" */
			if(*slash == '\0')
				break;
			continue;
		}
		if((of = get_dir_single_offset(ctx, prev, len, of)) == INVAL)
			return INVAL;
		prev = slash;
	}
	*file_name = prev;

	return of;
}

static void fill_info_file(nnc_romfs_ctx *ctx, nnc_romfs_info *info, u32 offset)
{
	u8 *file = FILE_META(ctx, offset);
	info->type = NNC_ROMFS_FILE;
	info->u.f.sibling = FILE_SIBLING(file);
	info->u.f.parent = FILE_PARENT(file);
	info->u.f.offset = FILE_OFFSET(file);
	info->u.f.size = FILE_SIZE(file);
	info->filename_length = FILE_NAMELEN(file) / sizeof(u16);
	info->filename = FILE_NAME(file);
}

static void fill_info_dir(nnc_romfs_ctx *ctx, nnc_romfs_info *info, u32 offset)
{
	u8 *dir = DIR_META(ctx, offset);
	info->type = NNC_ROMFS_DIR;
	info->u.d.dchildren = DIR_DCHILDREN(dir);
	info->u.d.fchildren = DIR_FCHILDREN(dir);
	info->u.d.sibling = DIR_SIBLING(dir);
	info->u.d.parent = DIR_PARENT(dir);
	info->filename_length = DIR_NAMELEN(dir) / sizeof(u16);
	info->filename = DIR_NAME(dir);
}

nnc_result nnc_get_info(nnc_romfs_ctx *ctx, nnc_romfs_info *info, const char *path)
{
	const char *file_name;
	u32 parent_of = get_dir_offset_nofile(ctx, path, &file_name), rof;
	if(parent_of == INVAL) return NNC_R_NOT_FOUND;
	/* file_name == NULL means we want to get info on / */
	if(!file_name) { rof = 0; goto do_dir; }
	u32 len = strlen(file_name);
	// TODO: If we convert path to UTF16 here we can save a conversion
	//       in get_dir_single_offset
	/* opening a file is the more likely case */
	rof = get_file_single_offset(ctx, file_name, len, parent_of);
	if(rof != INVAL)
	{
		fill_info_file(ctx, info, rof);
		return NNC_R_OK;
	}
	/* but a directory is possible too */
	rof = get_dir_single_offset(ctx, file_name, len, parent_of);
	if(rof != INVAL)
	{
do_dir:
		fill_info_dir(ctx, info, rof);
		return NNC_R_OK;
	}
	info->type = NNC_ROMFS_NONE;
	return NNC_R_NOT_FOUND;
}

int nnc_romfs_next(nnc_romfs_iterator *it, nnc_romfs_info *ent)
{
	if(!it->dir || it->next == INVAL) return 0;
	if(it->in_dir)
	{
		fill_info_dir(it->ctx, ent, it->next);
		it->next = ent->u.d.sibling;
		if(it->next == INVAL)
		{
			it->next = it->dir->u.d.fchildren;
			it->in_dir = false;
		}
	}
	else
	{
		fill_info_file(it->ctx, ent, it->next);
		it->next = ent->u.f.sibling;
	}
	return 1;
}

nnc_romfs_iterator nnc_romfs_mkit(nnc_romfs_ctx *ctx, nnc_romfs_info *dir)
{
	nnc_romfs_iterator ret;
	if(dir->type != NNC_ROMFS_DIR)
	{
		ret.dir = NULL;
		return ret;
	}
	ret.dir = dir;
	if(dir->u.d.dchildren == INVAL)
	{
		ret.next = dir->u.d.fchildren;
		ret.in_dir = false;
	}
	else
	{
		ret.next = dir->u.d.dchildren;
		ret.in_dir = true;
	}
	ret.ctx = ctx;
	return ret;
}

result nnc_romfs_open_subview(nnc_romfs_ctx *ctx, nnc_subview *sv, nnc_romfs_info *info)
{
	if(info->type != NNC_ROMFS_FILE) return NNC_R_NOT_A_FILE;
	nnc_subview_open(sv, ctx->rs, ctx->header.data_offset + info->u.f.offset,
		info->u.f.size);
	return NNC_R_OK;
}

result nnc_init_romfs(nnc_rstream *rs, nnc_romfs_ctx *ctx)
{
	result ret;
	TRY(nnc_read_romfs_header(rs, &ctx->header));

	ctx->file_meta_data = ctx->dir_meta_data = NULL;
	ctx->file_hash_tab = ctx->dir_hash_tab = NULL;
	ret = NNC_R_NOMEM;

	if(!(ctx->file_hash_tab = malloc(ctx->header.file_hash.length)))
		goto fail;
	if(!(ctx->file_meta_data = malloc(ctx->header.file_meta.length)))
		goto fail;
	if(!(ctx->dir_hash_tab = malloc(ctx->header.dir_hash.length)))
		goto fail;
	if(!(ctx->dir_meta_data = malloc(ctx->header.dir_meta.length)))
		goto fail;

	if((ret = read_at_exact(rs, ctx->header.file_hash.offset, (u8 *) ctx->file_hash_tab,
		ctx->header.file_hash.length)) != NNC_R_OK) goto fail;
	if((ret = read_at_exact(rs, ctx->header.file_meta.offset, (u8 *) ctx->file_meta_data,
		ctx->header.file_meta.length)) != NNC_R_OK) goto fail;
	if((ret = read_at_exact(rs, ctx->header.dir_hash.offset, (u8 *) ctx->dir_hash_tab,
		ctx->header.dir_hash.length)) != NNC_R_OK) goto fail;
	if((ret = read_at_exact(rs, ctx->header.dir_meta.offset, (u8 *) ctx->dir_meta_data,
		ctx->header.dir_meta.length)) != NNC_R_OK) goto fail;

	ctx->rs = rs;
	return NNC_R_OK;
fail:
	/* calls the same functions as we would want to do here */
	nnc_free_romfs(ctx);
	return ret;
}

void nnc_free_romfs(nnc_romfs_ctx *ctx)
{
	free(ctx->file_meta_data);
	free(ctx->file_hash_tab);
	free(ctx->dir_meta_data);
	free(ctx->dir_hash_tab);
}

