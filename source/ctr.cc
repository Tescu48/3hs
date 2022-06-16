/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "install.hh"
#include "error.hh"
#include "panic.hh"
#include "ctr.hh"

#include <string.h>

#define AEXEFS_SMDH_PATH             { 0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000 }
#define MAKE_EXEFS_APATH(tid, media) { (u32) (tid & 0xFFFFFFFF), (u32) ((tid >> 32) & 0xFFFFFFFF), media, 0x00000000 }
#define SMDH_MAGIC "SMDH"
#define SMDH_MAGIC_LEN 4


#define makebin(data) makebin_(data, sizeof(data))
static inline FS_Path makebin_(const void *path, u32 size)
{
	return { PATH_BINARY, size, path };
}

ctr::TitleSMDH *ctr::smdh::get(u64 tid)
{
	static const u32 smdhPath[5] = AEXEFS_SMDH_PATH;
	u32 exefsArchivePath[4] = MAKE_EXEFS_APATH(tid, ctr::mediatype_of(tid));

	Handle smdhFile;
	if(R_FAILED(FSUSER_OpenFileDirectly(&smdhFile, ARCHIVE_SAVEDATA_AND_CONTENT,
			makebin(exefsArchivePath), makebin(smdhPath), FS_OPEN_READ, 0)))
		return nullptr;

	TitleSMDH *ret = new TitleSMDH; u32 bread = 0;
	memset(ret, 0x0, sizeof(TitleSMDH));

	if(R_FAILED(FSFILE_Read(smdhFile, &bread, 0, ret, sizeof(TitleSMDH))))
	{ delete ret; ret = nullptr; goto finish; }

	// Invalid smdh
	if(memcmp(ret->magic, SMDH_MAGIC, SMDH_MAGIC_LEN) != 0)
	{ delete ret; ret = nullptr; goto finish; }

finish:
	FSFILE_Close(smdhFile);
	return ret;
}

ctr::TitleSMDHTitle *ctr::smdh::get_native_title(TitleSMDH *smdh)
{
	TitleSMDHTitle *title = nullptr;
	u8 syslang;

	if(R_SUCCEEDED(CFGU_GetSystemLanguage(&syslang)))
	{
		switch(syslang)
		{
		case CFG_LANGUAGE_JP: title = &smdh->titles[0]; break;
		case CFG_LANGUAGE_EN: title = &smdh->titles[1]; break;
		case CFG_LANGUAGE_FR: title = &smdh->titles[2]; break;
		case CFG_LANGUAGE_DE: title = &smdh->titles[3]; break;
		case CFG_LANGUAGE_IT: title = &smdh->titles[4]; break;
		case CFG_LANGUAGE_ES: title = &smdh->titles[5]; break;
		case CFG_LANGUAGE_ZH: title = &smdh->titles[6]; break;
		case CFG_LANGUAGE_KO: title = &smdh->titles[7]; break;
		case CFG_LANGUAGE_NL: title = &smdh->titles[8]; break;
		case CFG_LANGUAGE_PT: title = &smdh->titles[9]; break;
		case CFG_LANGUAGE_RU: title = &smdh->titles[10]; break;
		case CFG_LANGUAGE_TW: title = &smdh->titles[11]; break;
		}
	}

	if(title != nullptr && title->descShort[0] != 0)
		return title;

	// EN, JP, FR, DE, IT, ES, ZH, KO, NL, PT, RU, TW
	static const u8 lookuporder[] = { 1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	for(u8 i = 0; i < sizeof(lookuporder); ++i)
	{
		if(smdh->titles[i].descShort[0] != 0)
			return &smdh->titles[i];
	}

	return nullptr;
}

std::string ctr::smdh::u16conv(u16 *str, size_t size)
{
	u16 *strexpand = (u16 *) malloc(sizeof(u16) * (size + 1));
	memcpy(strexpand, str, size * sizeof(u16));
	strexpand[size] = 0; /* all this for a NULL term */

	u8 *buf = (u8 *) malloc(size * 4);
	ssize_t units = utf16_to_utf8(buf, str, size * 4);
	if(units == -1 || units > (ssize_t) size * 4)
		panic("Failed to decode utf16 sequence, units=" + std::to_string(units));

	free((void *) strexpand);
	std::string ret((char *) buf, units);
	free((void *) buf);
	return ret;
}

Result ctr::list_titles_on(FS_MediaType media, std::vector<u64>& ret)
{
	u32 tcount = 0;
	Result res = AM_GetTitleCount(media, &tcount);
	if(R_FAILED(res)) return res;

	u32 tread = 0;
	u64 *tids = new u64[tcount];

	res = AM_GetTitleList(&tread, media, tcount, tids);
	if(R_FAILED(res)) return res;

	if(tread != tcount)
	{
		delete [] tids;
		return APPERR_TITLE_MISMATCH;
	}

	ret.reserve(tcount);
	for(u32 i = 0; i < tcount; ++i)
		ret.push_back(tids[i]);

	delete [] tids;
	return res;
}

Result ctr::get_free_space(Destination media, u64 *size)
{
	FS_ArchiveResource resource = { 0, 0, 0, 0 };
	Result res = 0;

	switch(media)
	{
	case DEST_TWLNand: res = FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_TWL_NAND); break;
	case DEST_CTRNand: res = FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_CTR_NAND); break;
	case DEST_Sdmc: res = FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_SD); break;
	}

	if(!R_FAILED(res)) *size = (u64) resource.clusterSize * (u64) resource.freeClusters;
	return res;
}

Result ctr::get_title_entry(u64 tid, AM_TitleEntry& entry)
{
	return AM_GetTitleInfo(ctr::mediatype_of(tid), 1, &tid, &entry);
}

u64 ctr::str_to_tid(const std::string& str)
{
	return strtoull(str.c_str(), nullptr, 16);
}

std::string ctr::tid_to_str(u64 tid)
{
	// is tid supposed to be 0 somewhere?
	// if not we can remove this check
	if(tid == 0) return "";

	char buf[17];
	snprintf(buf, 17, "%016llX", tid);
	return buf;
}

bool ctr::title_exists(u64 tid, FS_MediaType media)
{
	u32 tcount = 0;

	if(R_FAILED(AM_GetTitleCount(media, &tcount)))
		return false;

	u64 *tids = new u64[tcount];
	if(R_FAILED(AM_GetTitleList(&tcount, media, tcount, tids)))
	{
		delete [] tids;
		return false;
	}

	for(size_t i = 0; i < tcount; ++i)
		if(tids[i] == tid)
		{
			delete [] tids;
			return true;
		}

	delete [] tids;
	return false;
}

Result ctr::delete_title(u64 tid, FS_MediaType media)
{
	Result ret = 0;

	if(R_FAILED(ret = AM_DeleteTitle(media, tid))) return ret;
	if(R_FAILED(ret = AM_DeleteTicket(tid))) return ret;

	// Reloads the databases
	if(media == MEDIATYPE_SD) ret = AM_QueryAvailableExternalTitleDatabase(NULL);
	return ret;
}

Result ctr::delete_if_exist(u64 tid, FS_MediaType media)
{
	if(title_exists(tid, media))
		return delete_title(tid, media);
	return 0;
}

bool ctr::is_base_tid(u64 tid)
{
	u16 cat = ctr::get_tid_cat(tid);
	/* 0x4 = AddOnContents,
	 * which updates (0xE) and DLC (0x8C) also include
	 * 0x8000 = DSiWare,
	 * consider all DSiWare base. For some reason sets 0x4 for all DSiWare */
	return (cat & 0x8000) || (cat & 0x4) == 0;
}

u64 ctr::get_base_tid(u64 tid)
{
	/* clear the bits of cat (2nd u16) */
	return tid & 0xFFFF0000FFFFFFFF;
}

u16 ctr::get_tid_cat(u64 tid)
{
	return (tid >> 32) & 0xFFFF;
}

Result ctr::lockNDM()
{
	/* basically ensures that we can use the network during sleep
	 * thanks Kartik for the help */
	aptSetSleepAllowed(false);
	Result res;
	if(R_FAILED(res = NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE)))
		return res;
	return NDMU_LockState();
}

void ctr::unlockNDM()
{
	NDMU_UnlockState();
	NDMU_LeaveExclusiveState();
	aptSetSleepAllowed(true);
}


