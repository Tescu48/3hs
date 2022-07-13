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

#include "next.hh"

#include <widgets/meta.hh>
#include <ui/list.hh>
#include <ui/base.hh>

#include <ctype.h>

#include "hsapi.hh"
#include "queue.hh"
#include "panic.hh"
#include "i18n.hh"
#include "util.hh"


const std::string *next::sel_cat(size_t *cursor)
{
	panic_assert(hsapi::get_index()->categories.size() > *cursor, "invalid cursor position");
	using list_t = ui::List<hsapi::Category>;

	std::string desc = set_desc(STRING(select_cat));
	bool focus = set_focus(false);
	const std::string *ret = nullptr;

	ui::RenderQueue queue;

	ui::CatMeta *meta;
	list_t *list;

	ui::builder<ui::CatMeta>(ui::Screen::bottom, hsapi::get_index()->categories[*cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &hsapi::get_index()->categories)
		.connect(list_t::to_string, [](const hsapi::Category& cat) -> std::string { return cat.disp; })
		.connect(list_t::select, [&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = &self->at(i).name;
			if(kDown & KEY_START)
				ret = next_cat_exit;
			return false;
		})
		.connect(list_t::change, [meta](list_t *self, size_t i) -> void {
			meta->set_cat(self->at(i));
		})
		.connect(list_t::buttons, KEY_START)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	if(cursor != nullptr) list->set_pos(*cursor);
	queue.render_finite();
	if(cursor != nullptr) *cursor = list->get_pos();

	set_focus(focus);
	set_desc(desc);
	return ret;
}

const std::string *next::sel_sub(const std::string& cat, size_t *cursor)
{
	using list_t = ui::List<hsapi::Subcategory>;

	std::string desc = set_desc(STRING(select_subcat));
	bool focus = set_focus(false);
	const std::string *ret = nullptr;

	hsapi::Category *rcat = hsapi::get_index()->find(cat);
	ui::RenderQueue queue;

	panic_assert(rcat, "couldn't find category");

	ui::SubMeta *meta;
	list_t *list;

	panic_assert(rcat->subcategories.size() > *cursor, "invalid cursor position");

	ui::builder<ui::SubMeta>(ui::Screen::bottom, rcat->subcategories[*cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &rcat->subcategories)
		.connect(list_t::to_string, [](const hsapi::Subcategory& scat) -> std::string { return scat.disp; })
		.connect(list_t::select, [&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = &self->at(i).name;
			if(kDown & KEY_B) ret = next_sub_back;
			if(kDown & KEY_START) ret = next_sub_exit;
			return false;
		})
		.connect(list_t::change, [meta](list_t *self, size_t i) -> void {
			meta->set_sub(self->at(i));
		})
		.connect(list_t::buttons, KEY_B | KEY_START)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	if(cursor != nullptr) list->set_pos(*cursor);
	queue.render_finite();
	if(cursor != nullptr) *cursor = list->get_pos();

	set_focus(focus);
	set_desc(desc);
	return ret;
}

template <typename T>
using sort_callback = bool (*) (T& a, T& b);

static bool string_case_cmp(const std::string& a, const std::string& b, bool lt)
{
	for(size_t i = 0; i < a.size() && i < b.size(); ++i)
	{
		char cha = tolower(a[i]), chb = tolower(b[i]);
		if(cha == chb) continue;
		return lt ? cha < chb : cha > chb;
	}
	return lt ? a.size() < b.size() : a.size() > b.size();
}

static bool sort_alpha_desc(hsapi::Title& a, hsapi::Title& b) { return string_case_cmp(a.name, b.name, false); }
static bool sort_tid_desc(hsapi::Title& a, hsapi::Title& b) { return a.tid > b.tid; }
static bool sort_size_desc(hsapi::Title& a, hsapi::Title& b) { return a.size > b.size; }
static bool sort_downloads_desc(hsapi::Title& a, hsapi::Title& b) { return a.dlCount > b.dlCount; }
static bool sort_id_desc(hsapi::Title& a, hsapi::Title& b) { return a.id > b.id; }

static bool sort_alpha_asc(hsapi::Title& a, hsapi::Title& b) { return string_case_cmp(a.name, b.name, true); }
static bool sort_tid_asc(hsapi::Title& a, hsapi::Title& b) { return a.tid < b.tid; }
static bool sort_size_asc(hsapi::Title& a, hsapi::Title& b) { return a.size < b.size; }
static bool sort_downloads_asc(hsapi::Title& a, hsapi::Title& b) { return a.dlCount < b.dlCount; }
static bool sort_id_asc(hsapi::Title& a, hsapi::Title& b) { return a.id < b.id; }

static sort_callback<hsapi::Title> get_sort_callback(SortDirection dir, SortMethod method)
{
	switch(dir)
	{
	case SortDirection::ascending:
		switch(method)
		{
		case SortMethod::alpha: return sort_alpha_asc;
		case SortMethod::tid: return sort_tid_asc;
		case SortMethod::size: return sort_size_asc;
		case SortMethod::downloads: return sort_downloads_asc;
		case SortMethod::id: return sort_id_asc;
		}
		break;
	case SortDirection::descending:
		switch(method)
		{
		case SortMethod::alpha: return sort_alpha_desc;
		case SortMethod::tid: return sort_tid_desc;
		case SortMethod::size: return sort_size_desc;
		case SortMethod::downloads: return sort_downloads_desc;
		case SortMethod::id: return sort_id_desc;
		}
		break;
	}
	/* how does this happen */
	fix_sort_settings();
	return sort_alpha_asc;
//	panic("invalid sort method/direction");
}

hsapi::hid next::sel_gam(std::vector<hsapi::Title>& titles, size_t *cursor)
{
	panic_assert(titles.size() > *cursor, "invalid cursor position");
	using list_t = ui::List<hsapi::Title>;

	std::string desc = set_desc(STRING(select_title));
	bool focus = set_focus(false);
	hsapi::hid ret = next_gam_back;

	SortDirection dir = SETTING_DEFAULT_SORTDIRECTION;
	SortMethod sortm = SETTING_DEFAULT_SORTMETHOD;
	std::sort(titles.begin(), titles.end(), get_sort_callback(dir, sortm));

	ui::RenderQueue queue;

	ui::TitleMeta *meta;
	list_t *list;

	ui::builder<ui::TitleMeta>(ui::Screen::bottom, titles[*cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &titles)
		.connect(list_t::to_string, [](const hsapi::Title& title) -> std::string { return title.name; })
		.connect(list_t::select, [&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = self->at(i).id;
			if(kDown & KEY_B) ret = next_gam_back;
			if(kDown & KEY_START) ret = next_gam_exit;
			if(kDown & KEY_Y)
			{
				ui::RenderQueue::global()->render_and_then([ret]() -> void {
					/* the true is there to stop ambiguidity (how?) */
					queue_add(ret, true);
				});
				return true;
			}
			return false;
		})
		.connect(list_t::change, [meta](list_t *self, size_t i) -> void {
			meta->set_title(self->at(i));
		})
		.connect(list_t::buttons, KEY_B | KEY_Y | KEY_START)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_L)
		.connect(ui::ButtonCallback::kdown, [list, &dir, &sortm, &titles, meta](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([list, &dir, &sortm, &titles, meta]() -> void {
				sortm = settings_sort_switch();
#if 0
				hsapi::hid curId = titles[list->get_pos()].id;
#endif
				std::sort(titles.begin(), titles.end(), get_sort_callback(dir, sortm));
				list->update();
#if 0
				auto it = std::find(titles.begin(), titles.end(), curId);
				panic_assert(it != titles.end(), "failed to find previously selected title");
				list->set_pos(it - titles.begin());
				meta->set_title(*it);
#endif
				list->set_pos(0);
				meta->set_title(titles[0]);
			});
			return true;
		}).add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_R)
		.connect(ui::ButtonCallback::kdown, [list, &dir, &sortm, &titles, meta](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([list, &dir, &sortm, &titles, meta]() -> void {
				dir = dir == SortDirection::ascending ? SortDirection::descending : SortDirection::ascending;
#if 0
				hsapi::hid curId = titles[list->get_pos()].id;
#endif
				std::sort(titles.begin(), titles.end(), get_sort_callback(dir, sortm));
				list->update();
#if 0
				auto it = std::find(titles.begin(), titles.end(), curId);
				panic_assert(it != titles.end(), "failed to find previously selected title");
				list->set_pos(it - titles.begin());
				meta->set_title(*it);
#endif
				list->set_pos(0);
				meta->set_title(titles[0]);
			});
			return true;
		}).add_to(queue);

	if(cursor != nullptr) list->set_pos(*cursor);
	queue.render_finite();
	if(cursor != nullptr) *cursor = list->get_pos();

	set_focus(focus);
	set_desc(desc);
	return ret;
}

void next::maybe_install_gam(std::vector<hsapi::Title>& titles)
{
	size_t cur = 0;
	do {
		hsapi::hid id = next::sel_gam(titles, &cur);
		if(id == next_gam_exit || id == next_gam_back)
			break;

		hsapi::FullTitle meta;
		if(show_extmeta_lazy(titles, id, &meta))
			install::gui::hs_cia(meta);
	} while(true);
}

