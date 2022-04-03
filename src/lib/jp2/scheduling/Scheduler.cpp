/*
 *    Copyright (C) 2016-2022 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "grk_includes.h"

namespace grk
{

Scheduler::Scheduler(uint8_t numResolutions) : state_(nullptr){
	if (numResolutions){
		state_ = new ScheduleState(numResolutions);
	}
}

Scheduler::~Scheduler()
{
	delete state_;
	for(auto& t : t1Implementations)
		delete t;
}

ScheduleState* Scheduler::getState(void){
	return state_;
}

} // namespace grk
