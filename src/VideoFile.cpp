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

#include "VideoFile.hpp"
#include "VideoFrame.hpp"

#include <iostream>

using namespace std;


const char* media_type_to_text( AVMediaType t ){
	switch( t ){
		case AVMEDIA_TYPE_UNKNOWN: return "unknown";
		case AVMEDIA_TYPE_VIDEO: return "video";
		case AVMEDIA_TYPE_AUDIO: return "audio";
		case AVMEDIA_TYPE_SUBTITLE: return "subtitle";
		case AVMEDIA_TYPE_ATTACHMENT: return "attachment";
		default: return "invalid";
	}
}


bool VideoEncode::open(){
	codec = avcodec_find_encoder( AV_CODEC_ID_H264 );
	if( !codec )
		return false;
	
	context = avcodec_alloc_context3( codec );
	
	context->width = 720;
	context->height = 576;
	context->time_base = (AVRational){ 1, 25 };
	context->pix_fmt = AV_PIX_FMT_YUV420P;
	
//	context->bit_rate = 40000000;
	context->gop_size = 25;
	context->max_b_frames = 1;
	
	//TODO: only interlaced
	context->flags |= CODEC_FLAG_INTERLACED_ME | CODEC_FLAG_INTERLACED_DCT;
	av_opt_set( context->priv_data, "preset", "ultrafast", 0 ); //TODO: set to slow
	
	AVDictionary *outDict = nullptr;
	av_dict_set( &outDict, "crf", "18", AV_DICT_APPEND );
	
	if( avcodec_open2( context, codec, &outDict ) < 0 ){
		cout << "Could not open codec" << endl;
		return false;
	}
	
	f = fopen( filename, "wb" );
	if( !f ){
		cout << "Could not open output file" << endl;
		return false;
	}
	
	return true;
}


bool VideoFile::open(){
	if( avformat_open_input( &format_context
		,	filepath.toLocal8Bit().constData(), nullptr, nullptr ) ){
		cout << "Couldn't open file, either missing, unsupported or corrupted\n";
		return false;
	}
	
	if( avformat_find_stream_info( format_context, nullptr ) < 0 ){
		cout << "Couldn't find stream\n";
		return false;
	}
	
	//Find the first video stream (and be happy)
	for( unsigned i=0; i<format_context->nb_streams; i++ ){
		if( format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO ){
			stream_index = i;
			codec_context = format_context->streams[i]->codec;
			break;
		}
	}
	
	if( !codec_context ){
		cout << "Couldn't find a video stream!\n";
		return false;
	}
	
	AVCodec *codec = avcodec_find_decoder( codec_context->codec_id );
	if( !codec ){
		cout << "Does not support this video codec :\\\n";
		return false;
	}
	
	if( avcodec_open2( codec_context, codec, nullptr ) < 0 ){
		cout << "Couldn't open codec\n";
		return false;
	}
	
	return true;
}

void VideoFile::debug_containter(){
	cout << "Number of streams: " << format_context->nb_streams << "\n";
	for( unsigned i=0; i<format_context->nb_streams; i++ ){
		cout << "\t" << i << ":\t" << media_type_to_text( format_context->streams[i]->codec->codec_type ) << "\n";
	}
}

bool VideoFile::seek( unsigned min, unsigned sec ){
	int64_t target = av_rescale_q( min * 60 + sec, format_context->streams[stream_index]->time_base, AV_TIME_BASE_Q );
	if( av_seek_frame( format_context, stream_index, target, AVSEEK_FLAG_ANY ) < 0 ){
		cout << "Couldn't seek\n";
		return false;
	}
	avcodec_flush_buffers( codec_context );
	cout << "target: " << target << "\n";
	return true;
}

bool VideoFile::seek( int64_t byte ){
	if( av_seek_frame( format_context, stream_index, byte, AVSEEK_FLAG_BYTE ) < 0 ){
		cout << "Couldn't seek\n";
		return false;
	}
	avcodec_flush_buffers( codec_context );
	cout << "target: " << byte << "\n";
	return true;
}

void VideoFile::run( VideoEncode& encode ){
	VideoFrame output;
	ffmpeg::Frame frame( av_frame_alloc() );
	
	AVPacket packet;
	int frame_done;
	int current = 0;
	while( av_read_frame( format_context, &packet ) >= 0 ){
		if( packet.stream_index == stream_index ){
			avcodec_decode_video2( codec_context, frame.getFrame(), &frame_done, &packet );
			
			if( frame_done ){
			//	cout << "start frame" << endl;
				output.initFrame( frame );
				output.process();
				
				encode.saveFrame( output.getFrame() );
				current++;
				if( current % 25 == 0 )
					cout << "Time: " << current / 25 << "s\n";
				
				if( current == 400 )
					break;
			}
			
		}
		av_free_packet( &packet );
	}
	
}

