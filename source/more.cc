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

#include "more.hh"

#include "hlink/hlink_view.hh"

#include "find_missing.hh"
#include "log_view.hh"
#include "settings.hh"
#include "hsapi.hh"
#include "about.hh"
#include "panic.hh"
#include "i18n.hh"
#include "util.hh"

#include <ui/menuselect.hh>
#include <ui/base.hh>

#include <stdlib.h>


enum MoreInds {
	IND_ABOUT = 0,
	IND_FIND_MISSING,
	IND_LOG,
#ifndef RELEASE
	IND_HLINK,
#endif
	IND_MAX
};


void show_more()
{
	bool focus = set_focus(true);

	ui::RenderQueue queue;
	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.connect(ui::MenuSelect::add, STRING(about_app), []() -> bool { show_about(); return true; })
		.connect(ui::MenuSelect::add, STRING(find_missing_content), []() -> bool { show_find_missing_all(); return true; })
		.connect(ui::MenuSelect::add, STRING(log), []() -> bool { show_logs_menu(); return true; })
		.connect(ui::MenuSelect::add, STRING(themes), []() -> bool { show_theme_menu(); return true; })
#ifndef RELEASE
		.connect(ui::MenuSelect::add, "hLink", []() -> bool { show_hlink(); return true; })
#endif
		.add_to(queue);
	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_START)
		.connect(ui::ButtonCallback::kdown, [](u32) -> bool { exit(0); return false; })
		.add_to(queue);

	queue.render_finite_button(KEY_B);
	set_focus(focus);
}

