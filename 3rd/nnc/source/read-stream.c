
#include <nnc/read-stream.h>
#include <stdlib.h>
#include <string.h>
#include "./internal.h"

static result file_read(nnc_file *self, u8 *buf, u32 max, u32 *totalRead)
{
	u32 total = fread(buf, 1, max, self->f);
	if(totalRead) *totalRead = total;
	if(total != max)
		return ferror(self->f) ? NNC_R_FAIL_READ : NNC_R_OK;
	return NNC_R_OK;
}

static result file_seek_abs(nnc_file *self, u32 pos)
{
	if(pos >= self->size) return NNC_R_SEEK_RANGE;
	fseek(self->f, pos, SEEK_SET);
	return NNC_R_OK;
}

static result file_seek_rel(nnc_file *self, u32 pos)
{
	u32 npos = ftell(self->f) + pos;
	if(npos >= self->size) return NNC_R_SEEK_RANGE;
	fseek(self->f, npos, SEEK_SET);
	return NNC_R_OK;
}

static u32 file_size(nnc_file *self)
{
	return self->size;
}

static void file_close(nnc_file *self)
{
	fclose(self->f);
}

static u32 file_tell(nnc_file *self)
{
	return ftell(self->f);
}

static const nnc_rstream_funcs file_funcs = {
	.read = (nnc_read_func) file_read,
	.seek_abs = (nnc_seek_abs_func) file_seek_abs,
	.seek_rel = (nnc_seek_rel_func) file_seek_rel,
	.size = (nnc_size_func) file_size,
	.close = (nnc_close_func) file_close,
	.tell = (nnc_tell_func) file_tell,
};

result nnc_file_open(nnc_file *self, const char *name)
{
	self->f = fopen(name, "rb");
	if(!self->f) return NNC_R_FAIL_OPEN;

	fseek(self->f, 0, SEEK_END);
	self->size = ftell(self->f);
	fseek(self->f, 0, SEEK_SET);

	self->funcs = &file_funcs;
	return NNC_R_OK;
}

static result mem_read(nnc_memory *self, u8 *buf, u32 max, u32 *totalRead)
{
	*totalRead = MIN(max, self->size);
	memcpy(buf, ((u8 *) self->ptr) + self->pos, *totalRead);
	self->pos += *totalRead;
	return NNC_R_OK;
}

static result mem_seek_abs(nnc_memory *self, u32 pos)
{
	if(pos >= self->size) return NNC_R_SEEK_RANGE;
	self->pos = pos;
	return NNC_R_OK;
}

static result mem_seek_rel(nnc_memory *self, u32 pos)
{
	u32 npos = self->pos + pos;
	if(npos >= self->size) return NNC_R_SEEK_RANGE;
	self->pos = npos;
	return NNC_R_OK;
}

static u32 mem_size(nnc_memory *self)
{
	return self->size;
}

static void mem_close(nnc_memory *self)
{
	/* nothing to do ... */
	(void) self;
}

static void mem_own_close(nnc_memory *self)
{
	free(self->ptr);
}

static u32 mem_tell(nnc_memory *self)
{
	return self->pos;
}

static const nnc_rstream_funcs mem_funcs = {
	.read = (nnc_read_func) mem_read,
	.seek_abs = (nnc_seek_abs_func) mem_seek_abs,
	.seek_rel = (nnc_seek_rel_func) mem_seek_rel,
	.size = (nnc_size_func) mem_size,
	.close = (nnc_close_func) mem_close,
	.tell = (nnc_tell_func) mem_tell,
};

static const nnc_rstream_funcs mem_own_funcs = {
	.read = (nnc_read_func) mem_read,
	.seek_abs = (nnc_seek_abs_func) mem_seek_abs,
	.seek_rel = (nnc_seek_rel_func) mem_seek_rel,
	.size = (nnc_size_func) mem_size,
	.close = (nnc_close_func) mem_own_close,
	.tell = (nnc_tell_func) mem_tell,
};

void nnc_mem_open(nnc_memory *self, void *ptr, u32 size)
{
	self->funcs = &mem_funcs;
	self->size = size;
	self->ptr = ptr;
	self->pos = 0;
}

void nnc_mem_own_open(nnc_memory *self, void *ptr, u32 size)
{
	self->funcs = &mem_own_funcs;
	self->size = size;
	self->ptr = ptr;
	self->pos = 0;
}

static result subview_read(nnc_subview *self, u8 *buf, u32 max, u32 *totalRead)
{
	u32 sizeleft = self->size - self->pos;
	max = MIN(max, sizeleft);
	result ret;
	/* seek to correct offset in child */
	TRY(NNC_RS_PCALL(self->child, seek_abs, self->off + self->pos));
	ret = NNC_RS_PCALL(self->child, read, buf, max, totalRead);
	self->pos += *totalRead;
	return ret;
}

static result subview_seek_abs(nnc_subview *self, u32 pos)
{
	if(pos >= self->size) return NNC_R_SEEK_RANGE;
	self->pos = pos;
	return NNC_R_OK;
}

static result subview_seek_rel(nnc_subview *self, u32 pos)
{
	u32 npos = self->pos + pos;
	if(npos >= self->size) return NNC_R_SEEK_RANGE;
	self->pos = npos;
	return NNC_R_OK;
}

static u32 subview_size(nnc_subview *self)
{
	return self->size;
}

static void subview_close(nnc_subview *self)
{
	/* nothing to do... */
	(void) self;
}

static nnc_u32 subview_tell(nnc_subview *self)
{
	return self->pos;
}

static const nnc_rstream_funcs subview_funcs = {
	.read = (nnc_read_func) subview_read,
	.seek_abs = (nnc_seek_abs_func) subview_seek_abs,
	.seek_rel = (nnc_seek_rel_func) subview_seek_rel,
	.size = (nnc_size_func) subview_size,
	.close = (nnc_close_func) subview_close,
	.tell = (nnc_tell_func) subview_tell,
};

void nnc_subview_open(nnc_subview *self, nnc_rstream *child, nnc_u32 off, nnc_u32 len)
{
	self->funcs = &subview_funcs;
	self->child = child;
	self->size = len;
	self->off = off;
	self->pos = 0;
}

