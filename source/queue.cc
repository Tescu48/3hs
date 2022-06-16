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

#include "queue.hh"

#include <widgets/indicators.hh>

#include <ui/confirm.hh>
#include <ui/list.hh>

#include <widgets/meta.hh>

#include <vector>

#include "lumalocale.hh"
#include "installgui.hh"
#include "install.hh"
#include "panic.hh"
#include "error.hh"
#include "i18n.hh"
#include "util.hh"
#include "ctr.hh"
#include "log.hh"

static std::vector<hsapi::FullTitle> g_queue;
std::vector<hsapi::FullTitle>& queue_get() { return g_queue; }

void queue_add(const hsapi::FullTitle& meta)
{
	g_queue.push_back(meta);
}

void queue_add(hsapi::hid id, bool disp)
{
	hsapi::FullTitle meta;
	Result res = disp ? hsapi::call(hsapi::title_meta, meta, std::move(id))
		: hsapi::scall(hsapi::title_meta, meta, std::move(id));
	if(R_FAILED(res)) return;
	queue_add(meta);
}

void queue_remove(size_t index)
{
	g_queue.erase(g_queue.begin() + index);
}

void queue_clear()
{
	g_queue.clear();
}

void queue_process(size_t index)
{
	if(R_SUCCEEDED(install::gui::hs_cia(g_queue[index])))
		queue_remove(index);
}

void queue_process_all()
{
	bool hasLock = R_SUCCEEDED(ctr::lockNDM());

	struct errvec {
		Result res;
		hsapi::FullTitle *meta;
	};
	std::vector<errvec> errs;
	enum PostProcFlag {
		NONE       = 0,
		WARN_THEME = 1,
		WARN_FILE  = 2,
		SET_PATCH  = 4,
	}; int procflag = NONE;
	for(hsapi::FullTitle& meta : g_queue)
	{
		ilog("Processing title with id=%llu", meta.id);
		Result res = install::gui::hs_cia(meta, false);
		ilog("Finished processing, res=%016lX", res);
		if(R_FAILED(res))
		{
			errvec ev;
			ev.res = res; ev.meta = &meta;
			errs.push_back(ev);
		}
		else
		{
			if(luma::set_locale(meta.tid))
				procflag |= SET_PATCH;
			if(meta.cat == THEMES_CATEGORY)
				procflag |= WARN_THEME;
			else if(meta.flags & hsapi::TitleFlag::installer)
				procflag |= WARN_FILE;
		}
	}

	if(procflag & SET_PATCH) luma::maybe_set_gamepatching();
	if(procflag & WARN_THEME) ui::notice(STRING(theme_installed));
	if(procflag & WARN_FILE) ui::notice(STRING(file_installed));

	if(errs.size() != 0)
	{
		ui::notice(STRING(replaying_errors));
		for(errvec& ev : errs)
		{
			error_container err = get_error(ev.res);
			handle_error(err, &ev.meta->name);
		}
	}

	if(hasLock) ctr::unlockNDM();
	/* TODO: Only clear successful installs */
	queue_clear();
}

static void queue_is_empty()
{
	ui::RenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, STRING(queue_empty))
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.wrap()
		.add_to(queue);

	queue.render_finite_button(KEY_A | KEY_B);
}

void show_queue()
{
	using list_t = ui::List<hsapi::FullTitle>;
	bool focus = set_focus(true);

	// Queue is empty :craig:
	if(g_queue.size() == 0)
	{
		queue_is_empty();
		set_focus(focus);
		return;
	}

	ui::RenderQueue queue;

	ui::TitleMeta *meta;

	ui::builder<ui::TitleMeta>(ui::Screen::bottom, g_queue[0])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &g_queue)
		.connect(list_t::to_string, [](const hsapi::FullTitle& meta) -> std::string { return meta.name; })
		.connect(list_t::select, [meta](list_t *self, size_t i, u32 kDown) -> bool {
			/* why is the cast necessairy? */
			((void) i);
			ui::RenderQueue::global()->render_and_then((std::function<bool()>) [self, meta, kDown]() -> bool {
				size_t i = self->get_pos(); /* for some reason the i param corrupted (?) */
				if(kDown & KEY_X)
					queue_remove(i);
				else if(kDown & KEY_A)
					queue_process(i);

				if(g_queue.size() == 0)
					return false; /* we're done */
				/* if we removed the last item */
				if(i >= g_queue.size())
					--i;

				meta->set_title(self->at(i));
				self->set_pos(i);
				self->update();

				return true;
			});

			return true;
		})
		.connect(list_t::change, [meta](list_t *self, size_t i) -> void {
			meta->set_title(self->at(i));
		})
		.connect(list_t::buttons, KEY_X)
		.x(5.0f).y(25.0f)
		.add_to(queue);

	ui::builder<ui::Button>(ui::Screen::bottom, STRING(install_all))
		.connect(ui::Button::click, []() -> bool {
			ui::RenderQueue::global()->render_and_then(queue_process_all);
			/* the queue will always be empty after this */
			return false;
		})
		.wrap()
		.x(ui::layout::right)
		.y(210.0f)
		.add_to(queue);

	queue.render_finite_button(KEY_B);
	set_focus(focus);
}

