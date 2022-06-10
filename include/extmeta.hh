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

#ifndef inc_extmeta_hh
#define inc_extmeta_hh

#include <vector>

#include "hsapi.hh"


/* returns if the user wants to continue installing this title or not.
 * lazy loads full title (for prodcode and version) */
bool show_extmeta_lazy(std::vector<hsapi::Title>& titles, hsapi::hid id,
	hsapi::FullTitle *full = nullptr);
bool show_extmeta_lazy(const hsapi::Title& base, hsapi::FullTitle *full = nullptr);
/* returns if the user wants to continue installing this title or not */
bool show_extmeta(const hsapi::FullTitle& title);

#endif

