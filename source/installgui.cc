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

#include "installgui.hh"

#include <ui/progress_bar.hh>
#include <ui/base.hh>

#include "widgets/indicators.hh"
#include "find_missing.hh"
#include "lumalocale.hh"
#include "settings.hh"
#include "panic.hh"
#include "util.hh"
#include "seed.hh"
#include "ctr.hh"
#include "log.hh"


UI_CTHEME_GETTER(color_led_green, ui::theme::led_green_color)
UI_CTHEME_GETTER(color_led_red, ui::theme::led_red_color)
static ui::slot_color_getter slotmgr_getters[] = {
	color_led_green, color_led_red
};

static ui::SlotManager slotmgr;


static void make_queue(ui::RenderQueue& queue, ui::ProgressBar **bar)
{
	ui::builder<ui::ProgressBar>(ui::progloc())
		.y(ui::layout::center_y)
		.add_to(bar, queue);
	queue.render_frame();
}

static bool ask_reinstall(bool interactive)
{
	return interactive ?
		ui::Confirm::exec(STRING(already_installed_reinstall)) : false;
}

static void finalize_install(u64 tid, bool interactive)
{
	ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();

	// Prompt to ask for extra content
	if(interactive && tid_can_have_missing(tid) && ISET_SEARCH_ECONTENT)
	{
		ssize_t added = show_find_missing(tid);
		if(added > 0) ui::notice(PSTRING(found_missing, added));
	}

	/* only set locale if we're interactive, otherwise the caller has to handle it himself */
	if(interactive && luma::set_locale(tid))
		luma::maybe_set_gamepatching();
}

/* returns if the user wants to continue */
static bool maybe_warn_already_installed(u64 tid, bool interactive)
{
	if(!interactive || !ISET_WARN_NO_BASE || ctr::is_base_tid(tid) || ctr::title_exists(ctr::get_base_tid(tid), ctr::to_mediatype(ctr::detect_dest(tid))))
		return true;

	/* we need to warn the user and ask if he wants to continue now */

	return ui::Confirm::exec(STRING(install_no_base));
}

Result install::gui::net_cia(const std::string& url, u64 tid, bool interactive, bool defaultReinstallable)
{
	if(!maybe_warn_already_installed(tid, interactive))
		return 0;

	bool focus = set_focus(true);
	ui::ProgressBar *bar;
	ui::RenderQueue queue;

	make_queue(queue, &bar);

	bool shouldReinstall = defaultReinstallable;
	Result res = 0;

start_install:
	res = install::net_cia(makeurlwrap(url), tid, [&queue, &bar](u64 now, u64 total) -> void {
		bar->update(now, total);
		bar->activate();
		queue.render_frame();
	}, shouldReinstall);

	if(res == APPERR_NOREINSTALL)
	{
		if((shouldReinstall = ask_reinstall(interactive)))
			goto start_install;
	}

	if(R_SUCCEEDED(res)) res = add_seed(tid);
	if(R_FAILED(res))
	{
		error_container err = get_error(res);
		report_error(err, "User was installing from " + url);
		if(interactive) handle_error(err);
	}
	else finalize_install(tid, interactive);

	set_focus(focus);
	return res;
}

Result install::gui::hs_cia(const hsapi::FullTitle& meta, bool interactive, bool defaultReinstallable)
{
	if(!maybe_warn_already_installed(meta.tid, interactive))
		return 0;

	bool focus = set_focus(true);
	ui::ProgressBar *bar;
	ui::RenderQueue queue;

	make_queue(queue, &bar);

	bool shouldReinstall = defaultReinstallable;
	Result res = 0;

	if(interactive && R_FAILED(res = ctr::lockNDM()))
		elog("failed to lock NDM: %08lX", res);

start_install:
	res = install::hs_cia(meta, [&queue, &bar](u64 now, u64 total) -> void {
		bar->update(now, total);
		bar->activate();
		queue.render_frame();
	}, shouldReinstall);

	if(res == APPERR_NOREINSTALL)
	{
		if((shouldReinstall = ask_reinstall(interactive)))
			goto start_install;
	}

	if(R_SUCCEEDED(res))
	{
		res = add_seed(meta.tid);
		finalize_install(meta.tid, interactive);
		if(interactive)
		{
			if(meta.cat == THEMES_CATEGORY)
				ui::notice(STRING(theme_installed));
			else if(meta.flags & hsapi::TitleFlag::installer)
				ui::notice(STRING(file_installed));
		}
		install::gui::SuccessLED();
	}
	else
	{
		install::gui::ErrorLED();
		error_container err = get_error(res);
		report_error(err, "User was installing (" + ctr::tid_to_str(meta.tid) + ") (" + std::to_string(meta.id) + ")");
		if(interactive) handle_error(err);
	}

	if(interactive) ctr::unlockNDM();
	else            ui::LED::SetTimeout(time(NULL) + 2);

	set_focus(focus);
	return res;
}

static void setled(size_t i)
{
	if(!slotmgr.is_initialized())
		slotmgr = ui::ThemeManager::global()->get_slots(nullptr, "__global_install_gui_colors", 2, slotmgr_getters);

	ilog("color: %08lX", slotmgr.get(i));

	ui::LED::Pattern pattern;
	ui::LED::Solid(&pattern, UI_LED_MAKE_ANIMATION(0, 0xFF, 0), slotmgr.get(i));
	ui::LED::SetSleepPattern(&pattern);

}

void install::gui::SuccessLED() { setled(0); }
void install::gui::ErrorLED()   { setled(1); }

