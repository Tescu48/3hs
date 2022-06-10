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

#include <algorithm>

#include <widgets/meta.hh>
#include <ui/base.hh>

#include "extmeta.hh"
#include "thread.hh"
#include "queue.hh"
#include "panic.hh"
#include "i18n.hh"
#include "util.hh"
#include "ctr.hh"
#include "log.hh"


enum class extmeta_return { yes, no, none };

/* don't call with exmeta_return::none */
static bool to_bool(extmeta_return r)
{
	return r == extmeta_return::yes;
}

static extmeta_return extmeta(ui::RenderQueue& queue, const hsapi::Title& base, const std::string& version_s, const std::string& prodcode_s)
{
	extmeta_return ret = extmeta_return::none;
	ui::Text *prodcode;
	ui::Text *version;

	ui::builder<ui::Text>(ui::Screen::top, STRING(press_to_install))
		.x(ui::layout::center_x)
		.y(170.0f)
		.wrap()
		.add_to(queue);

	/***
	 * name (wrapped)
	 * category -> subcategory
	 *
	 * "Press a to install, b to not"
	 * =======================
	 * version     prod
	 * tid
	 * size
	 * landing id
	 ***/

	/* name */
	ui::builder<ui::Text>(ui::Screen::top, base.name)
		.x(9.0f)
		.y(25.0f)
		// if this overflows to the point where it overlaps with
		// STRING(press_to_install) we're pretty fucked, but i
		// don't think we have such titles
		.wrap()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, STRING(name))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* category -> subcategory */
	// on the bottom screen there are cases where this overflows,
	// but i don't think that can happen on the top screen since it's a bit bigger
	ui::builder<ui::Text>(ui::Screen::top, hsapi::get_index()->find(base.cat)->disp + " -> " + hsapi::get_index()->find(base.cat)->find(base.subcat)->disp)
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, STRING(category))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* version */
	ui::builder<ui::Text>(ui::Screen::bottom, version_s)
		.x(9.0f)
		.y(20.0f)
		.add_to(&version, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(version))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* product code */
	ui::builder<ui::Text>(ui::Screen::bottom, prodcode_s)
		.x(150.0f)
		.align_y(version)
		.add_to(&prodcode, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(prodcode))
		.size(0.45f)
		.x(150.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* size */
	ui::builder<ui::Text>(ui::Screen::bottom, ui::human_readable_size_block<hsapi::hsize>(base.size))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(size))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* title id */
	ui::builder<ui::Text>(ui::Screen::bottom, ctr::tid_to_str(base.tid))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(tid))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* landing id */
	ui::builder<ui::Text>(ui::Screen::bottom, std::to_string(base.id))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(landing_id))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_B)
		.connect(ui::ButtonCallback::kdown, [&ret](u32) -> bool { ret = extmeta_return::no; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_A)
		.connect(ui::ButtonCallback::kdown, [&ret](u32) -> bool { ret = extmeta_return::yes; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_Y)
		.connect(ui::ButtonCallback::kdown, [&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
				queue_add(base.id, true);
			}); return true; })
		.add_to(queue);

	queue.render_finite();
	return ret;
}

static extmeta_return extmeta(const hsapi::FullTitle& title)
{
	ui::RenderQueue queue;
	return extmeta(queue, title, hsapi::parse_vstring(title.version) + " (" + std::to_string(title.version) + ")", title.prod);
}

bool show_extmeta_lazy(const hsapi::Title& base, hsapi::FullTitle *full)
{
	std::string desc = set_desc(STRING(more_about_content));
	ui::RenderQueue queue;
	bool ret = true;

	std::string version, prodcode;

	ctr::thread<std::string&, std::string&, ui::RenderQueue&, hsapi::FullTitle *> th([&base]
			(std::string& version, std::string& prodcode, ui::RenderQueue& queue, hsapi::FullTitle *fullptr) -> void {
		hsapi::FullTitle full;
		if(R_FAILED(hsapi::title_meta(full, base.id)))
			return;
		if(fullptr != nullptr)
			*fullptr = full;
		version = hsapi::parse_vstring(full.version) + " (" + std::to_string(full.version) + ")";
		prodcode = full.prod;
		queue.signal(ui::RenderQueue::signal_cancel);
	}, version, prodcode, queue, full);

	extmeta_return res = extmeta(queue, base, STRING(loading), STRING(loading));
	/* second thread returned more data */
	if(res == extmeta_return::none)
	{
		dlog("Lazy load finished before choice was made.");
		queue.clear();
		res = extmeta(queue, base, version, prodcode);
	}
	ret = to_bool(res);

	/* At this point we're done rendering and
	 * waiting for the *fetching* of the full data
	 * and *setting* of the renderqueue callback */
	th.join();

	set_desc(desc);
	return ret;
}

bool show_extmeta_lazy(std::vector<hsapi::Title>& titles, hsapi::hid id, hsapi::FullTitle *full)
{
	std::vector<hsapi::Title>::iterator it =
		std::find_if(titles.begin(), titles.end(), [id](const hsapi::Title& t) -> bool {
			return t.id == id;
		});

	panic_assert(it != titles.end(), "Could not find id in vector");
	return show_extmeta_lazy(*it, full);
}

bool show_extmeta(const hsapi::FullTitle& title)
{
	std::string desc = set_desc(STRING(more_about_content));
	bool ret = to_bool(extmeta(title));
	set_desc(desc);
	return ret;
}

