
#include <mbedtls/aes.h>
#include <nnc/ncch.h>
#include <string.h>
#include <stdlib.h>
#include "./internal.h"

#define EXHEADER_OFFSET NNC_MEDIA_UNIT
#define EXHEADER_SIZE 0x800


static u32 u32pow(u32 x, u8 y)
{
	u32 ret = 1;
	for(u32 i = 0; i < y; ++i)
		ret *= x;
	return ret;
}

result nnc_read_ncch_header(rstream *rs, nnc_ncch_header *ncch)
{
	u8 header[0x200];
	result ret;

	TRY(read_exact(rs, header, sizeof(header)));
	/* 0x000 */ ncch->keyy = nnc_u128_import_be(&header[0x000]);
	/* 0x000 */ /* signature (and keyY) */
	/* 0x100 */ if(memcmp(&header[0x100], "NCCH", 4) != 0)
	/* 0x100 */ 	return NNC_R_CORRUPT;
	/* 0x104 */ ncch->content_size = LE32P(&header[0x104]);
	/* 0x108 */ ncch->partition_id = LE64P(&header[0x108]);
	/* 0x110 */ ncch->maker_code[0] = header[0x110];
	/* 0x111 */ ncch->maker_code[1] = header[0x111];
	/* 0x111 */ ncch->maker_code[2] = '\0';
	/* 0x112 */ ncch->version = LE16P(&header[0x112]);
	/* 0x114 */ ncch->seed_hash = U32P(&header[0x114]);
	/* 0x118 */ ncch->title_id = LE64P(&header[0x118]);
	/* 0x120 */ /* reserved */
	/* 0x130 */ memcpy(ncch->logo_hash, &header[0x130], sizeof(nnc_sha256_hash));
	/* 0x150 */ memcpy(ncch->product_code, &header[0x150], 0x10);
	/* 0x150 */ ncch->product_code[0x10] = '\0';
	/* 0x160 */ memcpy(ncch->exheader_hash, &header[0x160], sizeof(nnc_sha256_hash));
	/* 0x180 */ ncch->exheader_size = LE32P(&header[0x180]);
	/* 0x184 */ /* reserved */
	/* 0x188 */ /* ncchflags[0] */
	/* 0x189 */ /* ncchflags[1] */
	/* 0x18A */ /* ncchflags[2] */
	/* 0x18B */ ncch->crypt_method = header[0x18B];
	/* 0x18C */ ncch->platform = header[0x18C];
	/* 0x18D */ ncch->type = header[0x18D];
	/* 0x18E */ ncch->content_unit = NNC_MEDIA_UNIT * u32pow(2, header[0x18E]);
	/* 0x18F */ ncch->flags = header[0x18F];
	/* 0x190 */ ncch->plain_offset = LE32P(&header[0x190]);
	/* 0x194 */ ncch->plain_size = LE32P(&header[0x194]);
	/* 0x198 */ ncch->logo_offset = LE32P(&header[0x198]);
	/* 0x19C */ ncch->logo_size = LE32P(&header[0x19C]);
	/* 0x1A0 */ ncch->exefs_offset = LE32P(&header[0x1A0]);
	/* 0x1A4 */ ncch->exefs_size = LE32P(&header[0x1A4]);
	/* 0x1A8 */ ncch->exefs_hash_size = LE32P(&header[0x1A8]);
	/* 0x1AC */ /* reserved */
	/* 0x1B0 */ ncch->romfs_offset = LE32P(&header[0x1B0]);
	/* 0x1B4 */ ncch->romfs_size = LE32P(&header[0x1B4]);
	/* 0x1B8 */ ncch->romfs_hash_size = LE32P(&header[0x1B8]);
	/* 0x1BC */ /* reserved */
	/* 0x1C0 */ memcpy(ncch->exefs_hash, &header[0x1C0], sizeof(nnc_sha256_hash));
	/* 0x1E0 */ memcpy(ncch->romfs_hash, &header[0x1E0], sizeof(nnc_sha256_hash));
	return NNC_R_OK;
}

#define SUBVIEW_R(mode, offset, size) \
	nnc_subview_open(&section->u. mode .sv, rs, offset, size)
#define SUBVIEW(mode, offset, size) \
	SUBVIEW_R(mode, NNC_MU_TO_BYTE(offset), NNC_MU_TO_BYTE(size))

result nnc_ncch_section_romfs(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_keypair *kp, nnc_ncch_section_stream *section)
{
	if(ncch->flags & NNC_NCCH_NO_ROMFS || ncch->romfs_size == 0)
		return NNC_R_NOT_FOUND;
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return SUBVIEW(dec, ncch->romfs_offset, ncch->romfs_size), NNC_R_OK;

	u8 iv[0x10]; result ret;
	TRY(nnc_get_ncch_iv(ncch, NNC_SECTION_ROMFS, iv));
	SUBVIEW(enc, ncch->romfs_offset, ncch->romfs_size);
	return nnc_aes_ctr_open(&section->u.enc.crypt, NNC_RSP(&section->u.enc.sv),
		&kp->secondary, iv);
}

result nnc_ncch_section_exefs_header(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_keypair *kp, nnc_ncch_section_stream *section)
{
	if(ncch->exefs_size == 0) return NNC_R_NOT_FOUND;
	/* ExeFS header is only 1 NNC_MEDIA_UNIT large */
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return SUBVIEW(dec, ncch->exefs_offset, 1), NNC_R_OK;

	u8 iv[0x10]; result ret;
	TRY(nnc_get_ncch_iv(ncch, NNC_SECTION_EXEFS, iv));
	SUBVIEW(enc, ncch->exefs_offset, 1);
	return nnc_aes_ctr_open(&section->u.enc.crypt, NNC_RSP(&section->u.enc.sv),
		&kp->primary, iv);
}

nnc_result nnc_ncch_section_exheader(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_keypair *kp, nnc_ncch_section_stream *section)
{
	if(ncch->exheader_size == 0) return NNC_R_NOT_FOUND;
	/* for some reason the header says it's 0x400 bytes whilest it really is 0x800 bytes */
	if(ncch->exheader_size != 0x400) return NNC_R_CORRUPT;
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return SUBVIEW_R(dec, EXHEADER_OFFSET, EXHEADER_SIZE), NNC_R_OK;

	u8 iv[0x10]; result ret;
	TRY(nnc_get_ncch_iv(ncch, NNC_SECTION_EXHEADER, iv));
	SUBVIEW_R(enc, EXHEADER_OFFSET, EXHEADER_SIZE);
	return nnc_aes_ctr_open(&section->u.enc.crypt, NNC_RSP(&section->u.enc.sv),
		&kp->primary, iv);
}

nnc_result nnc_ncch_exefs_subview(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_keypair *kp, nnc_ncch_section_stream *section, nnc_exefs_file_header *header)
{
	if(ncch->exefs_size == 0) return NNC_R_NOT_FOUND;
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return SUBVIEW_R(dec,
			NNC_MU_TO_BYTE(ncch->exefs_offset) + NNC_EXEFS_HEADER_SIZE + header->offset,
			header->size), NNC_R_OK;

	nnc_u128 *key; u8 iv[0x10]; result ret;
	TRY(nnc_get_ncch_iv(ncch, NNC_SECTION_EXEFS, iv));
	nnc_u128 ctr = nnc_u128_import_be(iv);
	/* we need to advance the IV to the correct section, split the header so the
	 * compiler hopefully has an easier job optimizing that */
	nnc_u128 addition = NNC_PROMOTE128((header->offset / 0x10) + (NNC_EXEFS_HEADER_SIZE / 0x10));
	nnc_u128_add(&ctr, &addition);
	nnc_u128_bytes_be(&ctr, iv);

	/* these belong to the "info menu" group */
	if(strcmp(header->name, "icon") == 0 || strcmp(header->name, "banner") == 0)
		key = &kp->primary;
	/* the rest belongs to the "content" group */
	else
		key = &kp->secondary;

	SUBVIEW_R(enc,
		NNC_MU_TO_BYTE(ncch->exefs_offset) + NNC_EXEFS_HEADER_SIZE + header->offset,
		header->size);
	return nnc_aes_ctr_open(&section->u.enc.crypt, NNC_RSP(&section->u.enc.sv), key, iv);
}

nnc_result nnc_ncch_section_plain(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_subview *section)
{
	if(ncch->plain_size == 0) return NNC_R_NOT_FOUND;
	nnc_subview_open(section, rs, NNC_MU_TO_BYTE(ncch->plain_offset),
		NNC_MU_TO_BYTE(ncch->plain_size));
	return 0;
}

nnc_result nnc_ncch_section_logo(nnc_ncch_header *ncch, nnc_rstream *rs,
	nnc_subview *section)
{
	if(ncch->logo_size == 0) return NNC_R_NOT_FOUND;
	if(ncch->logo_size != (0x2000 / NNC_MEDIA_UNIT)) return NNC_R_CORRUPT;
	nnc_subview_open(section, rs, NNC_MU_TO_BYTE(ncch->logo_offset),
		NNC_MU_TO_BYTE(ncch->logo_size));
	return 0;
}

