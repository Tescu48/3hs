/** \file  romfs.h
 *  \brief Functions relating to RomFS.
 *  \see   https://www.3dbrew.org/wiki/RomFS
 */
#ifndef inc_nnc_romfs_h
#define inc_nnc_romfs_h
#include <nnc/read-stream.h>
#include <nnc/base.h>
NNC_BEGIN

typedef struct nnc_romfs_header {
	struct nnc_romfs_header_oflen {
		nnc_u64 offset;    ///< Offset.
		nnc_u32 length;    ///< Length.
	}	dir_hash,          ///< Directory hash table.
		dir_meta,          ///< Directory metadata.
		file_hash,         ///< File hash table.
		file_meta;         ///< File metadata.
	nnc_u64 data_offset; ///< File data offset.
} nnc_romfs_header;

typedef struct nnc_romfs_ctx {
	struct nnc_romfs_header header;
	nnc_u32 *file_hash_tab;
	nnc_u32 *dir_hash_tab;
	nnc_u8 *file_meta_data;
	nnc_u8 *dir_meta_data;
	nnc_rstream *rs;
} nnc_romfs_ctx;

/** Information about either a directory or file in RomFS. */
typedef struct nnc_romfs_info {
	enum nnc_romfs_type {
		NNC_ROMFS_FILE, ///< File.
		NNC_ROMFS_DIR,  ///< Directory.
		NNC_ROMFS_NONE, ///< Not found.
	} type; ///< Information type.
	union nnc_romfs_info_union {
		struct nnc_romfs_dir_info {
			nnc_u32 fchildren;  ///< Offset to child files.
			nnc_u32 dchildren;  ///< Offset to child directories.
			nnc_u32 sibling;    ///< Offset to next sibling directory.
			nnc_u32 parent;     ///< Offset to parent directory.
		} d; ///< Directory information.
		struct nnc_romfs_file_info {
			nnc_u32 sibling;  ///< Offset to next sibling file.
			nnc_u32 parent;   ///< Offset to parent directory.
			nnc_u64 offset;   ///< Offset to file data.
			nnc_u64 size;     ///< Size of file data.
		} f; ///< File information.
	} u; ///< Information union, d for type == DIR, f for type == FILE.
	nnc_u32 filename_length; ///< Length of filename.
	const nnc_u16 *filename; ///< Filename (utf16).
} nnc_romfs_info;

typedef struct nnc_romfs_iterator {
	nnc_romfs_info *dir;
	nnc_romfs_ctx *ctx;
	nnc_u32 next;
	bool in_dir;
} nnc_romfs_iterator;


/** \brief      Gets the next directory entry in the iterator. Usage:
 *  \code
 *  while(nnc_romfs_next(&it, &ent))
 *    // ...
 *  \endcode
 *  \param it   The iterator from \ref nnc_romfs_mkit.
 *  \param ent  Output entry.
 *  \returns    If the entry has been updated.
 */
int nnc_romfs_next(nnc_romfs_iterator *it, nnc_romfs_info *ent);

/** \brief      Makes a new iterator.
 *  \param ctx  The context from \ref nnc_init_romfs the directory belongs to.
 *  \param dir  A directory to create an iterator to.
 *  \note       A pointer is saved to the directory in the iterator
 *              so make sure to not use the \p dir until you are done
 *              with the iterator.
 */
nnc_romfs_iterator nnc_romfs_mkit(nnc_romfs_ctx *ctx, nnc_romfs_info *dir);

/** \brief       Looks up information of a path.
 *  \param ctx   Context from \ref nnc_init_romfs.
 *  \param info  Output info.
 *  \param path  The absolute RomFS path to the file or directory.
 */
nnc_result nnc_get_info(nnc_romfs_ctx *ctx, nnc_romfs_info *info, const char *path);

/** \brief       Opens a RomFS file in a subview \ref nnc_rstream.
 *  \param ctx   Context from \ref nnc_init_romfs.
 *  \param sv    Output subview.
 *  \param info  \ref nnc_romfs_info for the desired file.
 */
nnc_result nnc_romfs_open_subview(nnc_romfs_ctx *ctx, nnc_subview *sv, nnc_romfs_info *info);

/** \brief      Prepare a context for use with various other RomFS-related functions.
 *  \param rs   Stream to read RomFS from.
 *  \param ctx  Output context.
 *  \note       This function allocates dynamic memory so make sure to free
 *              \p ctx with \ref nnc_free_romfs.
 *  \note       If this function does not return NNC_R_OK you musn't call \ref nnc_free_romfs
 */
nnc_result nnc_init_romfs(nnc_rstream *rs, nnc_romfs_ctx *ctx);

/** \brief      Free memory used by a context
 *  \param ctx  Context from \ref nnc_init_romfs.
 */
void nnc_free_romfs(nnc_romfs_ctx *ctx);

/** \brief        Reads the header of a RomFS. You probably want to use
 *                \ref nnc_init_romfs and related instead.
 *  \param rs     Stream to read RomFS from.
 *  \param romfs  Output RomFS header.
 */
nnc_result nnc_read_romfs_header(nnc_rstream *rs, nnc_romfs_header *romfs);

NNC_END
#endif

