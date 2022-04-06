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

#pragma once

#include "FlowComponent.h"

struct ResFlow{
	ResFlow(void);
	~ResFlow(void);

	void graph(void);
	ResFlow* precede(ResFlow *successor);
	ResFlow* precede(FlowComponent *successor);
	FlowComponent *blocks_;
	FlowComponent *waveletHorizL_;
	FlowComponent *waveletHorizH_;
	FlowComponent *waveletVert_;
};

class ImageComponentFlow {
public:
	ImageComponentFlow(uint8_t numResolutions);
	virtual ~ImageComponentFlow();
	std::string genBlockFlowTaskName(uint8_t resFlowNo);
	ResFlow *getResFlow(uint8_t resFlowNo);
	void graph(void);

	uint8_t numResFlows_;
	ResFlow *resFlows_;
	FlowComponent *waveletFinalCopy_;
};