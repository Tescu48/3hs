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

#ifndef inc_panic_hh
#define inc_panic_hh

#include <string>

#include <string.h>
#include <3ds.h>

#include "error.hh"

#ifdef RELEASE
	#define panic(...) panic_impl(std::string(__func__) + "@" + std::to_string(__LINE__) __VA_OPT__(,) __VA_ARGS__)
#else
	#define panic(...) panic_impl(std::string(PANIC_FILENAME) + ":" + std::string(__func__) + "@" + std::to_string(__LINE__) __VA_OPT__(,) __VA_ARGS__)
	#define PANIC_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define panic_if_err_3ds(result) do { Result res = (result); if(R_FAILED(res)) panic(res); } while(0)

#ifndef NDEBUG
# define panic_assert(cond, msg) if(!(cond)) panic("Assertion failed\n" #cond "\n" msg)
#else
# define panic_assert(...) (void)
#endif

void handle_error(const error_container& err, const std::string *label = nullptr);

[[noreturn]] void panic_impl(const std::string& caller, const std::string& msg);
[[noreturn]] void panic_impl(const std::string& caller, Result res);
[[noreturn]] void panic_impl(const std::string& caller);

Result init_services(bool& isLuma);
void exit_services();

#endif

