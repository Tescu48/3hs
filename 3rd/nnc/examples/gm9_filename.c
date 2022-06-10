/** \example gm9_filename.c
 *  \brief   This example shows how to get all necessary data
 *           from a CIA to generate a Gomode9 filename.
 */

#include <nnc/read-stream.h>
#include <nnc/crypto.h>
#include <nnc/exefs.h>
#include <inttypes.h>
#include <nnc/smdh.h>
#include <nnc/ncch.h>
#include <nnc/cia.h>
#include <nnc/tmd.h>
#include <nnc/utf.h>
#include <string.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "must specify at least 1 file.\n");
		return 1;
	}

	nnc_seeddb seeddb;
	nnc_keyset kset;
	nnc_result res;
	int ret = 0;

	if((res = nnc_scan_seeddb(&seeddb)) != NNC_R_OK)
		fprintf(stderr, "Failed to load seeddb: %s. Titles with new crypto will not work.", nnc_strerror(res));
	nnc_keyset_default(&kset, false);

	for(int i = 1; i < argc; ++i)
	{
		const char *fname = argv[i];
		nnc_rstream *rs;
		nnc_subview sv;
		nnc_file file;
		if((res = nnc_file_open(&file, fname)) != NNC_R_OK)
		{
			fprintf(stderr, "%s: %s.\n", fname, nnc_strerror(res));
			ret = 1;
			continue;
		}
		rs = NNC_RSP(&file);

		nnc_ncch_section_stream ncchSection = { .u = { .enc = { .crypt = { .funcs = NULL } } } };
		nnc_cia_content_stream ncch0Stream = { .u = { .enc = { .crypt = { .funcs = NULL } } } };
		nnc_exefs_file_header exefsHeaders[NNC_EXEFS_MAX_FILES];
		nnc_cia_content_reader reader = { .chunks = NULL };
		nnc_ncch_header ncch0;
		nnc_cia_header cia;
		nnc_tmd_header tmd;
		nnc_keypair kpair;
		nnc_i8 exefsIndex;
		nnc_smdh smdh;

		if((res = nnc_read_cia_header(rs, &cia)) != NNC_R_OK) goto err;
		/* we only care about ncch0 since it contains all useful metadata */
		if(!NNC_CINDEX_HAS(cia.content_index, 0))
		{
			fprintf(stderr, "%s: missing NCCH 0.\n", fname);
			goto err;
		}
		nnc_cia_open_tmd(&cia, rs, &sv);
		/* tmd contains title version and title id */
		if((res = nnc_read_tmd_header(NNC_RSP(&sv), &tmd)) != NNC_R_OK) goto err;
		if((res = nnc_cia_make_reader(&cia, rs, &kset, &reader)) != NNC_R_OK) goto err;
		if((res = nnc_cia_open_content(&reader, 0, &ncch0Stream, NULL)) != NNC_R_OK) goto err;
		if((res = nnc_read_ncch_header(NNC_RSP(&ncch0Stream), &ncch0)) != NNC_R_OK) goto err;
		if((res = nnc_fill_keypair(&kpair, &kset, &seeddb, &ncch0)) != NNC_R_OK) goto err;
		if((res = nnc_ncch_section_exefs_header(&ncch0, NNC_RSP(&ncch0Stream), &kpair, &ncchSection)) != NNC_R_OK) goto err;
		if((res = nnc_read_exefs_header(NNC_RSP(&ncchSection), exefsHeaders, NULL)) != NNC_R_OK) goto err;
		nnc_u8 major, minor, patch;
		nnc_parse_version(tmd.title_ver, &major, &minor, &patch);
		if((exefsIndex = nnc_find_exefs_file_index("icon", exefsHeaders)) != -1)
		{
			NNC_RS_CALL0(ncchSection, close); ncchSection.u.enc.crypt.funcs = NULL;
			if((res = nnc_ncch_exefs_subview(&ncch0, NNC_RSP(&ncch0Stream), &kpair, &ncchSection, &exefsHeaders[exefsIndex])) != NNC_R_OK) goto err;
			if((res = nnc_read_smdh(NNC_RSP(&ncchSection), &smdh)) != NNC_R_OK) goto err;

			nnc_u16 *name = smdh.titles[NNC_TITLE_ENGLISH].short_desc;
#define NAME8_LEN ((int) (sizeof(smdh.titles[0]) * 4))
			nnc_u8 name8[NAME8_LEN + 1];
			name8[NAME8_LEN] = '\0';
			if(nnc_utf16_to_utf8(name8, NAME8_LEN, name, sizeof(smdh.titles[0])/sizeof(nnc_u16)) >= NAME8_LEN)
			{
				fprintf(stderr, "%s: failed to get title.\n", fname);
				goto err;
			}
			for(int i = 0; name8[i] != '\0'; ++i)
			{
				if(name8[i] == '\n' || name8[i] == '\r' || name8[i] == '/')
					name8[i] = ' ';
			}

			char region[8] = "";
			if(smdh.lockout == NNC_LOCKOUT_FREE) strcpy(region, "W");
			else sprintf(region, "%s%s%s%s%s%s",
				(smdh.lockout & NNC_LOCKOUT_JAPAN) ? "J" : "",
				(smdh.lockout & NNC_LOCKOUT_NORTH_AMERICA) ? "U" : "",
				(smdh.lockout & NNC_LOCKOUT_EUROPE) ? "E" : "",
 				(smdh.lockout & NNC_LOCKOUT_CHINA) ? "C" : "",
				(smdh.lockout & NNC_LOCKOUT_KOREA) ? "K" : "",
				(smdh.lockout & NNC_LOCKOUT_TAIWAN) ? "T" : "");
			if(strcmp(region, "JUECKT") == 0) strcpy(region, "W");
			if(region[0] == '\0') strcpy(region, "UNK");

			printf("%016" PRIX64 " %s (%s) (%s) (v%d.%d.%d).cia\n", tmd.title_id, (char *) name8, ncch0.product_code, region, major, minor, patch);
			goto out;
		}

		printf("%016" PRIX64 " (%s) (v%d.%d.%d).cia\n", tmd.title_id, ncch0.product_code, major, minor, patch);
		goto out;
err:
		fprintf(stderr, "%s: %s.\n", fname, nnc_strerror(res));
		ret = 1;
out:
		if(ncch0Stream.u.enc.crypt.funcs != NULL)
			NNC_RS_CALL0(ncch0Stream, close);
		if(ncchSection.u.enc.crypt.funcs != NULL)
			NNC_RS_CALL0(ncchSection, close);
		nnc_cia_free_reader(&reader);
		NNC_RS_CALL0(file, close);
	}

	nnc_free_seeddb(&seeddb);
	return ret;
}

