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

#ifndef VIDEO_FILE_HPP
#define VIDEO_FILE_HPP

#include "ffmpeg.hpp"

#include <QString>

#include <cstdio>
#include <stdint.h>


class VideoEncode{
	private:
		const char* filename;
		AVCodec *codec{ nullptr };
		AVCodecContext *context{ nullptr };
		AVPacket packet;
		int index{ 0 };
		std::FILE *f;
		
	public:
		VideoEncode( const char* filename ) : filename(filename) {
		}
		
		~VideoEncode(){
			uint8_t endcode[] = { 0, 0, 1, 0xb7 };
			std::fwrite( endcode, 1, sizeof(endcode), f );
			std::fclose(f);
		}
		
		bool open();
		
		void saveFrame( AVFrame* frame ){
			av_init_packet( &packet );
			packet.data = nullptr;
			packet.size = 0;
			
			frame->pts = index++;
			
			int got_output = false;
			avcodec_encode_video2( context, &packet, frame, &got_output );
			
			if( got_output ){
				std::fwrite( packet.data, 1, packet.size, f );
				av_free_packet( &packet );
			}
		}
};

class VideoFile{
	private:
		QString filepath;
		AVFormatContext* format_context;
		AVCodecContext* codec_context;
		AVPacket packet;
		
		int stream_index;
		
		
	public:
		VideoFile( QString filepath )
			:	filepath( filepath )
			,	format_context( nullptr )
			,	codec_context( nullptr )
			{ }
		
		bool open();
		bool seek( unsigned min, unsigned sec );
		bool seek( int64_t byte );
		void run( VideoEncode& encode );
		void debug_containter();
};

#endif
