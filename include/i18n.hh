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

#ifndef inc_i18n_hh
#define inc_i18n_hh

#include <stdint.h>

#include <type_traits>
#include <vector>
#include <string>

#include "i18n_tab.hh"

// ParamSTRING
#define PSTRING(x, ...) i18n::interpolate(str::x, __VA_ARGS__)
#define SURESTRING(x) i18n::getsurestr(str::x)
#define STRING(x) i18n::getstr(str::x)


namespace i18n
{
	/* you don't have to cache the return of this function */
	const char *getstr(str::type sid, str::type lid);
	const char *getstr(str::type id);

	/* ensure the config exists; more expensive than getstr() */
	const char *getsurestr(str::type id);

	lang::type default_lang();

	// from here on out it's template shit

	namespace detail
	{
		template <typename T,
				// Does T have a std::to_string() overload?
				typename = decltype(std::to_string(std::declval<T>()))
			>
		std::string stringify(const T& arg)
		{ return std::to_string(arg); }

		// fallback for `const char *` (technically all arrays) and `std::string`
		template <typename T,
			typename std::enable_if<std::is_array<T>::value
				|| std::is_same<T, std::string>::value, bool>::type * = nullptr
			>
		std::string stringify(const T& arg)
		{ return std::string(arg); }

		template <typename T>
		std::string adv_getstr(str::type id, std::vector<std::string>& substs, const T& head)
		{
			substs.push_back(stringify(head));
			const char *rawstr = getstr(id);
			std::string ret;

			do
			{
				if(*rawstr == '%')
				{
					++rawstr;
					if(*rawstr == '%') // literal "%"
						ret.push_back('%');
					// lets just assume our input/output is always correct...
					else ret += substs[(*rawstr) - '0' - 1];
				}

				else ret.push_back(*rawstr);
			} while(*rawstr++);

			return ret;
		}

		template <typename THead, typename ... TTail>
		std::string adv_getstr(str::type id, std::vector<std::string>& substs,
			const THead& head, const TTail& ... tail)
		{
			substs.push_back(stringify(head));
			return adv_getstr(id, substs, tail...);
		}
	}

	/* you should cache the return of this function */
	template <typename T>
	std::string interpolate(str::type id, const T& head)
	{
		std::vector<std::string> substs;
		return detail::adv_getstr<T>(id, substs, head);
	}

	/* you should cache the return of this function */
	template <typename THead, typename ... TTail>
	std::string interpolate(str::type id, const THead& head, const TTail& ... tail)
	{
		std::vector<std::string> substs = { detail::stringify(head) };
		return detail::adv_getstr(id, substs, tail...);
	}
}

#endif

