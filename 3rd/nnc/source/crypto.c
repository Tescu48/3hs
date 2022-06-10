
#include <mbedtls/version.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>
#include <nnc/crypto.h>
#include <nnc/ncch.h>
#include <stdlib.h>
#include <string.h>
#include "./internal.h"

#define BLOCK_SZ 0x10000

/* In MbedTLS version 2 the normal functions were marked deprecated
 * you were supposed to use *_ret, but in mbedTLS version 3+ the
 * *_ret functions had the functions renamed to have the _ret suffix removed */
#if MBEDTLS_VERSION_MAJOR == 2
	#define mbedtls_sha256_starts mbedtls_sha256_starts_ret
	#define mbedtls_sha256_update mbedtls_sha256_update_ret
	#define mbedtls_sha256_finish mbedtls_sha256_finish_ret
#endif


result nnc_crypto_sha256_part(nnc_rstream *rs, nnc_sha256_hash digest, u32 size)
{
	mbedtls_sha256_context ctx;
	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts(&ctx, 0);
	u8 block[BLOCK_SZ];
	u32 read_left = size, next_read = MIN(size, BLOCK_SZ), read_ret;
	result ret;
	while(read_left != 0)
	{
		ret = NNC_RS_PCALL(rs, read, block, next_read, &read_ret);
		if(ret != NNC_R_OK) goto out;
		if(read_ret != next_read) { ret = NNC_R_TOO_SMALL; goto out; }
		mbedtls_sha256_update(&ctx, block, read_ret);
		read_left -= next_read;
		next_read = MIN(read_left, BLOCK_SZ);
	}
	mbedtls_sha256_finish(&ctx, digest);
	ret = NNC_R_OK;
out:
	mbedtls_sha256_free(&ctx);
	return ret;
}

result nnc_crypto_sha256(const u8 *buf, nnc_sha256_hash digest, u32 size)
{
	mbedtls_sha256_context ctx;
	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts(&ctx, 0);
	mbedtls_sha256_update(&ctx, buf, size);
	mbedtls_sha256_finish(&ctx, digest);
	mbedtls_sha256_free(&ctx);
	return NNC_R_OK;
}

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
	#include <unistd.h>
	#define UNIX_LIKE
	#define can_read(f) access(f, R_OK) == 0
#elif defined(_WIN32)
	#include <io.h>
	#define can_read(f) _access_s(f, 4) == 0
#endif

static bool find_support_file(const char *name, char *output)
{
	char pathbuf[1024 + 1];
#define CHECK_BASE(...) do { \
		snprintf(pathbuf, 1024, __VA_ARGS__); \
		if(can_read(pathbuf)) \
		{ \
			strcpy(output, pathbuf); \
			return true; \
		}\
	} while(0)
	char *envptr;
#define CHECKE(path) CHECK_BASE("%s/%s/%s", envptr, path, name)
#define CHECK(path) CHECK_BASE("%s/%s", path, name)

#ifdef UNIX_LIKE
	if((envptr = getenv("HOME")))
	{
		CHECKE(".config/3ds");
		CHECKE("3ds");
		CHECKE(".3ds");
	}
	CHECK("/usr/share/3ds");
#elif defined(_WIN32)
	if((envptr = getenv("USERPROFILE")))
	{
		CHECKE("3ds");
		CHECKE(".3ds");
	}
	if((envptr = getenv("APPDATA")))
		CHECKE("3ds");
#else
	/* no clue where to look */
	(void) pathbuf;
	(void) envptr;
	(void) output;
	(void) name;
#endif

	/* nothing found */
	return false;
#undef CHECK_BASE
#undef CHECKE
#undef CHECK
}

nnc_result nnc_seeds_seeddb(nnc_rstream *rs, nnc_seeddb *seeddb)
{
	u8 buf[0x20];
	result ret;
	seeddb->size = 0;
	u32 expected_size;
	TRY(read_exact(rs, buf, 0x10));
	expected_size = LE32P(&buf[0x00]);
	if(!(seeddb->entries = malloc(expected_size * sizeof(struct nnc_seeddb_entry))))
		return NNC_R_NOMEM;
	for(u32 i = 0; i < expected_size; ++i, ++seeddb->size)
	{
		TRY(read_exact(rs, buf, 0x20));
		memcpy(seeddb->entries[i].seed, &buf[0x08], NNC_SEED_SIZE);
		seeddb->entries[i].title_id = LE64P(&buf[0x00]);
	}
	return NNC_R_OK;
}

result nnc_scan_seeddb(nnc_seeddb *seeddb)
{
	char path[1024 + 1];
	if(!find_support_file("seeddb.bin", path))
		return NNC_R_NOT_FOUND;
	nnc_file f;
	result ret;
	TRY(nnc_file_open(&f, path));
	ret = nnc_seeds_seeddb(NNC_RSP(&f), seeddb);
	NNC_RS_CALL0(f, close);
	return ret;
}

u8 *nnc_get_seed(nnc_seeddb *seeddb, u64 tid)
{
	for(u32 i = 0; i < seeddb->size; ++i)
	{
		if(seeddb->entries[i].title_id == tid)
			return seeddb->entries[i].seed;
	}
	return NULL;
}

void nnc_free_seeddb(nnc_seeddb *seeddb)
{
	free(seeddb->entries);
}

enum keyfield {
	DEFAULT = 0x02,
	DEV     = 0x04,
	RETAIL  = 0x08,

	TYPE_FIELD = DEV | RETAIL,
};

#define TYPE_FLAG(is_dev) (is_dev ? DEV : RETAIL)

static const struct _kstore {
	const u128 kx_ncch0;
	const u128 kx_ncch1;
	const u128 kx_ncchA;
	const u128 kx_ncchB;
	const u128 ky_comy0;
	const u128 ky_comy1;
	const u128 ky_comy2;
	const u128 ky_comy3;
	const u128 ky_comy4;
	const u128 ky_comy5;
} default_keys[2] =
{
	{	/* retail */
		.kx_ncch0 = NNC_HEX128(0xB98E95CECA3E4D17,1F76A94DE934C053),
		.kx_ncch1 = NNC_HEX128(0xCEE7D8AB30C00DAE,850EF5E382AC5AF3),
		.kx_ncchA = NNC_HEX128(0x82E9C9BEBFB8BDB8,75ECC0A07D474374),
		.kx_ncchB = NNC_HEX128(0x45AD04953992C7C8,93724A9A7BCE6182),
		.ky_comy0 = NNC_HEX128(0x64C5FD55DD3AD988,325BAAEC5243DB98),
		.ky_comy1 = NNC_HEX128(0x4AAA3D0E27D4D728,D0B1B433F0F9CBC8),
		.ky_comy2 = NNC_HEX128(0xFBB0EF8CDBB0D8E4,53CD99344371697F),
		.ky_comy3 = NNC_HEX128(0x25959B7AD0409F72,684198BA2ECD7DC6),
		.ky_comy4 = NNC_HEX128(0x7ADA22CAFFC476CC,8297A0C7CEEEEEBE),
		.ky_comy5 = NNC_HEX128(0xA5051CA1B37DCF3A,FBCF8CC1EDD9CE02),
	},
	{	/* dev */
		.kx_ncch0 = NNC_HEX128(0x510207515507CBB1,8E243DCB85E23A1D),
		.kx_ncch1 = NNC_HEX128(0x81907A4B6F1B4732,3A677974CE4AD71B),
		.kx_ncchA = NNC_HEX128(0x304BF1468372EE64,115EBD4093D84276),
		.kx_ncchB = NNC_HEX128(0x6C8B2944A0726035,F941DFC018524FB6),
		.ky_comy0 = NNC_HEX128(0x55A3F872BDC80C55,5A654381139E153B),
		.ky_comy1 = NNC_HEX128(0x4434ED14820CA1EB,AB82C16E7BEF0C25),
		.ky_comy2 = NNC_HEX128(0xF62E3F958E28A21F,289EEC71A86629DC),
		.ky_comy3 = NNC_HEX128(0x2B49CB6F9998D9AD,94F2EDE7B5DA3E27),
		.ky_comy4 = NNC_HEX128(0x750552BFAA1C0407,55C8D59A55F9AD1F),
		.ky_comy5 = NNC_HEX128(0xAADA4CA8F6E5A977,E0A0F9E476CF0D63),
	}
};

nnc_result nnc_keyset_default(nnc_keyset *ks, bool dev)
{
	const struct _kstore *s = dev ? &default_keys[1] : &default_keys[0];
	u8 type = ks->flags & TYPE_FIELD, mtype = TYPE_FLAG(dev);
	if(type & mtype) return NNC_R_MISMATCH;
	ks->kx_ncch0 = s->kx_ncch0;
	ks->kx_ncch1 = s->kx_ncch1;
	ks->kx_ncchA = s->kx_ncchA;
	ks->kx_ncchB = s->kx_ncchB;
	ks->ky_comy0 = s->ky_comy0;
	ks->ky_comy1 = s->ky_comy1;
	ks->ky_comy2 = s->ky_comy2;
	ks->ky_comy3 = s->ky_comy3;
	ks->ky_comy4 = s->ky_comy4;
	ks->ky_comy5 = s->ky_comy5;
	ks->flags |= DEFAULT | mtype;
	return NNC_R_OK;
}

static const u128 C1_b = NNC_HEX128(0x1FF9E9AAC5FE0408,024591DC5D52768A);
static const u128 *C1 = &C1_b;

static const u128 system_fixed_key = NNC_HEX128(0x527CE630A9CA305F,3696F3CDE954194B);
static const u128 fixed_key = NNC_HEX128(0x0000000000000000,0000000000000000);

static void hwkgen_3ds(u128 *output, u128 *kx, u128 *ky)
{
	*output = *kx;
	nnc_u128_rol(output, 2);
	nnc_u128_xor(output, ky);
	nnc_u128_add(output, C1);
	nnc_u128_ror(output, 41);
}

nnc_result nnc_keyy_seed(nnc_ncch_header *ncch, nnc_u128 *keyy, u8 seed[NNC_SEED_SIZE])
{
	nnc_sha256_hash hashbuf;
	nnc_u8 strbuf[0x20];
	memcpy(strbuf, seed, NNC_SEED_SIZE);
	memcpy(strbuf + NNC_SEED_SIZE, &ncch->title_id, sizeof(ncch->title_id));
	nnc_crypto_sha256(strbuf, hashbuf, 0x18);
	if(U32P(hashbuf) != ncch->seed_hash)
		return NNC_R_CORRUPT;
	nnc_u128_bytes_be(&ncch->keyy, strbuf);
	memcpy(strbuf + 0x10, seed, NNC_SEED_SIZE);
	nnc_crypto_sha256(strbuf, hashbuf, 0x20);
	*keyy = nnc_u128_import_be(hashbuf);
	return NNC_R_OK;
}

static result ky_from_ncch(u128 *ky, nnc_ncch_header *ncch, nnc_seeddb *seeddb)
{
	result ret;
	/* We need to decrypt the keyY first */
	if(ncch->flags & NNC_NCCH_USES_SEED)
	{
		if(!seeddb) return NNC_R_SEED_NOT_FOUND;
		u8 *seed;
		if(!(seed = nnc_get_seed(seeddb, ncch->title_id)))
			return NNC_R_SEED_NOT_FOUND;
		TRY(nnc_keyy_seed(ncch, ky, seed));
	}
	/* it's fine to use */
	else *ky = ncch->keyy;
	return NNC_R_OK;
}

static void key_fixed(nnc_ncch_header *ncch, u128 *output)
{
	*output = nnc_tid_category(ncch->title_id) & NNC_TID_CAT_SYSTEM
		? system_fixed_key : fixed_key;
}

result nnc_key_menu_info(u128 *output, nnc_keyset *ks, nnc_ncch_header *ncch)
{
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return NNC_R_INVAL;
	if(ncch->flags & NNC_NCCH_FIXED_KEY)
		return key_fixed(ncch, output), NNC_R_OK;

	if(!(ks->flags & DEFAULT)) return NNC_R_KEY_NOT_FOUND;
	/* "menu info" always uses keyslot 0x2C and the unencrypted
	 * keyy even if NNC_NCCH_USES_SEED is set (!!!) */
	hwkgen_3ds(output, &ks->kx_ncch0, &ncch->keyy);
	return NNC_R_OK;
}

result nnc_key_content(u128 *output, nnc_keyset *ks, nnc_seeddb *seeddb,
	nnc_ncch_header *ncch)
{
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return NNC_R_INVAL;
	if(ncch->flags & NNC_NCCH_FIXED_KEY)
		return key_fixed(ncch, output), NNC_R_OK;

	result ret; u128 ky; TRY(ky_from_ncch(&ky, ncch, seeddb));

	switch(ncch->crypt_method)
	{
#define CASE(val, key, dep) case val: if(!(ks->flags & (dep))) return NNC_R_KEY_NOT_FOUND; hwkgen_3ds(output, &ks->kx_ncch##key, &ky); break;
	CASE(0x00, 0, DEFAULT)
	CASE(0x01, 1, DEFAULT)
	CASE(0x0A, A, DEFAULT)
	CASE(0x0B, B, DEFAULT)
#undef CASE
	default: return NNC_R_NOT_FOUND;
	}

	return NNC_R_OK;
}

nnc_result nnc_fill_keypair(nnc_keypair *output, nnc_keyset *ks, nnc_seeddb *seeddb,
	struct nnc_ncch_header *ncch)
{
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return NNC_R_OK; // no need to do any key generating
	result ret;
	TRY(nnc_key_content(&output->secondary, ks, seeddb, ncch));
	return nnc_key_menu_info(&output->primary, ks, ncch);
}

result nnc_get_ncch_iv(struct nnc_ncch_header *ncch, u8 for_section,
	u8 counter[0x10])
{
	if(ncch->flags & NNC_NCCH_NO_CRYPTO)
		return NNC_R_INVAL;
	if(for_section < NNC_SECTION_EXHEADER || for_section > NNC_SECTION_ROMFS)
		return NNC_R_INVAL;
	u64 *ctr64 = (u64 *) counter;
	u32 *ctr32 = (u32 *) counter;
	switch(ncch->version)
	{
	case 0:
	case 2:
		ctr64[0] = BE64(ncch->partition_id);
		ctr64[1] = 0;
		counter[8] = for_section;
		break;
	case 1:
		ctr64[0] = LE64(ncch->partition_id);
		ctr32[2] = 0;
		switch(for_section)
		{
		case NNC_SECTION_EXHEADER:
			ctr32[3] = BE32(NNC_MEDIA_UNIT);
			break;
		case NNC_SECTION_EXEFS:
			ctr32[3] = BE32(NNC_MU_TO_BYTE(ncch->exefs_offset));
			break;
		case NNC_SECTION_ROMFS:
			ctr32[3] = BE32(NNC_MU_TO_BYTE(ncch->romfs_offset));
			break;
		}
		break;
	default:
		return NNC_R_UNSUPPORTED;
	}
	return NNC_R_OK;
}

/* nnc_aes_ctr */

static void redo_ctr_iv(nnc_aes_ctr *ac, u32 offset)
{
	u128 ctr = NNC_PROMOTE128(offset / 0x10);
	nnc_u128_add(&ctr, &ac->iv);
	nnc_u128_bytes_be(&ctr, ac->ctr);
}

static result aes_ctr_read(nnc_aes_ctr *self, u8 *buf, u32 max, u32 *totalRead)
{
	if(max % 0x10 != 0) return NNC_R_BAD_ALIGN;

	result ret;
	TRY(NNC_RS_PCALL(self->child, read, buf, max, totalRead));

	/* if we read unaligned we need to align it down */
	if(*totalRead % 0x10 != 0) *totalRead = ALIGN(*totalRead, 0x10) - 0x10;

	/* now we gotta decrypt *totalRead bytes in buf */
	u8 block[0x10];
	size_t of = 0;
	mbedtls_aes_crypt_ctr(self->crypto_ctx, *totalRead, &of, self->ctr, block, buf, buf);

	return NNC_R_OK;
}

static result aes_ctr_seek_abs(nnc_aes_ctr *self, u32 pos)
{
	if(pos % 0x10 != 0) return NNC_R_BAD_ALIGN;
	u32 cpos = NNC_RS_PCALL0(self->child, tell);
	/* i doubt this will happen but it's here anyways
	 * to save a bit of time. */
	if(cpos == pos) return NNC_R_OK;
	NNC_RS_PCALL(self->child, seek_abs, pos);
	redo_ctr_iv(self, pos);
	return NNC_R_OK;
}

static result aes_ctr_seek_rel(nnc_aes_ctr *self, u32 pos)
{
	if(pos % 0x10 != 0) return NNC_R_BAD_ALIGN;
	/* i doubt this will happen but it's here anyways
	 * to save a bit of time. */
	if(pos == 0) return NNC_R_OK;
	pos = NNC_RS_PCALL0(self->child, tell) + pos;
	NNC_RS_PCALL(self->child, seek_abs, pos);
	redo_ctr_iv(self, pos);
	return NNC_R_OK;
}

static u32 aes_ctr_size(nnc_aes_ctr *self)
{
	return NNC_RS_PCALL0(self->child, size);
}

static u32 aes_ctr_tell(nnc_aes_ctr *self)
{
	return NNC_RS_PCALL0(self->child, tell);
}

static void aes_ctr_close(nnc_aes_ctr *self)
{
	mbedtls_aes_free(self->crypto_ctx);
	free(self->crypto_ctx);
}

static const nnc_rstream_funcs aes_ctr_funcs = {
	.read = (nnc_read_func) aes_ctr_read,
	.seek_abs = (nnc_seek_abs_func) aes_ctr_seek_abs,
	.seek_rel = (nnc_seek_rel_func) aes_ctr_seek_rel,
	.size = (nnc_size_func) aes_ctr_size,
	.close = (nnc_close_func) aes_ctr_close,
	.tell = (nnc_tell_func) aes_ctr_tell,
};

nnc_result nnc_aes_ctr_open(nnc_aes_ctr *self, nnc_rstream *child, u128 *key, u8 iv[0x10])
{
	self->funcs = &aes_ctr_funcs;
	if(!(self->crypto_ctx = malloc(sizeof(mbedtls_aes_context))))
		return NNC_R_NOMEM;
	self->iv = nnc_u128_import_be(iv);
	self->child = child;

	u8 buf[0x10];
	nnc_u128_bytes_be(key, buf);
	mbedtls_aes_setkey_enc(self->crypto_ctx, buf, 128);

	redo_ctr_iv(self, 0);
	return NNC_R_OK;
}

static nnc_result redo_cbc_iv(nnc_aes_cbc *self, u32 offset)
{
	if(offset == 0) memcpy(self->iv, self->init_iv, 0x10);
	else
	{
		result ret, saved;
		/* the new IV is the previous encrypted block */
		TRY(NNC_RS_PCALL(self->child, seek_abs, NNC_RS_PCALL0(self->child, tell) - 16));
		u32 read;
		saved = NNC_RS_PCALL(self->child, read, self->iv, 16, &read);
		if(saved != NNC_R_OK || read != 0x10)
			return NNC_R_TOO_SMALL;
	}
	return NNC_R_OK;
}

static result aes_cbc_read(nnc_aes_cbc *self, u8 *buf, u32 max, u32 *totalRead)
{
	if(max % 0x10 != 0) return NNC_R_BAD_ALIGN;

	result ret;
	TRY(NNC_RS_PCALL(self->child, read, buf, max, totalRead));

	/* if we read unaligned we need to align it down */
	if(*totalRead % 0x10 != 0) *totalRead = ALIGN(*totalRead, 0x10) - 0x10;

	/* now we gotta decrypt *totalRead bytes in buf */
	mbedtls_aes_crypt_cbc(self->crypto_ctx, MBEDTLS_AES_DECRYPT, *totalRead, self->iv,
		buf, buf);
	return NNC_R_OK;
}

static result aes_cbc_seek_abs(nnc_aes_cbc *self, u32 pos)
{
	if(pos % 0x10 != 0) return NNC_R_BAD_ALIGN;
	u32 cpos = NNC_RS_PCALL0(self->child, tell);
	/* i doubt this will happen but it's here anyways
	 * to save a bit of time. */
	if(cpos == pos) return NNC_R_OK;
	NNC_RS_PCALL(self->child, seek_abs, pos);
	return redo_cbc_iv(self, pos);
}

static result aes_cbc_seek_rel(nnc_aes_cbc *self, u32 pos)
{
	if(pos % 0x10 != 0) return NNC_R_BAD_ALIGN;
	/* i doubt this will happen but it's here anyways
	 * to save a bit of time. */
	if(pos == 0) return NNC_R_OK;
	pos = NNC_RS_PCALL0(self->child, tell) + pos;
	NNC_RS_PCALL(self->child, seek_abs, pos);
	return redo_cbc_iv(self, pos);
}

static u32 aes_cbc_size(nnc_aes_cbc *self)
{
	return NNC_RS_PCALL0(self->child, size);
}

static u32 aes_cbc_tell(nnc_aes_cbc *self)
{
	return NNC_RS_PCALL0(self->child, tell);
}

static void aes_cbc_close(nnc_aes_cbc *self)
{
	mbedtls_aes_free(self->crypto_ctx);
	free(self->crypto_ctx);
}

static const nnc_rstream_funcs aes_cbc_funcs = {
	.read = (nnc_read_func) aes_cbc_read,
	.seek_abs = (nnc_seek_abs_func) aes_cbc_seek_abs,
	.seek_rel = (nnc_seek_rel_func) aes_cbc_seek_rel,
	.size = (nnc_size_func) aes_cbc_size,
	.close = (nnc_close_func) aes_cbc_close,
	.tell = (nnc_tell_func) aes_cbc_tell,
};

nnc_result nnc_aes_cbc_open(nnc_aes_cbc *self, nnc_rstream *child, u8 key[0x10], u8 iv[0x10])
{
	self->funcs = &aes_cbc_funcs;
	if(!(self->crypto_ctx = malloc(sizeof(mbedtls_aes_context))))
		return NNC_R_NOMEM;
	memcpy(self->init_iv, iv, 0x10);
	memcpy(self->iv, iv, 0x10);
	self->child = child;

	mbedtls_aes_setkey_dec(self->crypto_ctx, key, 128);

	return NNC_R_OK;
}

result nnc_decrypt_tkey(nnc_ticket *tik, nnc_keyset *ks, nnc_u8 decrypted[0x10])
{
	u128 *used_keyy;
	switch(tik->common_keyy)
	{
	case 0: used_keyy = &ks->ky_comy0; break;
	case 1: used_keyy = &ks->ky_comy1; break;
	case 2: used_keyy = &ks->ky_comy2; break;
	case 3: used_keyy = &ks->ky_comy3; break;
	case 4: used_keyy = &ks->ky_comy4; break;
	case 5: used_keyy = &ks->ky_comy5; break;
	default: return NNC_R_CORRUPT; /* invalid key selected */
	}
	u64 iv[2] = { BE64(tik->title_id), 0 };
	mbedtls_aes_context ctx;
	u8 buf[0x10];

	nnc_u128_bytes_be(used_keyy, buf);
	mbedtls_aes_setkey_dec(&ctx, buf, 128);
	mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, 0x10, (u8 *) iv,
		tik->title_key, decrypted);
	mbedtls_aes_free(&ctx);
	return NNC_R_OK;
}

