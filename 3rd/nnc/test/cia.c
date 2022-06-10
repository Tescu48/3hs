
#include <nnc/read-stream.h>
#include <nnc/romfs.h>
#include <nnc/ncch.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <nnc/cia.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

void die(const char *fmt, ...);


static void extract(nnc_rstream *rs, const char *to, const char *type)
{
	printf("Saving %s to %s ...\n", type, to);
	nnc_u32 len = NNC_RS_PCALL0(rs, size), rlen;
	nnc_u8 *buf = malloc(len);
	if(NNC_RS_PCALL(rs, read, buf, len, &rlen) != NNC_R_OK || len != rlen)
		die("read failure for %s", to);
	FILE *out = fopen(to, "w");
	if(fwrite(buf, len, 1, out) != 1)
		die("write failure for %s", to);
	fclose(out);
	free(buf);
}

int cia_main(int argc, char *argv[])
{
	if(argc != 3) die("usage: %s <cia-file> <output-directory>", argv[0]);
	const char *cia_file = argv[1];
	char *output = argv[2];

	int len = strlen(output);
	/* trim leading /'s for a prettier output */
	while(output[--len] == '/')
		output[len] = '\0';

	if(access(output, F_OK) != 0 && mkdir(output, 0777) != 0)
		die("failed to create directory '%s': %s", output, strerror(errno));

	nnc_file f;
	if(nnc_file_open(&f, cia_file) != NNC_R_OK)
		die("failed to open '%s'", cia_file);

	nnc_cia_header header;
	if(nnc_read_cia_header(NNC_RSP(&f), &header) != NNC_R_OK)
		die("failed to read CIA header from '%s'", cia_file);

	printf(
		"== %s ==\n"
		" Type                    : %04X\n"
		" Version                 : %04X\n"
		" Certificate Chain Size  : 0x%08" PRIX32 "\n"
		" Ticket Size             : 0x%08" PRIX32 "\n"
		" TMD Size                : 0x%08" PRIX32 "\n"
		" Meta Size               : 0x%08" PRIX32 "\n"
		" Content Size            : 0x%016" PRIX64 "\n"
		" Content Index           : ["
	, cia_file, header.type, header.version, header.cert_chain_size
	, header.ticket_size, header.tmd_size, header.meta_size, header.content_size);

	nnc_u32 index, comma = 0;
	NNC_FOREACH_CINDEX(index, header.content_index)
	{
		printf(comma ? ", %X" : "%X", index);
		comma = 1;
	}
	puts("]");

	char pathbuf[1024];
	nnc_subview sv;
 
	snprintf(pathbuf, sizeof(pathbuf), "%s/certchain", output);
	nnc_cia_open_ticket(&header, NNC_RSP(&f), &sv);
	extract(NNC_RSP(&sv), pathbuf, "certificate chain");

	snprintf(pathbuf, sizeof(pathbuf), "%s/tik", output);
	nnc_cia_open_ticket(&header, NNC_RSP(&f), &sv);
	extract(NNC_RSP(&sv), pathbuf, "ticket");

	snprintf(pathbuf, sizeof(pathbuf), "%s/tmd", output);
	nnc_cia_open_tmd(&header, NNC_RSP(&f), &sv);
	extract(NNC_RSP(&sv), pathbuf, "TMD");

	snprintf(pathbuf, sizeof(pathbuf), "%s/meta", output);
	if(nnc_cia_open_meta(&header, NNC_RSP(&f), &sv) == NNC_R_OK)
		extract(NNC_RSP(&sv), pathbuf, "CIA meta section");
	else fprintf(stderr, "WARN: no meta in CIA.\n");

	nnc_cia_content_reader reader;
	nnc_keyset kset = NNC_KEYSET_INIT;
	nnc_keyset_default(&kset, false);
	if(nnc_cia_make_reader(&header, NNC_RSP(&f), &kset, &reader) != NNC_R_OK)
		die("failed making content reader");

	nnc_keypair kp;
	nnc_keyset ks = NNC_KEYSET_INIT;
	nnc_keyset_default(&ks, false);

	NNC_FOREACH_CINDEX(index, header.content_index)
	{
		nnc_cia_content_stream ncch;
		nnc_chunk_record *chunk;
		nnc_result res = nnc_cia_open_content(&reader, index, &ncch, &chunk);
		if(res != NNC_R_OK) die("failed opening NCCH index %i", index);
		snprintf(pathbuf, sizeof(pathbuf), "%s/%08" PRIX32, output, chunk->id);
		static char type[] = "NCCH content index XXXX";
		snprintf(type + 13, 5, "%04X", chunk->index);
		extract(NNC_RSP(&ncch), pathbuf, type);

		/* this section is here to test CBC seeking */
		NNC_RS_CALL(ncch, seek_abs, 0);
		nnc_ncch_header ncch_hdr;
		if(nnc_read_ncch_header(NNC_RSP(&ncch), &ncch_hdr) != NNC_R_OK)
			die("failed reading NCCH header");
		nnc_ncch_section_stream rrs;
		nnc_fill_keypair(&kp, &ks, NULL, &ncch_hdr);
		if(nnc_ncch_section_romfs(&ncch_hdr, NNC_RSP(&ncch), &kp, &rrs) == NNC_R_OK)
		{
			nnc_romfs_header romfs;
			if(nnc_read_romfs_header(NNC_RSP(&rrs), &romfs) != NNC_R_OK)
				printf("failed reading romfs\n");
			else printf("read romfs correctly\n");
		}

		NNC_RS_CALL0(ncch, close);
	}
	nnc_cia_free_reader(&reader);

	NNC_RS_CALL0(f, close);
	return 0;
}

