/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
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
 *
 *    This source code incorporates work covered by the BSD 2-clause license.
 *    Please see the LICENSE file in the root directory for details.
 *
 */

#pragma once
#include "testing.h"
#include <vector>

#include "TileProcessor.h"

namespace grk {

// tile component
struct TileComponent : public grk_rect_u32 {
	TileComponent();
	~TileComponent();

	void create_buffer(grk_rect_u32 unreduced_tile_comp_dims,grk_rect_u32 unreduced_tile_comp_region_dims);
	bool init(bool isCompressor,
			bool whole_tile,
			grk_rect_u32 unreduced_tile_comp_dims,
			grk_rect_u32 unreduced_tile_comp_region_dims,
			uint8_t prec,
			CodingParams *cp,
			TileCodingParams *tcp,
			TileComponentCodingParams* tccp,
			grk_plugin_tile *current_plugin_tile);

	 void allocSparseBuffer(uint32_t numres);
	 void release_mem();
	 bool subbandIntersectsAOI(uint32_t resno,
	 								uint32_t bandno,
	 								const grk_rect_u32 *aoi) const;

	 TileComponentWindowBuffer<int32_t>* getBuffer();
	 bool isWholeTileDecoding();
	 ISparseBuffer* getSparseBuffer();

	Resolution *resolutions; /* resolutions information */
	uint32_t numresolutions; /* number of resolution levels */
	uint32_t resolutions_to_decompress; /* number of resolutions level to decompress (at max)*/
#ifdef DEBUG_LOSSLESS_T2
	Resolution* round_trip_resolutions;  /* round trip resolution information */
#endif
private:
	ISparseBuffer *m_sa;
    bool   whole_tile_decoding;
	bool m_is_encoder;
	TileComponentWindowBuffer<int32_t> *buf;
	TileComponentCodingParams *m_tccp;

};

}




