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

#include "panic.hh"

#include <stdlib.h>

#include <ui/base.hh>

#include "install.hh"
#include "hsapi.hh"
#include "i18n.hh"
#include "util.hh"
#include "log.hh"

Result init_services(bool& isLuma)
{
	Result res;

	Handle lumaCheck;
	isLuma = R_SUCCEEDED(svcConnectToPort(&lumaCheck, "hb:ldr"));
	if(isLuma) svcCloseHandle(lumaCheck);

	// Doesn't work in citra
	if(isLuma) if(R_FAILED(res = mcuHwcInit())) return res;
	/* 1MiB */ if(R_FAILED(res = httpcInit(1024 * 1024))) return res;
	if(R_FAILED(res = romfsInit())) return res;
	if(R_FAILED(res = cfguInit())) return res;
	if(R_FAILED(res = aptInit())) return res;
	if(R_FAILED(res = nsInit())) return res;
	if(R_FAILED(res = fsInit())) return res;
	if(R_FAILED(res = amInit())) return res;
	if(R_FAILED(res = psInit())) return res;
	if(R_FAILED(res = acInit())) return res;

	/* basically ensures that we can use the network during sleep
	 * thanks Kartik for the help */
	aptSetSleepAllowed(false);
	if(R_FAILED(res = ndmuInit())) return res;
	if(R_FAILED(res = NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE))) return res;
	if(R_FAILED(res = NDMU_LockState())) return res;

	return res;
}

void exit_services()
{
	NDMU_UnlockState();
	NDMU_LeaveExclusiveState();
	ndmuExit();
	aptSetSleepAllowed(true);

	mcuHwcExit();
	httpcExit();
	romfsExit();
	cfguExit();
	aptExit();
	fsExit();
	nsExit();
	amExit();
	psExit();
	acExit();
}


static void pusha(ui::RenderQueue& queue)
{
	ui::builder<ui::Text>(ui::Screen::top, STRING(press_a_exit))
		.x(ui::layout::center_x)
		.y(ui::layout::bottom)
		.wrap()
		.add_to(queue);
}

static void pusherror(const error_container& err, ui::RenderQueue& queue, float base)
{
	ui::Text *first;
	ui::BaseWidget *back = queue.back();
	ui::builder<ui::Text>(ui::Screen::top, format_err(err.sDesc, err.iDesc))
		.x(ui::layout::center_x)
		.y(base)
		.wrap()
		.add_to(&first, queue);
	if(base == 0.0f) first->set_y(ui::under(back, first));
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(result_code, "0x" + pad8code(err.full)))
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(level, format_err(err.sLvl, err.iLvl)))
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(summary, format_err(err.sSum, err.iSum)))
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(module, format_err(err.sMod, err.iMod)))
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
}

void handle_error(const error_container& err, const std::string *label)
{
	ui::RenderQueue queue;
	pusha(queue);
	float base = 50.0f;
	if(label)
	{
		ui::builder<ui::Text>(ui::Screen::top, *label)
			.x(ui::layout::center_x)
			.y(30.0f)
			.wrap()
			.add_to(queue);
		base = 0.0f;
	}
	pusherror(err, queue, base);
	queue.render_finite_button(KEY_A);
}


[[noreturn]] static void panic_core(const std::string& caller, ui::RenderQueue& queue)
{
	elog("PANIC -- THERE IS AN ERROR IN THE APPLCIATION");
	elog("Caller is %s", caller.c_str());
	aptSetHomeAllowed(true); /* these might be set otherwise in other parts of the code */
	C3D_FrameRate(60.0f);

	ui::Text *action = ui::RenderQueue::global()->find_tag<ui::Text>(ui::tag::action);
	/* panic may be called before core ui is set-up so we can't be sure
	 * ui::tag::action is already active */
	if(action == nullptr)
	{
		ui::builder<ui::Text>(ui::Screen::top, STRING(fatal_panic))
			.x(ui::layout::center_x)
			.y(4.0f)
			.wrap()
			.add_to(&action, ui::RenderQueue::global());
	}

	else
	{
		/* this will be the final focus shift so we don't need to revert the state after */
		set_desc(STRING(fatal_panic));
		set_focus(false);
	}

	pusha(queue);
	ui::builder<ui::Text>(ui::Screen::top, caller)
		.x(ui::layout::center_x)
		.under(action)
		.wrap()
		.add_to(queue);

	queue.render_finite_button(KEY_A);

	exit(0);
}

[[noreturn]] void panic_impl(const std::string& caller, const std::string& msg)
{
	ui::RenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, msg)
		.x(ui::layout::center_x)
		.y(70.0f)
		.wrap()
		.add_to(queue);

	elog("PANIC MESSAGE -- %s", msg.c_str());
	panic_core(caller, queue);
}

[[noreturn]] void panic_impl(const std::string& caller, Result res)
{
	ui::RenderQueue queue;

	error_container err = get_error(res);
	pusherror(err, queue, 70.0f);

	elog("PANIC RESULT -- 0x%016lX", res);
	panic_core(caller, queue);
}

[[noreturn]] void panic_impl(const std::string& caller)
{
	ui::RenderQueue queue;
	panic_core(caller, queue);
}

