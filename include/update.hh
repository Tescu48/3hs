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

#ifndef inc_update_hh
#define inc_update_hh

#define VERSION_MAJOR 1
#define VERSION_MINOR 3
#define VERSION_PATCH 2
#define VERSION_DESC "libere dorme"

#define INT_TO_STR(i) INT_TO_STR_(i)
#define INT_TO_STR_(i) #i

#define MK_UA(MA,MI,PA) "hShop (3DS/CTR/KTR; ARMv6) 3hs/" MA "." MI "." PA
#define VERSION INT_TO_STR(VERSION_MAJOR) "." INT_TO_STR(VERSION_MINOR) "." INT_TO_STR(VERSION_PATCH)
#define VVERSION "v" VERSION
#define USER_AGENT MK_UA(INT_TO_STR(VERSION_MAJOR), INT_TO_STR(VERSION_MINOR), INT_TO_STR(VERSION_PATCH))


bool update_app();

#endif
