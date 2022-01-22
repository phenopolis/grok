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
 *
 */

#include <FileStreamIO.h>
#include "common.h"

FileStreamIO::FileStreamIO() : fileHandle_(nullptr) {}

FileStreamIO::~FileStreamIO()
{
	close();
}

bool FileStreamIO::open(std::string fileName, std::string mode)
{
	bool useStdio = grk::useStdio(fileName.c_str());
	switch(mode[0])
	{
		case 'r':
			if(useStdio)
			{
				if(!grk::grk_set_binary_mode(stdin))
					return false;
				fileHandle_ = stdin;
			}
			else
			{
				fileHandle_ = fopen(fileName.c_str(), "rb");
				if(!fileHandle_)
				{
					spdlog::error("Failed to open {} for reading", fileName);
					return false;
				}
			}
			break;
		case 'w':
			if(!grk::grk_open_for_output(&fileHandle_, fileName.c_str(), useStdio))
				return false;
			break;
	}
	fileName_ = fileName;

	return true;
}
bool FileStreamIO::close(void)
{
	bool rc = true;
	if(!grk::useStdio(fileName_) && fileHandle_)
		rc = grk::safe_fclose(fileHandle_);
	fileHandle_ = nullptr;
	return rc;
}
bool FileStreamIO::write(uint8_t* buf, uint64_t offset, size_t len, size_t maxLen, bool pooled)
{
	(void)offset;
	(void)pooled;
	(void)maxLen;
	auto actual = fwrite(buf, 1, len, fileHandle_);
	if(actual < len)
		spdlog::error("wrote fewer bytes {} than expected number of bytes {}.", actual, len);

	return actual == len;
}
bool FileStreamIO::write(GrkSerializeBuf buffer, grk_serialize_buf* reclaimed,
						 uint32_t max_reclaimed, uint32_t* num_reclaimed)
{
	auto actual = fwrite(buffer.data, 1, buffer.dataLen, fileHandle_);
	(void)reclaimed;
	(void)max_reclaimed;
	(void)num_reclaimed;

	if(actual < buffer.dataLen)
		spdlog::error("wrote fewer bytes {} than expected number of bytes {}.", actual,
					  buffer.dataLen);

	return actual == buffer.dataLen;
}
bool FileStreamIO::read(uint8_t* buf, size_t len)
{
	auto actual = fread(buf, 1, len, fileHandle_);
	if(actual < len)
		spdlog::error("read fewer bytes {} than expected number of bytes {}.", actual, len);

	return actual == len;
}
bool FileStreamIO::seek(uint64_t pos, int whence)
{
	return GRK_FSEEK(fileHandle_, pos, whence) == 0;
}

FILE* FileStreamIO::getFileStream()
{
	return fileHandle_;
}
int FileStreamIO::getFileDescriptor(void)
{
#ifndef __WIN32__
	if(fileHandle_)
		return fileno(fileHandle_);
#endif
	return 0;
}
