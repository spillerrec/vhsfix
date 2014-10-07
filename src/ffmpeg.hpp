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

#ifndef FFMPEG_HPP
#define FFMPEG_HPP

#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavutil/mathematics.h>

	#include <libavutil/opt.h>
	#include <libavutil/frame.h>
	#include <libavutil/imgutils.h>
}

namespace ffmpeg{


	class Frame{
		private:
			AVFrame *frame;
			
		//Basic ffmpeg properties
		public:
			uint32_t width() const{ return frame->width; }
			uint32_t height() const{ return frame->height; }
			
			AVPixelFormat format() const{ return (AVPixelFormat)frame->format; }
			AVFrame* getFrame(){ return frame; }
			
		public:
			///Gives ownership
			Frame( AVFrame *frame ) : frame(frame) { } //TODO: throw if nullptr?
			
			///Initialize with image data
			Frame( unsigned width, unsigned height, AVPixelFormat format = AV_PIX_FMT_YUV420P )
				:	frame( av_frame_alloc() ) {
				
				if( frame ){
					frame->format = format;
					frame->width = width;
					frame->height = height;
					
					//TODO: check, and throw on both errors
					av_image_alloc(
							frame->data
						,	frame->linesize
						,	frame->width
						,	frame->height
						,	format
						,	32
						);
				}
			}
			
			Frame( const Frame& other )
				: Frame( other.width(), other.height(), other.format() ) {
				av_frame_copy( frame, other.frame ); //TODO: throw on return < 0
			}
			
			//TODO: add move constructor
			
			~Frame(){ av_frame_free( &frame ); }
			
			
			const uint8_t* constScanline( unsigned y ) const {
				return (const uint8_t*)frame->data[0] + y * frame->linesize[0];
			}

			uint8_t* scanline( unsigned y ) {
				return frame->data[0] + y * frame->linesize[0];
			}
			
			
		public:
			class ElemIt{
				private:
					uint8_t* pos;
					unsigned char spacing;
				
				public:
					ElemIt( uint8_t* data, unsigned char spacing )
						:	pos(data), spacing(spacing) { }
					
					uint8_t& operator*() const{ return *pos; }
					ElemIt& operator++(){ pos += spacing; return *this; }
					bool operator!=( const ElemIt& it ) const{ return pos != it.pos; }
			};
			
			class LineIt{
				private:
					uint8_t* data;
					unsigned width;
					unsigned line_width;
					unsigned char spacing;
				
				public:
					LineIt( uint8_t* data, unsigned width, unsigned line_width, unsigned char spacing )
						:	data(data), width(width), line_width(line_width), spacing(spacing) { }
					
					LineIt& operator*(){ return *this; } //TODO: why doesn't const work here?
					LineIt& operator++(){ data += line_width; return *this; }
					bool operator!=( const LineIt& it ) const{ return data != it.data; }
				
					ElemIt begin(){ return ElemIt( data, spacing ); }
					ElemIt end(){ return ElemIt( data + width*spacing, spacing ); }
					
					unsigned size() const{ return width; }
					uint8_t& operator[]( unsigned index ) const{ return data[ index*spacing ]; }
			};
			
			class Plane{
				private:
					uint8_t* data;
					unsigned width;
					unsigned height;
					unsigned line_width;
					unsigned char spacing;
					
				public:
					Plane( uint8_t* data, unsigned width, unsigned height, unsigned line_width, unsigned char spacing )
						:	data(data), width(width), height(height), line_width(line_width), spacing(spacing) { }
					
					unsigned size() const{ return height; }
					LineIt operator[]( unsigned index ){ return LineIt( data + index*line_width, width, line_width, spacing ); }
					
					LineIt begin(){ return (*this)[0]; }
					LineIt end(){ return (*this)[height]; }
					
					unsigned getWidth() const{ return width; }
					unsigned getHeight() const{ return height; }
			};
			
			Plane getPlane( int plane ){
				//Query information about current format
				auto desc = av_pix_fmt_desc_get( format() );
				auto comp = desc->comp[plane];
				
				//Calculate the information we need
				auto offset = comp.offset_plus1 - 1;
				auto spacing = comp.step_minus1 + 1;
				auto real_width = width() >> ((plane > 0) ? desc->log2_chroma_w : 0);
				auto real_height = height() >> ((plane > 0) ? desc->log2_chroma_h : 0);
				
				return Plane( frame->data[comp.plane]+offset, real_width, real_height, frame->linesize[comp.plane], spacing );
			}
	};


}

#endif
