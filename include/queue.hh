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

#ifndef inc_queue_hh
#define inc_queue_hh

#include <vector>
#include <3ds.h>

#include "hsapi.hh"

std::vector<hsapi::FullTitle>& queue_get();

void queue_add(hsapi::hid id, bool disp = true);
void queue_add(const hsapi::FullTitle& meta);
void queue_process(size_t index);
void queue_remove(size_t index);
void queue_add(hsapi::hid id);
void queue_process_all();
void queue_clear();
void show_queue();

#endif

