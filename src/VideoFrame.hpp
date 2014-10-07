/*
	This file is part of vhsfix.

	vhsfix is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	vhsfix is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with vhsfix.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VIDEO_FRAME_HPP
#define VIDEO_FRAME_HPP

#include "ffmpeg.hpp"

#include <stdint.h>
#include <vector>

class VideoFrame : public ffmpeg::Frame{
	private:
		std::vector<uint8_t> scaled;
		
	public:
		VideoFrame() : ffmpeg::Frame( 720, 576 ) { }
		
		void initFrame( ffmpeg::Frame& newFrame );
		void process();
		
		void fixFrameAlignment();
		void fixBottom();
		void fixInterlazing();
		
		void separateFrames();
		
		uint8_t getDepth() const{ return 8; }
};

#endif
