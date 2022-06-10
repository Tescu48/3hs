
#include <nnc/read-stream.h>
#include <inttypes.h>
#include <nnc/tmd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define CUREC_SIZE 1024

void die(const char *fmt, ...);
void print_hash(nnc_u8 *b);


static const char *flags2str(nnc_u16 flags)
{
	static char buffer[0x100];
	int pos = 0;
#define ADD(flag, s) if(flags & (NNC_CHUNKF_##flag)) { strcpy(buffer + pos, s ", "); pos += strlen(s) + 2; buffer[pos] = '\0'; }
	ADD(ENCRYPTED, "encrypted")
	ADD(DISC, "disc")
	ADD(CFM, "CFM")
	ADD(OPTIONAL, "optional")
	ADD(SHARED, "shared")
#undef ADD
	if(pos == 0) return "none";
	/* trim final comma+space */
	buffer[pos - 2] = '\0';
	return buffer;
}

static const char *get_assumed_purpose(nnc_u16 index)
{
	switch(index)
	{
	case 0: return "Main content";
	case 1: return "Home menu manual";
	case 2: return "DLP child container";
	default: return "Unknown";
	}
}

int tmd_info_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <file>", argv[0]);
	const char *tmd_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, tmd_file) != NNC_R_OK)
		die("f->open() failed");

	nnc_tmd_header tmd;
	nnc_cinfo_record cirecords[NNC_CINFO_MAX_SIZE];
	nnc_chunk_record curecords[CUREC_SIZE];
	nnc_result res;
	if((res = nnc_read_tmd_header(NNC_RSP(&f), &tmd)) != NNC_R_OK)
		die("nnc_read_tmd_header() failed: %i", res);
	if(nnc_read_tmd_info_records(NNC_RSP(&f), &tmd, cirecords) != NNC_R_OK)
		die("nnc_read_tmd_info_records() failed");
	if(tmd.content_count > CUREC_SIZE)
		die("too many contents (want %i; have %i)", tmd.content_count, CUREC_SIZE);
	if(nnc_read_tmd_chunk_records(NNC_RSP(&f), &tmd, curecords) != NNC_R_OK)
		die("nnc_read_tmd_chunk_records() failed");

	nnc_u8 major, minor, patch;
	nnc_parse_version(tmd.title_ver, &major, &minor, &patch);

	printf(
		"== %s ==\n"
		" == TMD header ==\n"
		"  Signature Type         : %s\n"
		"  Signature Issuer       : %s\n"
		"  TMD Version            : %02X\n"
		"  CA CRL Version         : %02X\n"
		"  Signer CRL Version     : %02X\n"
		"  System version         : %016" PRIX64 "\n"
		"  Title ID               : %016" PRIX64 "\n"
		"  Title Type             : %08X\n"
		"  Save Size              : %i KiB (%i bytes)\n"
		"  SRL Private Save Size  : %i KiB (%i bytes)\n"
		"  Access Rights          : %08X\n"
		"  Title Version          : %i.%i.%i (v%i)\n"
		"  Group ID               : %04X\n"
		"  Content Count          : %04X\n"
		"  Boot Content           : %04X\n"
		"  Info Records Hash      : "
	, tmd_file
	, nnc_sigstr(tmd.sig.type), tmd.sig.issuer, tmd.version
	, tmd.ca_crl_ver, tmd.signer_crl_ver, tmd.sys_ver
	, tmd.title_id, tmd.title_type, tmd.save_size / 1024
	, tmd.save_size, tmd.priv_save_size / 1024, tmd.priv_save_size
	, tmd.access_rights, major, minor, patch, tmd.title_ver
	, tmd.group_id, tmd.content_count, tmd.boot_content);

	print_hash(tmd.hash);
	printf(nnc_verify_tmd_info_records(NNC_RSP(&f), &tmd) ? "  OK\n" : "  NOT OK\n");
	printf("  Content Chunk Records  : %s\n",
		nnc_verify_tmd_chunk_records(NNC_RSP(&f), &tmd, cirecords) ? "OK" : "NOT OK");
	printf("  Content Info Records   : \\\n");
	for(int i = 0; !NNC_CINFO_IS_LAST(cirecords[i]); ++i)
	{
		printf(
			"     ==  ==  %2i  ==  ==\n"
			"    Offset               : %04X\n"
			"    Count                : %04X\n"
			"    Hash                 : "
		, i, cirecords[i].offset, cirecords[i].count);
		print_hash(cirecords[i].hash);
		puts("");
	}
	printf("  Content Chunks         : \\\n");
	for(int i = 0; i < tmd.content_count; ++i)
	{
		printf(
			"     ==  ==  %2i  ==  ==\n"
			"    Assumed Purpose      : %s\n"
			"    ID                   : %08X\n"
			"    Index                : %04X\n"
			"    Flags                : %s (%04X)\n"
			"    Size                 : %" PRIu64 " KiB (%016" PRIX64 " bytes)\n"
			"    Hash                 : "
		, i, get_assumed_purpose(curecords[i].index)
		, curecords[i].id, curecords[i].index
		, flags2str(curecords[i].flags), curecords[i].flags
		, curecords[i].size / 1024, curecords[i].size);
		print_hash(curecords[i].hash);
		puts("");
	}

	NNC_RS_CALL0(f, close);
	return 0;
}

