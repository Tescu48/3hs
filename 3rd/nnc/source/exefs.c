
#include <nnc/exefs.h>
#include <string.h>
#include <stdlib.h>
#include "./internal.h"


bool nnc_exefs_file_in_use(nnc_exefs_file_header *fh)
{
	return fh->name[0] != '\0';
}

result nnc_read_exefs_header(nnc_rstream *rs, nnc_exefs_file_header *headers, nnc_u8 *size)
{
	result ret;
	u8 i = 0;

	u8 data[0x200];
	u8 *cur = data;
	TRY(read_at_exact(rs, 0x0, data, sizeof(data)));
	for(; i < NNC_EXEFS_MAX_FILES && cur[0] != '\0'; ++i, cur = &data[0x10 * i])
	{
		/* 0x00 */ memcpy(headers[i].name, cur, 8);
		/* 0x00 */ headers[i].name[8] = '\0';
		/* 0x08 */ headers[i].offset = LE32P(&cur[0x8]);
		/* 0x0C */ headers[i].size = LE32P(&cur[0xC]);
	}
	if(i != NNC_EXEFS_MAX_FILES)
		headers[i].name[0] = '\0';

	for(u8 j = 0; j < i; ++j)
	{
		memcpy(headers[j].hash,
			&data[0xC0 + ((NNC_EXEFS_MAX_FILES - j - 1) * sizeof(nnc_sha256_hash))],
			sizeof(nnc_sha256_hash));
	}

	if(size) *size = i;

	return NNC_R_OK;
}

i8 nnc_find_exefs_file_index(const char *name, nnc_exefs_file_header *headers)
{
	u32 len = strlen(name);
	if(len > 8) return -1;
	++len;
	for(u8 i = 0; i < NNC_EXEFS_MAX_FILES && nnc_exefs_file_in_use(&headers[i]); ++i)
	{
		if(memcmp(name, headers[i].name, len) == 0)
			return i;
	}
	return -1;
}

void nnc_exefs_subview(nnc_rstream *rs, nnc_subview *sv, nnc_exefs_file_header *header)
{
	nnc_subview_open(sv, rs, NNC_EXEFS_HEADER_SIZE + header->offset, header->size);
}

bool nnc_verify_file(nnc_rstream *rs, nnc_exefs_file_header *header)
{
	nnc_sha256_hash hash;
	TRYB(nnc_crypto_sha256_part(rs, hash, header->size));
	return memcmp(hash, header->hash, sizeof(header->hash)) == 0;
}

