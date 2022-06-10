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

#include "seed.hh"

#include <unordered_map>
#include <string.h>
#include <stdio.h>
#include <3ds.h>

#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

/* because you can't copy a u8[0x10] but you can a struct containing a u8[0x10].... */
typedef struct Seed
{
	u8 seed[0x10];
} Seed;

typedef struct SeedDBHeader
{
	u32 size;
	u8 pad[0xC];
} SeedDBHeader;

typedef struct SeedDBEntry
{
	u64 tid;
	Seed seed;
	u8 pad[0x8];
} SeedDBEntry;

static std::unordered_map<u64 /* tid */, Seed /* seed */> g_seeddb;

// from FBI:
// https://github.com/Steveice10/FBI/blob/6e3a28e4b674e0d7a6f234b0419c530b358957db/source/core/http.c#L440-L453
static Result FSUSER_AddSeed(u64 tid, const void *seed)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x087A0180;
	cmdbuf[1] = (u32) (tid & 0xFFFFFFFF);
	cmdbuf[2] = (u32) (tid >> 32);
	memcpy(&cmdbuf[3], seed, 16);

	Result ret;
	if(R_FAILED(ret = svcSendSyncRequest(*fsGetSessionHandle())))
		return ret;

	ret = cmdbuf[1];
	return ret;
}

void init_seeddb()
{
	FILE *f = fopen("romfs:/seeddb.bin", "r");
	if(f == nullptr) panic(STRING(failed_open_seeddb));
	SeedDBHeader head;

	fread(&head, sizeof(SeedDBHeader), 1, f);
	SeedDBEntry *entries = new SeedDBEntry[head.size];
	fread(entries, sizeof(SeedDBEntry), head.size, f);

	for(size_t i = 0x0; i < head.size; ++i)
		g_seeddb[entries[i].tid] = entries[i].seed;

	delete [] entries;
	fclose(f);
}

Result add_seed(u64 tid)
{
	if(g_seeddb.count(tid) == 0)
	{
		ilog("Not adding seed for %016llX", tid);
		return 0x0;
	}
	ilog("Adding seed for %016llX", tid);
	return FSUSER_AddSeed(tid, g_seeddb[tid].seed);
}

