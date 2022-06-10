
#include <nnc/ticket.h>
#include <nnc/cia.h>
#include <nnc/tmd.h>
#include <string.h>
#include <stdlib.h>
#include "./internal.h"

#define CALIGN(n) ALIGN(n, 64)
#define HDR_AL CALIGN(0x2020)

nnc_result nnc_read_cia_header(nnc_rstream *rs, nnc_cia_header *cia)
{
	nnc_u8 header[0x2020];
	result ret;

	TRY(read_exact(rs, header, sizeof(header)));
	/* 0x00 */ if(LE32P(&header[0x00]) != 0x2020) return NNC_R_CORRUPT;
	/* 0x04 */ cia->type = LE16P(&header[0x04]);
	/* 0x06 */ cia->version = LE16P(&header[0x06]);
	/* 0x08 */ cia->cert_chain_size = LE32P(&header[0x08]);
	/* 0x0C */ cia->ticket_size = LE32P(&header[0x0C]);
	/* 0x10 */ cia->tmd_size = LE32P(&header[0x10]);
	/* 0x14 */ cia->meta_size = LE32P(&header[0x14]);
	/* 0x18 */ cia->content_size = LE64P(&header[0x18]);
	/* 0x20 */ memcpy(cia->content_index, &header[0x20], sizeof(cia->content_index));
	return NNC_R_OK;
}

nnc_result nnc_cia_open_certchain(nnc_cia_header *cia, nnc_rstream *rs, nnc_subview *sv)
{
	nnc_u32 offset = HDR_AL;
	nnc_subview_open(sv, rs, offset, cia->cert_chain_size);
	return NNC_R_OK;
}

nnc_result nnc_cia_open_ticket(nnc_cia_header *cia, nnc_rstream *rs, nnc_subview *sv)
{
	nnc_u32 offset = HDR_AL + CALIGN(cia->cert_chain_size);
	nnc_subview_open(sv, rs, offset, cia->ticket_size);
	return NNC_R_OK;
}

nnc_result nnc_cia_open_tmd(nnc_cia_header *cia, nnc_rstream *rs, nnc_subview *sv)
{
	nnc_u32 offset = HDR_AL + CALIGN(cia->cert_chain_size) + CALIGN(cia->ticket_size);
	nnc_subview_open(sv, rs, offset, cia->tmd_size);
	return NNC_R_OK;
}

nnc_result nnc_cia_open_meta(nnc_cia_header *cia, nnc_rstream *rs, nnc_subview *sv)
{
	if(cia->meta_size == 0) return NNC_R_NOT_FOUND;
	nnc_u32 offset = HDR_AL + CALIGN(cia->cert_chain_size) + CALIGN(cia->ticket_size) + CALIGN(cia->tmd_size) + CALIGN(cia->content_size);
	nnc_subview_open(sv, rs, offset, cia->meta_size);
	return NNC_R_OK;
}

nnc_result nnc_cia_make_reader(nnc_cia_header *cia, nnc_rstream *rs,
	nnc_keyset *ks, nnc_cia_content_reader *reader)
{
	result ret;
	reader->cia = cia;
	reader->rs = rs;

	/* first we get the chunk records */
	nnc_tmd_header tmdhdr;
	nnc_subview sv;
	nnc_cia_open_tmd(cia, rs, &sv);
	TRY(nnc_read_tmd_header(NNC_RSP(&sv), &tmdhdr));
	reader->content_count = tmdhdr.content_count;
	reader->chunks = malloc(sizeof(nnc_chunk_record) * reader->content_count);
	if(!reader->chunks) return NNC_R_NOMEM;
	TRYLBL(nnc_read_tmd_chunk_records(NNC_RSP(&sv), &tmdhdr, reader->chunks), free_chunks);

	/* now we derive title key */
	nnc_ticket tik;
	nnc_cia_open_ticket(cia, rs, &sv);
	TRYLBL(nnc_read_ticket(NNC_RSP(&sv), &tik), free_chunks);
	/* obviously corrupt */
	if(tik.title_id != tmdhdr.title_id) { ret = NNC_R_CORRUPT; goto free_chunks; }
	TRYLBL(nnc_decrypt_tkey(&tik, ks, reader->key), free_chunks);

	return NNC_R_OK;
free_chunks:
	free(reader->chunks);
	return ret;
}

static nnc_result open_content(nnc_cia_content_reader *reader, nnc_chunk_record *chunk, nnc_u32 offset,
	nnc_cia_content_stream *content)
{
	if(chunk->flags & NNC_CHUNKF_ENCRYPTED)
	{
		u16 iv[8] = { BE16(chunk->index), 0, 0, 0, 0, 0, 0, 0 };
		nnc_subview_open(&content->u.enc.sv, reader->rs, offset, chunk->size);
		return nnc_aes_cbc_open(&content->u.enc.crypt, NNC_RSP(&content->u.enc.sv),
			reader->key, (u8 *) iv);
	}
	nnc_subview_open(&content->u.dec.sv, reader->rs, offset, chunk->size);
	return NNC_R_OK;
}

nnc_result nnc_cia_open_content(nnc_cia_content_reader *reader, nnc_u16 index,
	nnc_cia_content_stream *content, nnc_chunk_record **chunk_output)
{
	nnc_u16 i;
	nnc_u32 offset = HDR_AL + CALIGN(reader->cia->cert_chain_size) + CALIGN(reader->cia->ticket_size) + CALIGN(reader->cia->tmd_size);
	for(i = 0; i < reader->content_count; ++i)
	{
		if(reader->chunks[i].index == index) break;
		offset += CALIGN(reader->chunks[i].size);
	}
	if(i == reader->content_count)
		return NNC_R_NOT_FOUND;
	nnc_chunk_record *chunk = &reader->chunks[i];
	if(chunk_output) *chunk_output = chunk;

	return open_content(reader, chunk, offset, content);
}

void nnc_cia_free_reader(nnc_cia_content_reader *reader)
{
	free(reader->chunks);
}

