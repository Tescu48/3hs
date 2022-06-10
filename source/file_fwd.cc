
#include <nnc/read-stream.h>
#include <nnc/romfs.h>
#include <nnc/ncch.h>
#include <nnc/cia.h>

#include <string.h>
#include <3ds.h>

#include "error.hh"
#include "log.hh"

/**
 * This code basically just takes a theme installer CIA file (internally known as a "file forwarder") and
 * installs the file in RomFS to the correct directory on the SD card
 */

/** format:
 *   - cia ncch0 romfs contains 2 files
 *     - config.ini is a configuration file containing 2 lines
 *       - destination=... # File destination on SD
 *       - location=...    # The location in RomFS of the file being copied
 *     - the other file has a variable name found in config.ini
 */

static u8 *read_rs(nnc_rstream *rs, size_t *len)
{
	*len = NNC_RS_PCALL0(rs, size);
	u32 readSize;
	u8 *contents = (u8 *) malloc(*len + 1);
	if(NNC_RS_PCALL(rs, read, contents, *len, &readSize) != NNC_R_OK || readSize != *len)
	{ free(contents); return NULL; }
	return contents;
}

/* prototyped in install.hh */
Result install_forwarder(u8 *data, size_t len)
{
	nnc_keyset kset;
	nnc_keyset_default(&kset, false);

	nnc_memory cia;
	nnc_mem_open(&cia, data, len);

	nnc_cia_content_reader reader;
	nnc_cia_header header;
	u8 *contents = NULL;
	char *contents_s, *dest, *src, *end;
	nnc_romfs_ctx romfs;
	nnc_ncch_section_stream romfsSection;
	nnc_cia_content_stream ncch0;
	nnc_keypair kpair;
	nnc_romfs_info info;
	nnc_subview sv;
	nnc_ncch_header ncchHeader;
	FILE *out = NULL;
	std::string rdest;

	const char *deststr = "destination=";
	const size_t deststrlen = strlen(deststr);
	const char *locstr = "location=";
	const size_t locstrlen = strlen(locstr);

	if(nnc_read_cia_header(NNC_RSP(&cia), &header) != NNC_R_OK) return APPERR_FILEFWD_FAIL;
	if(nnc_cia_make_reader(&header, NNC_RSP(&cia), &kset, &reader) != NNC_R_OK) return APPERR_FILEFWD_FAIL;
	if(nnc_cia_open_content(&reader, 0, &ncch0, NULL) != NNC_R_OK) goto fail;
	if(nnc_read_ncch_header(NNC_RSP(&ncch0), &ncchHeader) != NNC_R_OK) goto fail;
	/* we don't need a seeddb here since theme installers will never use a seed */
	if(nnc_fill_keypair(&kpair, &kset, NULL, &ncchHeader) != NNC_R_OK) goto fail;
	if(nnc_ncch_section_romfs(&ncchHeader, NNC_RSP(&ncch0), &kpair, &romfsSection)) goto fail;
	if(nnc_init_romfs(NNC_RSP(&romfsSection), &romfs) != NNC_R_OK) goto fail;

	/* finally the actual parsing starts */
	if(nnc_get_info(&romfs, &info, "/config.ini") != NNC_R_OK) goto fail2;
	if(nnc_romfs_open_subview(&romfs, &sv, &info) != NNC_R_OK) goto fail2;
	if(!(contents = read_rs(NNC_RSP(&sv), &len))) goto fail2;
	contents_s = (char *) contents;
	contents[len] = '\0';

	dest = strstr(contents_s, deststr);
	if(!dest) goto fail2;
	dest += deststrlen;
	end = strchr(dest, '\n');

	src = strstr(contents_s, locstr);
	if(!src) goto fail2;
	src += locstrlen;
	/* we can only now terminate the dest end because else the previous strstr may fail */
	if(end) *end = '\0';
	end = strchr(src, '\n');
	/* this one terminates the src end */
	if(end) *end = '\0';

	/* now we just have to copy the files around */
	rdest = strstr(dest, "sdmc:/") == dest ? dest : "sdmc:/" + std::string(dest);
	if(nnc_get_info(&romfs, &info, src) != NNC_R_OK) goto fail2;
	if(nnc_romfs_open_subview(&romfs, &sv, &info)) goto fail2;
	free(contents);
	if(!(contents = read_rs(NNC_RSP(&sv), &len))) goto fail2;
	out = fopen(rdest.c_str(), "w");
	if(!out) goto fail2;
	if(fwrite(contents, len, 1, out) != 1) goto fail2;

	fclose(out);
	nnc_free_romfs(&romfs);
	free(contents);
	nnc_cia_free_reader(&reader);
	return 0;
fail2:
	if(out) fclose(out);
	nnc_free_romfs(&romfs);
fail:
	free(contents);
	nnc_cia_free_reader(&reader);
	return APPERR_FILEFWD_FAIL;
}

