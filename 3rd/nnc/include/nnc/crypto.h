/** \file   crypto.h
 *  \brief  Functions relating to encryption, decryption and hashing.
 */
#ifndef inc_nnc_crypto_h
#define inc_nnc_crypto_h

#include <nnc/read-stream.h>
#include <nnc/ticket.h>
#include <nnc/u128.h>
#include <nnc/base.h>
NNC_BEGIN

/** Initializer for \ref nnc_keyset, note that it does not initialize any keys,
 *  just the flags field. */
#define NNC_KEYSET_INIT { .flags = 0 }

/** Size of a seed. */
#define NNC_SEED_SIZE 0x10

/** Buffer large enough to hold a sha256 hash. */
typedef nnc_u8 nnc_sha256_hash[0x20];

/** Container struct to hold keys which you should initialize with \ref NNC_KEYSET_INIT. */
typedef struct nnc_keyset {
	nnc_u8 flags;      ///< bit1 = default keys available, bit2 = developer keys, bit3 = retail keys.
	nnc_u128 kx_ncch0; ///< Keyslot 0x2C.
	nnc_u128 kx_ncch1; ///< Keyslot 0x25.
	nnc_u128 kx_ncchA; ///< Keyslot 0x18.
	nnc_u128 kx_ncchB; ///< Keyslot 0x1B.
	nnc_u128 ky_comy0; ///< Common Key 0 (application).
	nnc_u128 ky_comy1; ///< Common Key 1 (system).
	nnc_u128 ky_comy2; ///< Common key 2.
	nnc_u128 ky_comy3; ///< Common key 3.
	nnc_u128 ky_comy4; ///< Common key 4.
	nnc_u128 ky_comy5; ///< Common key 5.
} nnc_keyset;

/** Container struct to hold seeds. */
typedef struct nnc_seeddb {
	nnc_u32 size;
	struct nnc_seeddb_entry {
		nnc_u8 seed[NNC_SEED_SIZE];
		nnc_u64 title_id;
	} *entries;
} nnc_seeddb;

enum nnc_section {
	NNC_SECTION_EXHEADER  = 1, ///< NCCH extended header section.
	NNC_SECTION_EXEFS     = 2, ///< NCCH ExeFS section.
	NNC_SECTION_ROMFS     = 3, ///< NCCH RomFS section.
};

typedef struct nnc_aes_ctr {
	const struct nnc_rstream_funcs *funcs;
	void *crypto_ctx; ///< Context for the cryptographic library used.
	nnc_rstream *child;
	nnc_u8 ctr[0x10];
	nnc_u128 iv;
} nnc_aes_ctr;

typedef struct nnc_aes_cbc {
	const struct nnc_rstream_funcs *funcs;
	void *crypto_ctx; ///< Context for the cryptographic library used.
	nnc_rstream *child;
	nnc_u8 init_iv[0x10];
	nnc_u8 iv[0x10];
} nnc_aes_cbc;

typedef struct nnc_keypair {
	nnc_u128 primary;   ///< Also known as the "info menu" key.
	nnc_u128 secondary; ///< Also known as the "content" key.
} nnc_keypair;


/** \brief          Hash a \ref nnc_rstream partly.
 *  \param rs       Stream to hash.
 *  \param digest   Output digest.
 *  \param size     Amount of data to hash.
 *  \returns
 *  \p NNC_R_TOO_SMALL => \p rs is smaller than \p size.
 */
nnc_result nnc_crypto_sha256_part(nnc_rstream *rs, nnc_sha256_hash digest, nnc_u32 size);

/** \brief          Hash a buffer.
 *  \param buf      Buffer to hash.
 *  \param digest   Output digest.
 *  \param size     Size of buffer.
 *  \returns
 *  Always succeeds currently.
 */
nnc_result nnc_crypto_sha256(const nnc_u8 *buf, nnc_sha256_hash digest, nnc_u32 size);

/** \brief      Sets default keys in a keyset.
 *              Keys set: kx_ncch1, kx_ncchA, kx_ncchB.
 *  \param ks   Output keyset.
 *  \param dev  Whether or not to use a developer set.
 */
nnc_result nnc_keyset_default(nnc_keyset *ks, bool dev);

/** \brief         Extracts all seeds from a SeedDB.
 *                 If this functions fails it is still safe to call
 *                 \ref nnc_get_seed on but it will never return any seed
 *                 and \ref nnc_free_seeddb will not free anything.
 *  \param rs      Stream to read SeedDB from.
 *  \param seeddb  Output SeedDB.
 *  \note          This function allocates dynamic memory so be sure to free
 *                 it with \ref nnc_free_seeddb.
 *  \returns
 *  Anything \p rs->read() can return.\n
 *  \p NNC_R_NOMEM => Failed to allocate memory for seeds.
 */
nnc_result nnc_seeds_seeddb(nnc_rstream *rs, nnc_seeddb *seeddb);

/** \brief         Search in OS-specific directories for seeddb.bin and extract keys from a SeedDB.
 *                 This function scans:\n
 *                 - Linux/MacOS: $HOME/.config/3ds, $HOME/3ds, $HOME/.3ds, /usr/share/3ds.\n
 *                 - Windows: %USERPROFILE%/3ds, %USERPROFILE%/.3ds, %APPDATA/3ds
 *  \param seeddb  Output SeedDB.
 *  \note          This function allocates dynamic memory so be sure to free
 *                 it with \ref nnc_free_seeddb.
 *  \returns
 *  Anything \ref nnc_seeds_seeddb can return.\n
 *  Anything \ref nnc_file_open can return.\n
 *  \p NNC_R_NOT_FOUND => seeddb.bin not found in checked paths.
 */
nnc_result nnc_scan_seeddb(nnc_seeddb *seeddb);

/** \brief         Get a seed from a SeedDB.
 *  \param tid     Title ID to search for.
 *  \param seeddb  SeedDB to search in.
 *  \returns       Pointer to seed if found, else NULL.
 */
nnc_u8 *nnc_get_seed(nnc_seeddb *seeddb, nnc_u64 tid);

/** \brief         Frees dynamic memory allocated by \ref nnc_seeds_seeddb or
 *                 \ref nnc_scan_seeddb.
 *  \param seeddb  SeedDB to free.
 */
void nnc_free_seeddb(nnc_seeddb *seeddb);

/* forward declaration from ncch.h */
struct nnc_ncch_header;

/** \brief       Gets the actual keyY if a seed is used.
 *  \param ncch  NCCH to get keyY for.
 *  \param keyy  Output keyY.
 *  \param seed  Input seed.
 *  \returns
 *  \p NNC_R_CORRUPT => Seed is invalid.
 */
nnc_result nnc_keyy_seed(struct nnc_ncch_header *ncch, nnc_u128 *keyy,
	nnc_u8 seed[NNC_SEED_SIZE]);

/** \brief         Gets the normal key of a region in the menu info group.
 *  \param output  Output key
 *  \param ks      Keyset.
 *  \param ncch    NCCH to get key from.
 *  \returns
 *  \p NNC_R_KEY_NOT_FOUND => KeyX not found.\n
 *  \p NNC_R_INVAL => NCCH is not encrypted.
 */
nnc_result nnc_key_menu_info(nnc_u128 *output, nnc_keyset *ks, struct nnc_ncch_header *ncch);

/** \brief         Gets the normal key of region in the content group.
 *  \param output  Output key.
 *  \param ks      Keyset.
 *  \param ncch    NCCH to get key from.
 *  \param seeddb  SeedDB to fetch seeds from if required.
 *                 If \p seeddb is NULL this function will return
 *                 NNC_R_SEED_NOT_FOUND for any NCCH that requires a seed.
 *  \returns
 *  Anything \ref nnc_get_seed can return.\n
 *  \p NNC_R_KEY_NOT_FOUND => KeyX not found.\n
 *  \p NNC_R_CORRUPT => Seed is invalid.\n
 *  \p NNC_R_INVAL => NCCH is not encrypted.
 */
nnc_result nnc_key_content(nnc_u128 *output, nnc_keyset *ks, nnc_seeddb *seeddb, struct nnc_ncch_header *ncch);

/** \brief              Gets the initial counter.
 *  \param ncch         NCCH to get counter for.
 *  \param for_section  Section to get counter for, see \ref nnc_section.
 *  \param counter      Output counter.
 *  \returns
 *  \p NNC_R_UNSUPPORTED => Unsupported NCCH version.
 *  \p NNC_R_INVAL => \p for_section is invalid or NCCH is not encrypted.
 */
nnc_result nnc_get_ncch_iv(struct nnc_ncch_header *ncch, nnc_u8 for_section,
	nnc_u8 counter[0x10]);

/** \brief        Decrypt an AES-CTR stream on-the-fly.
 *  \param self   Output AES-CTR stream.
 *  \param child  Child stream to decrypt from.
 *  \param key    Encryption key.
 *  \param iv     Initial counter.
 *  \warning      As this stream does not keep it's own offsets, do not open one multiple
 *                times to one substream.
 *  \note         All operations to this stream should be aligned to 0x10 bytes.
 *  \note         Calling close on this stream doesn't close the substream.
 *  \returns
 *  \p NNC_R_NOMEM => Failed to allocate AES-CTR context.
 */
nnc_result nnc_aes_ctr_open(nnc_aes_ctr *self, nnc_rstream *child, nnc_u128 *key,
	nnc_u8 iv[0x10]);

/** \brief        Decrypt an AES-CBC stream on-the-fly.
 *  \param self   Output AES-CBC stream.
 *  \param child  Child stream to decrypt from.
 *  \param key    Encryption key.
 *  \param iv     IV.
 *  \warning      As this stream does not keep it's own offsets, do not open one multiple
 *                times to one substream.
 *  \note         All operations to this stream should be aligned to 0x10 bytes.
 *  \note         Calling close on this stream doesn't close the substream.
 *  \returns
 *  \p NNC_R_NOMEM => Failed to allocate AES-CBC context.
 */
nnc_result nnc_aes_cbc_open(nnc_aes_cbc *self, nnc_rstream *child, nnc_u8 key[0x10],
	nnc_u8 iv[0x10]);

/** \brief         Get a key pair for an NCCH.
 *  \param output  Output keypair.
 *  \param ks      Keyset from \ref nnc_keyset_default.
 *  \param seeddb  SeedDB. See comments in \ref nnc_key_content.
 *  \param ncch    NCCH to get keypair for.
 *  \note          If the NCCH is decrypted, this function will do nothing.
 *                 So if you know for a fact the NCCH is decrypted passing NULL
 *                 as keypair for any function that takes it will have the same
 *                 effect as passing the return of this function.
 *  \returns
 *  Anything \ref nnc_key_menu_info can return.\n
 *  Anything \ref nnc_key_content can return.
 */
nnc_result nnc_fill_keypair(nnc_keypair *output, nnc_keyset *ks, nnc_seeddb *seeddb,
	struct nnc_ncch_header *ncch);

/** \brief            Decrypt a title key from a ticket.
 *  \param tik        Ticket from \ref nnc_read_ticket.
 *  \param ks         Keyset from \ref nnc_keyset_default.
 *  \param decrypted  Output decrypted title key.
 *  \returns
 *  \p NNC_R_CORRUPT => Ticket keyY is invalid.
 */
nnc_result nnc_decrypt_tkey(nnc_ticket *tik, nnc_keyset *ks, nnc_u8 decrypted[0x10]);

NNC_END
#endif

