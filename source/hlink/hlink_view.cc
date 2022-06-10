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

#include "hlink/hlink_view.hh"
#include "hlink/hlink.hh"

#include "panic.hh"
#include "util.hh"
#include "i18n.hh"

#include <ui/base.hh>


static void addreq(ui::RenderQueue& queue, const std::string& reqstr)
{
	ui::builder<ui::Text>(ui::Screen::bottom, "Last request\n" + reqstr)
		.x(ui::layout::center_x)
		.y(ui::layout::base)
		.wrap()
		.add_to(queue);
}

void show_hlink()
{
	bool focus = set_focus(true);

	std::string reqstr = STRING(no_req);

	hlink::create_server(
		[&reqstr](const std::string& from) -> bool {
			ui::RenderQueue queue;
			addreq(queue, reqstr);
			bool ret = false;

			ui::builder<ui::Text>(ui::Screen::top,
					"Do you want to accept a connection\n"
					"from " + from + "?\n"
					"Press " UI_GLYPH_A " to accept and " UI_GLYPH_B " to decline")
				.x(ui::layout::center_x)
				.y(ui::layout::base)
				.add_to(queue);

			ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_A)
				.connect(ui::ButtonCallback::kdown, [&ret](u32) -> bool { ret = true; return false; })
				.add_to(queue);

			ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_B)
				.connect(ui::ButtonCallback::kdown, [](u32) -> bool { return false; })
				.add_to(queue);

			queue.render_finite();
			return ret;
		},
		[&reqstr](const std::string& err) -> void {
			ui::RenderQueue queue;
			addreq(queue, reqstr);

			ui::builder<ui::Text>(ui::Screen::top, "Press " UI_GLYPH_A " to continue")
				.x(ui::layout::center_x)
				.y(ui::dimensions::height - 30.0f)
				.add_to(queue);
			ui::builder<ui::Text>(ui::Screen::top, "An error occured in the hLink server\n" + err)
				.x(ui::layout::center_x)
				.y(ui::layout::base)
				.add_to(queue);

			queue.render_finite_button(KEY_A);
		},
		[&reqstr](const std::string& ip) -> void {
			ui::RenderQueue queue;
			addreq(queue, reqstr);

			ui::builder<ui::Text>(ui::Screen::top,
					"Created the hLink server\n"
					"Connect to " + ip + ":8000 for http;\n"
					"Connect to " + ip + ":" + std::to_string(hlink::port) + " for hLink")
				.x(ui::layout::center_x)
				.y(ui::layout::base)
				.add_to(queue);

			queue.render_frame();
		},
		[]() -> bool {
			if(!aptMainLoop()) return false;
			ui::Keys keys = ui::RenderQueue::get_keys();
			return !((keys.kDown | keys.kHeld) & (KEY_START | KEY_B));
		},
		[&reqstr](const std::string& str) -> void {
			ui::RenderQueue queue;
			reqstr = str;

			addreq(queue, reqstr);
			queue.render_frame();
		}
	);

	set_focus(focus);
}

