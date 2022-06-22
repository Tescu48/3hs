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

#include <ui/theme.hh>
#include <ui/base.hh>
#include <algorithm>

#include "image_ldr.hh"
#include "update.hh"
#include "panic.hh"
#include "log.hh"

#define BASE_THEME "romfs:/light.hstx"

#define VERSION_INT ((VERSION_MAJOR << 20) | (VERSION_MINOR << 10) | (VERSION_PATCH))
/* 3DS is Little Endian ... */
#define U16(a) (__builtin_bswap16(a))
#define U32(a) (__builtin_bswap32(a))
#define DESCRIPTOR_MAX 128

struct hstx_header {
	char magic[4];
	u32 format_version;
	u32 target_version;
	u32 num_descriptors;
	u32 blob_size;
	u32 name_offset;
	u32 author_offset;
	u32 reserved0[5];
};

struct hstx_descriptor {
	u32 ident;
	union hstx_descriptor_data {
		struct hstx_descriptor_data_color {
			u32 value;
			u32 reserved1[2];
		} color;
		struct hstx_descriptor_data_image {
			u32 img_ptr;
			u16 w;
			u16 h;
			u32 reserved2;
		} image;
	} data;
};

enum hstx_ident {
	ID_BG_CLR            = 0x1001,
	ID_TEXT_CLR          = 0x1002,
	ID_BTN_BG_CLR        = 0x1003,
	ID_BTN_BORDER_CLR    = 0x1004,
	ID_BATTERY_GREEN_CLR = 0x1005,
	ID_BATTERY_RED_CLR   = 0x1006,
	ID_TOGGLE_GREEN_CLR  = 0x1007,
	ID_TOGGLE_RED_CLR    = 0x1008,
	ID_TOGGLE_SLID_CLR   = 0x1009,
	ID_PROGBAR_FG_CLR    = 0x1010,
	ID_PROGBAR_BG_CLR    = 0x1011,
	ID_SCROLLBAR_CLR     = 0x1012,
	ID_LED_GREEN_CLR     = 0x1013,
	ID_LED_RED_CLR       = 0x1014,
	ID_SMDH_BORDER_CLR   = 0x1015,

	ID_MORE_IMG          = 0x2001,
	ID_BATTERY_IMG       = 0x2002,
	ID_SEARCH_IMG        = 0x2003,
	ID_SETTINGS_IMG      = 0x2004,
	ID_SPINNER_IMG       = 0x2005,
	ID_RANDOM_IMG        = 0x2006,
};

static ui::ThemeManager manager;
static ui::Theme        ctheme;


ui::Theme *ui::Theme::global()
{ return &ctheme; }

void ui::Theme::cleanup_images()
{
	for(u32 i = ui::theme::max_color + 1; i < ui::theme::max; ++i)
		if(this->descriptors[i].image.actual_image.tex && this->descriptors[i].image.isOwn)
			delete_image(this->descriptors[i].image.actual_image);
	this->clear();
}

bool ui::Theme::open(const char *filename, ui::Theme *base, u8 flags)
{
	this->cleanup_images();
	if(base) this->replace_without_meta(*base);
	return this->parse(filename, flags);
}

void ui::Theme::clear()
{
	memset(this->descriptors, 0, sizeof(this->descriptors));
}

void ui::Theme::replace_with(ui::Theme& other)
{
	this->replace_without_meta(other);
	this->author = other.author;
	this->name = other.name;
}

void ui::Theme::replace_without_meta(ui::Theme& other)
{
	/* this memcpy will only copy the images as a reference, and colors entirely */
	memcpy(this->descriptors, other.descriptors, sizeof(this->descriptors));
	for(u32 i = ui::theme::max_color + 1; i < ui::theme::max; ++i)
		this->descriptors[i].image.isOwn = false;
}

bool ui::Theme::parse(const char *filename, u8 flags)
{
#define EXIT_LOG(...) do { elog(__VA_ARGS__); goto out; } while(0)
	FILE *f = fopen(filename, "r");
	if(!f) { elog("theme parser: failed to open %s", filename); return false; }
	bool ret = false;
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	u8 *foot = NULL, head[0x30], *blob_addr, *blob_base;
	u32 format_version, target_version, blob_size, num_descriptors, blob_rel_addr;
	hstx_descriptor *descriptors;
	hstx_header *hdr;
	bool isReplacing;
	if(fread(head, 0x30, 1, f) != 1) EXIT_LOG("theme parser: failed to read 0x30 from %s", filename);

	hdr = (hstx_header *) &head[0x00];
	format_version = U32(hdr->format_version);
	target_version = U32(hdr->target_version);
	blob_size = U32(hdr->blob_size);
	num_descriptors = U32(hdr->num_descriptors);

	if(format_version != 0) EXIT_LOG("theme parser: unknown/unsupported version %lu", format_version);
	if(num_descriptors > DESCRIPTOR_MAX) EXIT_LOG("too many descriptors %lu", num_descriptors);
	/* this check should probably be done before we read the entire file */
	if(size != 0x30 + 0x10 * num_descriptors + blob_size) EXIT_LOG("theme parser: file size as expected");

	if(memcmp(hdr->magic, "HSTX", 4) != 0) EXIT_LOG("theme parser: invalid magic %.4s", hdr->magic);
	ilog("theme parser: target_version is %s the current_version (%lu vs %u), v%lu theme",
		target_version == VERSION_INT ? "equal to"
			: target_version > VERSION_INT ? "greater than"
				: "lesser than", target_version, VERSION_INT, format_version);

	size -= 0x30;
	foot = (u8 *) malloc(size + 1);
	if(!foot) EXIT_LOG("theme parser: failed to allocate %u", size + 1);
	if(fread(foot, 1, size, f) != size) EXIT_LOG("theme parser: failed to read %u from %s", size, filename);
	foot[size] = '\0';

	descriptors = (hstx_descriptor *) &foot[0x0];
	blob_base = &foot[0x10 * num_descriptors];

#define GETBLOBADDR(offset) (offset ? offset > blob_size ? NULL : &blob_base[offset] : NULL)

	if(flags & ui::Theme::load_meta)
	{
		blob_rel_addr = U32(hdr->name_offset);
		if((blob_addr = GETBLOBADDR(blob_rel_addr)))
			this->name = std::string((char *) blob_addr);
		else this->name = "Unknown";
		blob_rel_addr = U32(hdr->author_offset);
		if((blob_addr = GETBLOBADDR(blob_rel_addr)))
			this->author = std::string((char *) blob_addr);
		else this->author = "Unknown";
	}

	/* if we don't want to actually load any data but just metadata we're done now */
	if(!(flags & ui::Theme::load_data))
	{
		ret = true;
		goto out;
	}

	u32 ident, offset, isize;
	u32 *ptr;
	u16 w, h;
	for(u32 i = 0; i < num_descriptors; ++i)
	{
		ident = U32(descriptors[i].ident);
		switch(ident)
		{
                                                                           /* We want AGBR: do not byteswap */
#define CVAL(fid, iid) case fid: this->descriptors[ui::theme::iid].color = (descriptors[i].data.color.value); break
		CVAL(ID_BG_CLR, background_color);
		CVAL(ID_TEXT_CLR, text_color);
		CVAL(ID_BTN_BG_CLR, button_background_color);
		CVAL(ID_BTN_BORDER_CLR, button_border_color);
		CVAL(ID_BATTERY_GREEN_CLR, battery_green_color);
		CVAL(ID_BATTERY_RED_CLR, battery_red_color);
		CVAL(ID_TOGGLE_GREEN_CLR, toggle_green_color);
		CVAL(ID_TOGGLE_RED_CLR, toggle_red_color);
		CVAL(ID_TOGGLE_SLID_CLR, toggle_slider_color);
		CVAL(ID_PROGBAR_FG_CLR, progress_bar_foreground_color);
		CVAL(ID_PROGBAR_BG_CLR, progress_bar_background_color);
		CVAL(ID_SCROLLBAR_CLR, scrollbar_color);
		CVAL(ID_LED_GREEN_CLR, led_green_color);
		CVAL(ID_LED_RED_CLR, led_red_color);
		CVAL(ID_SMDH_BORDER_CLR, smdh_icon_border_color);
#undef CVAL
#define IVAL(fid, iid) case fid: \
	offset = U32(descriptors[i].data.image.img_ptr); \
	w = U16(descriptors[i].data.image.w); h = U16(descriptors[i].data.image.h); \
	isize = w * h * 4; \
	ptr = (u32 *) GETBLOBADDR(offset); \
	if(offset + isize > blob_size || !offset) { \
		elog("theme parser: invalid blob offset (got: %lu-%lu, max is %lu)", offset, offset + isize, blob_size); \
		continue; \
	} \
	rgba_to_abgr(ptr, w, h); \
	isReplacing = this->descriptors[ui::theme::iid].image.actual_image.tex != NULL && this->descriptors[ui::theme::iid].image.isOwn; \
	if(isReplacing) delete_image_data(this->descriptors[ui::theme::iid].image.actual_image); \
	load_rgba8(&this->descriptors[ui::theme::iid].image.actual_image, ptr, w, h, !isReplacing); \
	this->descriptors[ui::theme::iid].image.isOwn = true; \
	break
		IVAL(ID_MORE_IMG, more_image);
		IVAL(ID_BATTERY_IMG, battery_image);
		IVAL(ID_SEARCH_IMG, search_image);
		IVAL(ID_SETTINGS_IMG, settings_image);
		IVAL(ID_SPINNER_IMG, spinner_image);
		IVAL(ID_RANDOM_IMG, random_image);
#undef IVAL
		default:
			elog("theme parser: unknown identifier %lu", ident);
			break;
		}
	}

	ret = true;
out:
	free((void *) foot);
	fclose(f);
	return ret;
#undef GETBLOBADDR
#undef EXIT_LOG
}

#define fill_colors(s) fill_colors_((s).colors, (s).getters, (s).len)
static void fill_colors_(u32 *colors, const ui::slot_color_getter *getters, size_t len)
{
	for(size_t i = 0; i < len; ++i)
		colors[i] = getters[i]();
}

ui::ThemeManager::~ThemeManager()
{
	for(auto it : this->slots)
		if(it.second.len)
			free(it.second.colors);
}

ui::ThemeManager *ui::ThemeManager::global()
{ return &manager; }

ui::SlotManager ui::ThemeManager::get_slots(ui::BaseWidget *that, const char *id, size_t count, const slot_color_getter *getters)
{
	auto it = this->slots.find(id);
	if(it != this->slots.end())
	{
		if(that && that->supports_theme_hook())
			it->second.slaves.push_back(that); 
		return ui::SlotManager(it->second.colors);
	}

	slot_data slot;
	if(that && that->supports_theme_hook())
		slot.slaves = { that };
	slot.len = count;
	if(count)
	{
		slot.colors = (u32 *) malloc(sizeof(u32) * count);
		slot.getters = getters;
		fill_colors(slot);
	}

	this->slots[id] = slot;
	return ui::SlotManager(slot.colors);
}

void ui::ThemeManager::reget(const char *id)
{
	auto it = this->slots.find(id);
	if(it == this->slots.end())
		panic(std::string(id) + ": not found");
	if(it->second.len)
		fill_colors(it->second);
	for(ui::BaseWidget *w : it->second.slaves)
		w->update_theme_hook(); 
}

void ui::ThemeManager::reget()
{
	for(auto& it : this->slots)
	{
		if(it.second.len)

			fill_colors(it.second);
		for(size_t i = 0; i < it.second.slaves.size(); ++i)
			it.second.slaves[i]->update_theme_hook(); 
	}
}

void ui::ThemeManager::unregister(ui::BaseWidget *w)
{
	for(auto& it : this->slots)
		for(size_t i = 0; i < it.second.slaves.size(); ++i)
			if(it.second.slaves[i] == w)
			{
				it.second.slaves.erase(it.second.slaves.begin() + i);
				return;
			}
}


