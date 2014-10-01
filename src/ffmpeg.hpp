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

#include <stdint.h>
#include <cstring>
#include <vector>

namespace ffmpeg{
	namespace api{
		using namespace std;
		extern "C" {
			#include <libavcodec/avcodec.h>
			#include <libavformat/avformat.h>
			#include <libavutil/mathematics.h>

			#include <libavutil/opt.h>
			#include <libavutil/imgutils.h>
		}
	}


	class AVFrame{
		private:
			api::AVFrame *frame;
			
		//Basic ffmpeg properties
		public:
			uint32_t width() const{ return frame->width; }
			uint32_t height() const{ return frame->height; }
			
			api::AVFrame* getFrame(){ return frame; }
			
		public:
			///Gives ownership
			AVFrame( api::AVFrame *frame ) : frame(frame) { } //TODO: throw if nullptr?
			
			///Initialize with image data
			AVFrame( unsigned width, unsigned height, api::AVPixelFormat format = api::AV_PIX_FMT_YUV420P )
				:	frame( api::av_frame_alloc() ) {
				
				if( frame ){
					frame->format = format;
					frame->width = width;
					frame->height = height;
					
					//TODO: check, and throw on both errors
					api::av_image_alloc(
							frame->data
						,	frame->linesize
						,	frame->width
						,	frame->height
						,	format
						,	32
						);
				}
			}
			
			//TODO: add move constructor
			
			~AVFrame(){ api::av_frame_free( &frame ); }
			
			
			const uint8_t* constScanline( unsigned y ) const {
				return (const uint8_t*)frame->data[0] + y * frame->linesize[0];
			}

			uint8_t* scanline( unsigned y ) {
				return frame->data[0] + y * frame->linesize[0];
			}
			
			
		public:
			struct LineIt{
				uint8_t* pos;
				unsigned char spacing;
				
				public:
					LineIt( uint8_t* data, unsigned char spacing )
						:	pos(data), spacing(spacing) { }
					
					LineIt& operator++(){ pos += spacing; return *this; }
					bool operator==( const LineIt& it ) const{ return pos == it.pos; }
					bool operator!=( const LineIt& it ) const{ return !( (*this) == it); }
					
					uint8_t& operator*() const{ return *pos; }
					uint8_t* operator->() const{ return pos; }
					uint8_t& operator[]( unsigned index ) const{ return pos[ index * spacing ]; }
			};
			
			struct iterator{
				LineIt line;
				unsigned width;
				unsigned line_width;
				
				public:
					iterator( uint8_t* data, unsigned width, unsigned line_width, unsigned char spacing )
						:	line( data, spacing ), width(width), line_width(line_width) { }
					
					iterator& operator++(){ line.pos += line_width; return *this; }
					bool operator==( const iterator& it ) const{ return line == it.line; }
					bool operator!=( const iterator& it ) const{ return !( (*this) == it); }
					
					//TODO: ref?
					iterator& operator*() { return *this; } //TODO: const?
					//TODO: LineIt operator[]( unsigned index ) const{ return pos[ index * width ]; }
					
					LineIt begin(){ return line; }
					LineIt end(){ return LineIt( &line[width], line.spacing ); }
			};
			
			iterator begin()
				{ return iterator( frame->data[0], width(), frame->linesize[0], 1 ); }
			
			iterator end(){
				auto linesize = frame->linesize[0];
				return iterator( frame->data[0] + height()*linesize, width(), linesize, 1 );
			}
	};


}


