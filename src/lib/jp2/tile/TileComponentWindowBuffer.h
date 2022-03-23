/**
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

#include "grk_includes.h"
#include <stdexcept>
#include <algorithm>

/*
 Various coordinate systems are used to describe regions in the tile component buffer.

 1) Canvas coordinates:  JPEG 2000 global image coordinates.

 2) Tile component coordinates: canvas coordinates with sub-sampling applied

 3) Band coordinates: coordinates relative to a specified sub-band's origin

 4) Buffer coordinates: coordinate system where all resolutions are translated
	to common origin (0,0). If each code block is translated relative to the origin of the
 resolution that **it belongs to**, the blocks are then all in buffer coordinate system

 Note: the name of any method or variable returning non canvas coordinates is appended
 with "REL", to signify relative coordinates.

 */

namespace grk
{
enum eSplitOrientation
{
	SPLIT_L,
	SPLIT_H,
	SPLIT_NUM_ORIENTATIONS
};

/**
 * Class: ResWindowBuffer
 *
 * Manage all buffers for a single windowed DWT resolution. This class
 * stores a buffer for the resolution (in REL coordinates),
 * and also buffers for the 4 sub-bands generated by DWT transform
 * (in Canvas coordinates)
 *
 */
template<typename T>
struct ResWindowBuffer
{
	ResWindowBuffer(uint8_t numresolutions, uint8_t resno,
					grkBuffer2d<T, AllocatorAligned>* resWindowTopLevelREL,
					Resolution* tileCompAtRes, Resolution* tileCompAtLowerRes,
					grk_rect32 resWindow, grk_rect32 tileCompWindowUnreduced,
					grk_rect32 tileCompUnreduced, uint32_t FILTER_WIDTH)
		: allocated_(false), tileCompRes_(tileCompAtRes), tileCompResLower_(tileCompAtLowerRes),
		  resWindowBufferREL_(new grkBuffer2d<T, AllocatorAligned>(resWindow.width(),
																   resWindow.height())),
		  resWindowBufferTopLevelREL_(resWindowTopLevelREL), filterWidth_(FILTER_WIDTH)
	{
		for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
			resWindowBufferSplitREL_[i] = nullptr;
		// windowed decompression
		if(FILTER_WIDTH)
		{
			uint32_t numDecomps =
				(resno == 0) ? (uint32_t)(numresolutions - 1U) : (uint32_t)(numresolutions - resno);

			/*
			bandWindowPadded_ is used for determining which precincts and code blocks overlap
			the window of interest, in each respective resolution
			*/
			for(uint8_t orient = 0; orient < ((resno) > 0 ? BAND_NUM_ORIENTATIONS : 1); orient++){
				auto padded = getBandWindow(numDecomps, orient,
						  tileCompWindowUnreduced,
						  tileCompUnreduced, 2 * FILTER_WIDTH);
				bandWindowPadded_.push_back(padded);
			}
			if(tileCompResLower_)
			{
				assert(resno > 0);
				for(uint8_t orient = 0; orient < BAND_NUM_ORIENTATIONS; orient++)
				{
					// todo: should only need padding equal to FILTER_WIDTH, not 2*FILTER_WIDTH
					auto bandWindow = getBandWindow(numDecomps, orient, tileCompWindowUnreduced,
													tileCompUnreduced, 2 * FILTER_WIDTH);
					auto bandFull = orient == BAND_ORIENT_LL ? *((grk_rect32*)tileCompResLower_)
															 : tileCompRes_->tileBand[orient - 1];
					auto bandWindowREL =
						bandWindow.pan(-(int64_t)bandFull.x0, -(int64_t)bandFull.y0);
					bandWindowBufferPaddedREL_.push_back(
						new grkBuffer2d<T, AllocatorAligned>(&bandWindowREL));
				}
				auto winLow = bandWindowBufferPaddedREL_[BAND_ORIENT_LL];
				auto winHigh = bandWindowBufferPaddedREL_[BAND_ORIENT_HL];
				resWindowBufferREL_->x0 = (std::min<uint32_t>)(2 * winLow->x0, 2 * winHigh->x0 + 1);
				resWindowBufferREL_->x1 = (std::max<uint32_t>)(2 * winLow->x1, 2 * winHigh->x1 + 1);
				winLow = bandWindowBufferPaddedREL_[BAND_ORIENT_LL];
				winHigh = bandWindowBufferPaddedREL_[BAND_ORIENT_LH];
				resWindowBufferREL_->y0 = (std::min<uint32_t>)(2 * winLow->y0, 2 * winHigh->y0 + 1);
				resWindowBufferREL_->y1 = (std::max<uint32_t>)(2 * winLow->y1, 2 * winHigh->y1 + 1);

				// todo: shouldn't need to clip
				auto resBounds = grk_rect32(0, 0, tileCompRes_->width(), tileCompRes_->height());
				resWindowBufferREL_->clip(&resBounds);

				// two windows formed by horizontal pass and used as input for vertical pass
				grk_rect32 splitResWindowREL[SPLIT_NUM_ORIENTATIONS];

				splitResWindowREL[SPLIT_L] = grk_rect32(
					resWindowBufferREL_->x0, bandWindowBufferPaddedREL_[BAND_ORIENT_LL]->y0,
					resWindowBufferREL_->x1, bandWindowBufferPaddedREL_[BAND_ORIENT_LL]->y1);

				resWindowBufferSplitREL_[SPLIT_L] =
					new grkBuffer2d<T, AllocatorAligned>(&splitResWindowREL[SPLIT_L]);

				splitResWindowREL[SPLIT_H] = grk_rect32(
					resWindowBufferREL_->x0,
					bandWindowBufferPaddedREL_[BAND_ORIENT_LH]->y0 + tileCompResLower_->height(),
					resWindowBufferREL_->x1,
					bandWindowBufferPaddedREL_[BAND_ORIENT_LH]->y1 + tileCompResLower_->height());

				resWindowBufferSplitREL_[SPLIT_H] =
					new grkBuffer2d<T, AllocatorAligned>(&splitResWindowREL[SPLIT_H]);
			}
			// compression or full tile decompression
		}
		else
		{
			// dummy LL band window
			bandWindowBufferPaddedREL_.push_back(new grkBuffer2d<T, AllocatorAligned>(0, 0));
			assert(tileCompAtRes->numTileBandWindows == 3 || !tileCompAtLowerRes);
			if(tileCompResLower_)
			{
				for(uint32_t i = 0; i < tileCompAtRes->numTileBandWindows; ++i)
				{
					auto b = tileCompAtRes->tileBand + i;
					bandWindowBufferPaddedREL_.push_back(
						new grkBuffer2d<T, AllocatorAligned>(b->width(), b->height()));
				}
				// note: only dimensions of split resolution window buffer matter, not actual
				// coordinates
				for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
					resWindowBufferSplitREL_[i] = new grkBuffer2d<T, AllocatorAligned>(
						resWindow.width(), resWindow.height() / 2);
			}
		}
	}
	~ResWindowBuffer()
	{
		delete resWindowBufferREL_;
		for(auto& b : bandWindowBufferPaddedREL_)
			delete b;
		for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
			delete resWindowBufferSplitREL_[i];
	}
	bool alloc(bool clear)
	{
		if(allocated_)
			return true;

		// if top level window is present, then all buffers attach to this window
		if(resWindowBufferTopLevelREL_)
		{
			// ensure that top level window is allocated
			if(!resWindowBufferTopLevelREL_->alloc2d(clear))
				return false;

			// don't allocate bandWindows for windowed decompression
			if(filterWidth_)
				return true;

			// attach to top level window
			if(resWindowBufferREL_ != resWindowBufferTopLevelREL_)
				resWindowBufferREL_->attach(resWindowBufferTopLevelREL_->getBuffer(),
											resWindowBufferTopLevelREL_->stride);

			// tileCompResLower_ is null for lowest resolution
			if(tileCompResLower_)
			{
				for(uint8_t orientation = 0; orientation < bandWindowBufferPaddedREL_.size();
					++orientation)
				{
					switch(orientation)
					{
						case BAND_ORIENT_HL:
							bandWindowBufferPaddedREL_[orientation]->attach(
								resWindowBufferTopLevelREL_->getBuffer() +
									tileCompResLower_->width(),
								resWindowBufferTopLevelREL_->stride);
							break;
						case BAND_ORIENT_LH:
							bandWindowBufferPaddedREL_[orientation]->attach(
								resWindowBufferTopLevelREL_->getBuffer() +
									tileCompResLower_->height() *
										resWindowBufferTopLevelREL_->stride,
								resWindowBufferTopLevelREL_->stride);
							break;
						case BAND_ORIENT_HH:
							bandWindowBufferPaddedREL_[orientation]->attach(
								resWindowBufferTopLevelREL_->getBuffer() +
									tileCompResLower_->width() +
									tileCompResLower_->height() *
										resWindowBufferTopLevelREL_->stride,
								resWindowBufferTopLevelREL_->stride);
							break;
						default:
							break;
					}
				}
				resWindowBufferSplitREL_[SPLIT_L]->attach(resWindowBufferTopLevelREL_->getBuffer(),
														  resWindowBufferTopLevelREL_->stride);
				resWindowBufferSplitREL_[SPLIT_H]->attach(
					resWindowBufferTopLevelREL_->getBuffer() +
						tileCompResLower_->height() * resWindowBufferTopLevelREL_->stride,
					resWindowBufferTopLevelREL_->stride);
			}
		}
		else
		{
			// resolution window is always allocated
			if(!resWindowBufferREL_->alloc2d(clear))
				return false;

			// band windows are allocated if present
			for(auto& b : bandWindowBufferPaddedREL_)
			{
				if(!b->alloc2d(clear))
					return false;
			}
			if(tileCompResLower_)
			{
				resWindowBufferSplitREL_[SPLIT_L]->attach(resWindowBufferREL_->getBuffer(),
														  resWindowBufferREL_->stride);
				resWindowBufferSplitREL_[SPLIT_H]->attach(resWindowBufferREL_->getBuffer() +
															  tileCompResLower_->height() *
																  resWindowBufferREL_->stride,
														  resWindowBufferREL_->stride);
			}
		}
		allocated_ = true;

		return true;
	}

	/**
	 * Get band window (in tile component coordinates) for specified number
	 * of decompositions
	 *
	 * Note: if numDecomps is zero, then the band window (and there is only one)
	 * is equal to the unreduced tile component window
	 *
	 * See table F-1 in JPEG 2000 standard
	 *
	 */
	static grk_rect32 getBandWindow(uint32_t numDecomps, uint8_t orientation,
									grk_rect32 tileCompWindowUnreduced)
	{
		assert(orientation < BAND_NUM_ORIENTATIONS);
		if(numDecomps == 0)
			return tileCompWindowUnreduced;

		uint32_t tcx0 = tileCompWindowUnreduced.x0;
		uint32_t tcy0 = tileCompWindowUnreduced.y0;
		uint32_t tcx1 = tileCompWindowUnreduced.x1;
		uint32_t tcy1 = tileCompWindowUnreduced.y1;

		/* project window onto sub-band generated by `numDecomps` decompositions */
		/* See equation B-15 of the standard. */
		uint32_t bx0 = orientation & 1;
		uint32_t by0 = (uint32_t)(orientation >> 1U);

		uint32_t bx0Shift = (1U << (numDecomps - 1)) * bx0;
		uint32_t by0Shift = (1U << (numDecomps - 1)) * by0;

		return grk_rect32(
			(tcx0 <= bx0Shift) ? 0 : ceildivpow2<uint32_t>(tcx0 - bx0Shift, numDecomps),
			(tcy0 <= by0Shift) ? 0 : ceildivpow2<uint32_t>(tcy0 - by0Shift, numDecomps),
			(tcx1 <= bx0Shift) ? 0 : ceildivpow2<uint32_t>(tcx1 - bx0Shift, numDecomps),
			(tcy1 <= by0Shift) ? 0 : ceildivpow2<uint32_t>(tcy1 - by0Shift, numDecomps));
	}
	/**
	 * Get band window (in tile component coordinates) for specified number
	 * of decompositions (with padding)
	 *
	 * Note: if numDecomps is zero, then the band window (and there is only one)
	 * is equal to the unreduced tile component window (with padding)
	 */
	static grk_rect32 getBandWindow(uint32_t numDecomps, uint8_t orientation,
									grk_rect32 unreducedTileCompWindow,
									grk_rect32 unreducedTileComp, uint32_t padding)
	{
		assert(orientation < BAND_NUM_ORIENTATIONS);
		if(numDecomps == 0)
		{
			assert(orientation == 0);
			return unreducedTileCompWindow.grow(padding).intersection(&unreducedTileComp);
		}
		auto oneLessDecompWindow = unreducedTileCompWindow;
		auto oneLessDecompTile = unreducedTileComp;
		if(numDecomps > 1)
		{
			oneLessDecompWindow = getBandWindow(numDecomps - 1, 0, unreducedTileCompWindow);
			oneLessDecompTile = getBandWindow(numDecomps - 1, 0, unreducedTileComp);
		}

		return getBandWindow(
			1, orientation, oneLessDecompWindow.grow(2 * padding).intersection(&oneLessDecompTile));
	}

	bool allocated_;

	Resolution* tileCompRes_; // non-null will trigger creation of band window buffers
	Resolution* tileCompResLower_; // null for lowest resolution

	std::vector<grkBuffer2d<T, AllocatorAligned>*> bandWindowBufferPaddedREL_;
	std::vector<grk_rect32> bandWindowPadded_;

	grkBuffer2d<T, AllocatorAligned>* resWindowBufferSplitREL_[SPLIT_NUM_ORIENTATIONS];
	grkBuffer2d<T, AllocatorAligned>* resWindowBufferREL_;
	grkBuffer2d<T, AllocatorAligned>* resWindowBufferTopLevelREL_;

	uint32_t filterWidth_;
};

template<typename T>
struct TileComponentWindowBuffer
{
	TileComponentWindowBuffer(bool isCompressor, bool lossless, bool wholeTileDecompress,
							  grk_rect32 tileCompUnreduced, grk_rect32 tileCompReduced,
							  grk_rect32 unreducedTileCompOrImageCompWindow,
							  Resolution* tileCompResolution, uint8_t numresolutions,
							  uint8_t reducedNumResolutions)
		: unreducedBounds_(tileCompUnreduced), bounds_(tileCompReduced),
		  numResolutions_(numresolutions), compress_(isCompressor),
		  wholeTileDecompress_(wholeTileDecompress)
	{
		if(!compress_)
		{
			// for decompress, we are passed the unreduced image component window
			auto unreducedImageCompWindow = unreducedTileCompOrImageCompWindow;
			bounds_ = unreducedImageCompWindow.rectceildivpow2(
				(uint32_t)(numResolutions_ - reducedNumResolutions));
			bounds_ = bounds_.intersection(tileCompReduced);
			assert(bounds_.isValid());
			unreducedBounds_ = unreducedImageCompWindow.intersection(tileCompUnreduced);
			assert(unreducedBounds_.isValid());
		}
		// fill resolutions vector
		assert(reducedNumResolutions > 0);
		for(uint32_t resno = 0; resno < reducedNumResolutions; ++resno)
			resolution_.push_back(tileCompResolution + resno);

		auto tileCompAtRes = tileCompResolution + reducedNumResolutions - 1;
		auto tileCompAtLowerRes =
			reducedNumResolutions > 1 ? tileCompResolution + reducedNumResolutions - 2 : nullptr;
		// create resolution buffers
		auto topLevel = new ResWindowBuffer<T>(
			numresolutions, (uint8_t)(reducedNumResolutions - 1U), nullptr, tileCompAtRes,
			tileCompAtLowerRes, bounds_, unreducedBounds_, tileCompUnreduced,
			wholeTileDecompress ? 0 : getFilterPad<uint32_t>(lossless));
		// setting top level prevents allocation of tileCompBandWindows buffers
		if(!useBandWindows())
			topLevel->resWindowBufferTopLevelREL_ = topLevel->resWindowBufferREL_;

		for(uint8_t resno = 0; resno < reducedNumResolutions - 1; ++resno)
		{
			// resolution window ==  next resolution band window at orientation 0
			auto resWindow = ResWindowBuffer<T>::getBandWindow((uint32_t)(numresolutions - 1 - resno),
															 0, unreducedBounds_);
			resWindowBuffers.push_back(new ResWindowBuffer<T>(
				numresolutions, resno, useBandWindows() ? nullptr : topLevel->resWindowBufferREL_,
				tileCompResolution + resno, resno > 0 ? tileCompResolution + resno - 1 : nullptr,
				resWindow, unreducedBounds_, tileCompUnreduced,
				wholeTileDecompress ? 0 : getFilterPad<uint32_t>(lossless)));
		}
		resWindowBuffers.push_back(topLevel);
	}
	~TileComponentWindowBuffer()
	{
		for(auto& b : resWindowBuffers)
			delete b;
	}

	/**
	 * Transform code block offsets from canvas coordinates
	 * to either band coordinates (relative to sub band origin)
	 * or buffer coordinates (relative to associated resolution origin)
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {LL,HL,LH,HH}
	 * @param offsetx x offset of code block in canvas coordinates
	 * @param offsety y offset of code block in canvas coordinates
	 *
	 */
	void toRelativeCoordinates(uint8_t resno, eBandOrientation orientation, uint32_t& offsetx,
							   uint32_t& offsety) const
	{
		assert(resno < resolution_.size());

		auto res = resolution_[resno];
		auto band = res->tileBand + getBandIndex(resno, orientation);

		uint32_t x = offsetx;
		uint32_t y = offsety;

		// get offset relative to band
		x -= band->x0;
		y -= band->y0;

		if(useBufferCoordinatesForCodeblock() && resno > 0)
		{
			auto resLower = resolution_[resno - 1U];

			if(orientation & 1)
				x += resLower->width();
			if(orientation & 2)
				y += resLower->height();
		}
		offsetx = x;
		offsety = y;
	}
	/**
	 * Get code block destination window
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {LL,HL,LH,HH}
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>*
		getCodeBlockDestWindowREL(uint8_t resno, eBandOrientation orientation) const
	{
		return (useBufferCoordinatesForCodeblock())
				   ? getResWindowBufferHighestREL()
				   : getBandWindowBufferPaddedREL(resno, orientation);
	}
	/**
	 * Get padded band window buffer
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {0,1,2,3} for {LL,HL,LH,HH} band windows
	 *
	 * If resno is > 0, return LL,HL,LH or HH band window, otherwise return LL resolution window
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>*
		getBandWindowBufferPaddedREL(uint8_t resno, eBandOrientation orientation) const
	{
		assert(resno < resolution_.size());
		assert(resno > 0 || orientation == BAND_ORIENT_LL);

		if(resno == 0 && (compress_ || wholeTileDecompress_))
			return resWindowBuffers[0]->resWindowBufferREL_;

		return resWindowBuffers[resno]->bandWindowBufferPaddedREL_[orientation];
	}

	/**
	 * Get padded band window
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {0,1,2,3} for {LL,HL,LH,HH} band windows
	 *
	 */
	const grk_rect32* getBandWindowPadded(uint8_t resno, eBandOrientation orientation) const
	{
		if(resWindowBuffers[resno]->bandWindowPadded_.empty())
			return nullptr;
		return &resWindowBuffers[resno]->bandWindowPadded_[orientation];
	}
	/*
	 * Get intermediate split window
	 *
	 * @param orientation 0 for upper split window, and 1 for lower split window
	 */
	const grkBuffer2d<T, AllocatorAligned>*
		getResWindowBufferSplitREL(uint8_t resno, eSplitOrientation orientation) const
	{
		assert(resno > 0 && resno < resolution_.size());

		return resWindowBuffers[resno]->resWindowBufferSplitREL_[orientation];
	}
	/**
	 * Get resolution window
	 *
	 * @param resno resolution number
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>* getResWindowBufferREL(uint32_t resno) const
	{
		return resWindowBuffers[resno]->resWindowBufferREL_;
	}
	/**
	 * Get highest resolution window
	 *
	 *
	 */
	grkBuffer2d<T, AllocatorAligned>* getResWindowBufferHighestREL(void) const
	{
		return resWindowBuffers.back()->resWindowBufferREL_;
	}
	bool alloc()
	{
		for(auto& b : resWindowBuffers)
		{
			if(!b->alloc(!compress_))
				return false;
		}

		return true;
	}
	/**
	 * Get bounds of tile component (canvas coordinates)
	 * decompress: reduced canvas coordinates of window
	 * compress: unreduced canvas coordinates of entire tile
	 */
	grk_rect32 bounds() const
	{
		return bounds_;
	}
	grk_rect32 unreducedBounds() const
	{
		return unreducedBounds_;
	}
	uint64_t stridedArea(void) const
	{
		return getResWindowBufferHighestREL()->stride * bounds_.height();
	}

	// set data to buf without owning it
	void attach(T* buffer, uint32_t stride)
	{
		getResWindowBufferHighestREL()->attach(buffer, stride);
	}
	// transfer data to buf, and cease owning it
	void transfer(T** buffer, uint32_t* stride)
	{
		getResWindowBufferHighestREL()->transfer(buffer, stride);
	}

  private:
	bool useBandWindows() const
	{
		return !wholeTileDecompress_;
	}
	bool useBufferCoordinatesForCodeblock() const
	{
		return compress_ || !wholeTileDecompress_;
	}
	uint8_t getBandIndex(uint8_t resno, eBandOrientation orientation) const
	{
		uint8_t index = 0;
		if(resno > 0)
		{
			index = (uint8_t)orientation;
			index--;
		}
		return index;
	}
	/******************************************************/
	// decompress: unreduced/reduced image component window
	// compress:  unreduced/reduced tile component
	grk_rect32 unreducedBounds_;
	grk_rect32 bounds_;
	/******************************************************/

	std::vector<Resolution*> resolution_;
	// windowed bounds for windowed decompress, otherwise full bounds
	std::vector<ResWindowBuffer<T>*> resWindowBuffers;

	// unreduced number of resolutions
	uint8_t numResolutions_;

	bool compress_;
	bool wholeTileDecompress_;
};

} // namespace grk
