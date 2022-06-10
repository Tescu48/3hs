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

#ifndef inc_log_hh
#define inc_log_hh

#include <stddef.h>


enum class LogLevel
{
	fatal    = 0,
	error    = 1,
	warning  = 2,
	info     = 3,
	debug    = 4,
	verbose  = 5,
};

__attribute__((format(printf, 5, 6)))
void _logf(const char *fnname, const char *filen,
	size_t line, LogLevel lvl, const char *fmt, ...);

const char *log_filename();
void log_delete_invalid();
void log_flush();
void log_init();
void log_exit();
void log_del();

#ifdef RELEASE
	/* inlined check so compiler can optimize it out entirely */
	#define l__log(l, ...) if(l <= LogLevel::info) \
		_logf(__func__, NULL, __LINE__, l, __VA_ARGS__)
#else
	#define l__log(l, ...) _logf(__func__, __FILE__, __LINE__, l, __VA_ARGS__)
#endif

#define elog(...) l__log(LogLevel::error, __VA_ARGS__)
#define flog(...) l__log(LogLevel::fatal, __VA_ARGS__)
#define wlog(...) l__log(LogLevel::warning, __VA_ARGS__)
#define ilog(...) l__log(LogLevel::info, __VA_ARGS__)
#define dlog(...) l__log(LogLevel::debug, __VA_ARGS__)
#define vlog(...) l__log(LogLevel::verbose, __VA_ARGS__)

#endif

