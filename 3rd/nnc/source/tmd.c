
#include <nnc/sigcert.h>
#include <nnc/tmd.h>
#include <string.h>
#include "./internal.h"

#define CINFO_SIZE (NNC_CINFO_MAX_SIZE * 0x24)


result nnc_read_tmd_header(rstream *rs, nnc_tmd_header *tmd)
{
	result ret;
	TRY(nnc_read_sig(rs, &tmd->sig));
	u8 buf[0x84];
	TRY(read_exact(rs, buf, sizeof(buf)));
	/* 0x00 */ tmd->version = buf[0x0];
	/* 0x01 */ tmd->ca_crl_ver = buf[0x1];
	/* 0x02 */ tmd->signer_crl_ver = buf[0x2];
	/* 0x03 */ /* reserved */
	/* 0x04 */ tmd->sys_ver = BE64P(&buf[0x4]);
	/* 0x0C */ tmd->title_id = BE64P(&buf[0xC]);
	/* 0x14 */ tmd->title_type = BE32P(&buf[0x14]);
	/* 0x18 */ tmd->group_id = BE16P(&buf[0x18]);
	/* 0x1A */ tmd->save_size = LE32P(&buf[0x1A]);
	/* 0x1E */ tmd->priv_save_size = LE32P(&buf[0x1E]);
	/* 0x22 */ /* reserved */
	/* 0x58 */ tmd->access_rights = BE32P(&buf[0x58]);
	/* 0x5C */ tmd->title_ver = BE16P(&buf[0x5C]);
	/* 0x5E */ tmd->content_count = BE16P(&buf[0x5E]);
	/* 0x64 */ memcpy(tmd->hash, &buf[0x64], sizeof(nnc_sha256_hash));
	return NNC_R_OK;
}

static u32 get_cinfo_pos(nnc_tmd_header *tmd)
{
	u32 ret = nnc_sig_size(tmd->sig.type);
	if(!ret) return 0;
	return ret + 0xC4;
}

static u32 get_crec_pos(nnc_tmd_header *tmd)
{
	u32 ret = nnc_sig_size(tmd->sig.type);
	if(!ret) return 0;
	return ret + 0x9C4;
}

static result parse_info_records(u8 *buf, nnc_cinfo_record *records)
{
	for(u8 i = 0; i < NNC_CINFO_MAX_SIZE; ++i)
	{
		u8 *blk = buf + (i * 0x24);
		/* 0x00 */ records[i].offset = BE16P(&blk[0x00]);
		/* 0x02 */ records[i].count = BE16P(&blk[0x02]);
		if(records[i].count == 0)
			break; /* end */
		/* 0x04 */ memcpy(records[i].hash, &blk[0x04], 0x20);
	}
	return NNC_R_OK;
}

result nnc_verify_read_tmd_info_records(rstream *rs, nnc_tmd_header *tmd,
	nnc_cinfo_record *records)
{
	u32 pos = get_cinfo_pos(tmd);
	if(!pos) return NNC_R_INVALID_SIG;

	result ret;
	u8 data[CINFO_SIZE];
	nnc_sha256_hash digest;
	TRY(read_at_exact(rs, pos, data, CINFO_SIZE));
	TRY(nnc_crypto_sha256(digest, data, CINFO_SIZE));
	if(memcmp(digest, tmd->hash, sizeof(digest)) != 0)
		return NNC_R_CORRUPT;
	return parse_info_records(data, records);
}

result nnc_read_tmd_info_records(rstream *rs, nnc_tmd_header *tmd,
	nnc_cinfo_record *records)
{
	u32 pos = get_cinfo_pos(tmd);
	if(!pos) return NNC_R_INVALID_SIG;

	result ret;
	u8 data[CINFO_SIZE];
	TRY(read_at_exact(rs, pos, data, CINFO_SIZE));
	return parse_info_records(data, records);
}

bool nnc_verify_tmd_info_records(rstream *rs, nnc_tmd_header *tmd)
{
	u32 pos = get_cinfo_pos(tmd);
	if(!pos) return false;

	nnc_sha256_hash digest;
	TRYB(NNC_RS_PCALL(rs, seek_abs, pos));
	TRYB(nnc_crypto_sha256_part(rs, digest, CINFO_SIZE));
	return memcmp(digest, tmd->hash, sizeof(digest)) == 0;
}

bool nnc_verify_tmd_chunk_records(rstream *rs, nnc_tmd_header *tmd, nnc_cinfo_record *records)
{
	u32 pos = get_crec_pos(tmd);
	if(!pos) return false;
	TRYB(NNC_RS_PCALL(rs, seek_abs, pos));
	u32 to_hash = tmd->content_count;
	for(u32 i = 0; records[i].count != 0 && to_hash != 0; ++i)
	{
		to_hash -= records[i].count;
		/* sizeof(chunk_record) = 0x30 */
		u32 hash_size = records[i].count * 0x30;
		nnc_sha256_hash digest;
		TRYB(nnc_crypto_sha256_part(rs, digest, hash_size));
		if(memcmp(digest, records[i].hash, sizeof(digest)) != 0)
			return false;
	}
	return to_hash == 0;
}

result nnc_read_tmd_chunk_records(rstream *rs, nnc_tmd_header *tmd, nnc_chunk_record *records)
{
	u32 pos = get_crec_pos(tmd);
	if(!pos) return NNC_R_INVALID_SIG;
	result ret;
	TRY(NNC_RS_PCALL(rs, seek_abs, pos));
	for(u16 i = 0; i < tmd->content_count; ++i)
	{
		nnc_chunk_record *rec = &records[i];
		u8 blk[0x30];
		TRY(read_exact(rs, blk, sizeof(blk)));
		/* 0x00 */ rec->id = BE32P(&blk[0x00]);
		/* 0x04 */ rec->index = BE16P(&blk[0x04]);
		/* 0x06 */ rec->flags = BE16P(&blk[0x06]);
		/* 0x08 */ rec->size = BE64P(&blk[0x08]);
		/* 0x10 */ memcpy(rec->hash, &blk[0x10], sizeof(nnc_sha256_hash));
	}
	return NNC_R_OK;
}

