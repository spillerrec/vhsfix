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

#include "dump/DumpPlane.hpp"

#include "ffmpeg.hpp"

using namespace ffmpeg::api;


#include <QCoreApplication>
#include <QStringList>
#include <QFile>

#include <iostream>

//NOTE: None of this supports depths higher than 8 bits

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


class VideoFrame : public ffmpeg::AVFrame{
	private:
		vector<uint8_t> scaled;
		
	public:
		VideoFrame() : ffmpeg::AVFrame( 720, 576 ) { }
		
		void initFrame( ffmpeg::AVFrame& newFrame );
		void process();
		
		uint8_t getDepth() const{ return 8; }
};

class VideoLine{
	private:
		uint8_t* data;
		uint32_t width;
		
	public:
		VideoLine( VideoFrame& frame, unsigned y ){
			data = frame.scanline( y );
			width = frame.width();
		}
		
		VideoLine( vector<uint8_t>& buffer ){
			data = buffer.data();
			width = buffer.size();
		}
		
		uint32_t getWidth() const{ return width; }

		uint8_t& operator[]( unsigned x ){ return data[x]; }
		const uint8_t& operator[]( unsigned x ) const{ return data[x]; }
};

double cubic( double b, double c, double x ){
	x = abs( x );
	
	if( x < 1 )
		return
				(12 - 9*b - 6*c)/6 * x*x*x
			+	(-18 + 12*b + 6*c)/6 * x*x
			+	(6 - 2*b)/6
			;
	else if( x < 2 )
		return
				(-b - 6*c)/6 * x*x*x
			+	(6*b + 30*c)/6 * x*x
			+	(-12*b - 48*c)/6 * x
			+	(8*b + 24*c)/6
			;
	else
		return 0;
}

double scale_func( double x ){
	return cubic( 1.0/3, 1.0/3, x );
}


void scaleLineEx( const VideoLine& p, double x_scale, vector<uint8_t>& data ){
	unsigned width = unsigned(p.getWidth() * x_scale);
	data.resize( width );
	
	for( unsigned ix=0; ix<data.size(); ix++ ){
		double pos = ix * (p.getWidth()-1) / ((p.getWidth()-1) * x_scale);
		int left = floor( pos - 2 );
		unsigned right = ceil( pos + 2 );
		
		//Limit
		left = max( left, 0 );
		right = min( right, p.getWidth() );
		
		double sum = 0.0;
		double amount = 0.0;
		for( unsigned jx=left; jx<=right; jx++ ){
			double w = scale_func( jx - pos );
			sum += w * p[jx];
			amount += w;
		}
		data[ix] = sum / amount;
	}
}

vector<uint8_t> scaleLine( const VideoLine& p, double x_scale ){
	vector<uint8_t> data;
	scaleLineEx( p, x_scale, data );
	return data;
}

vector<uint8_t> scaleLine( VideoFrame& frame, unsigned y, double x_scale ){
	VideoLine p( frame, y );
	return scaleLine( p, x_scale );
}

void scaleLineEx( VideoFrame& frame, unsigned y, double x_scale, vector<uint8_t>& data ){
	VideoLine p( frame, y );
	scaleLineEx( p, x_scale, data );
}


vector<uint8_t> scaleLineDown( const VideoLine& p, double x_scale ){
	x_scale = 1.0 / x_scale;
	unsigned width = unsigned(p.getWidth() * x_scale);
	vector<uint8_t> data( width, 0 ); //TODO: same as upscale
	
	for( unsigned ix=0; ix<data.size(); ix++ ){
		unsigned left_big = max( ix, 2u ) - 2;
		unsigned right_big = min( ix+2, p.getWidth()-1 );
		
		//Scale
		unsigned left = left_big * (p.getWidth()-1) / (data.size()-1);
		unsigned right = right_big * (p.getWidth()-1) / (data.size()-1);
		double center = ix * (p.getWidth()-1.0) / (data.size()-1.0);
		
		double sum = 0.0;
		double amount = 0.0;
		for( unsigned jx=left; jx<=right; jx++ ){
			double w = scale_func( (jx - center)*x_scale );
			sum += w * p[jx];
			amount += w;
		}
		data[ix] = sum / amount;
	}
	
	return data;
}

vector<uint8_t> scaleLineDown( vector<uint8_t>& p, double x_scale ){
	VideoLine line( p );
	return scaleLineDown( line, x_scale );
}

unsigned diffLines( const VideoLine& p1, const VideoLine& p2, int dx ){
	unsigned sum=0;
	for( unsigned ix=0; ix<p1.getWidth(); ix++ ){
		unsigned pos = unsigned(ix+dx+p2.getWidth()) % p2.getWidth();
		auto error = abs( (int)p1[ix] - (int)p2[pos] );
		sum += error;
	}
	
	return sum;
}


pair<unsigned,int> recursiveDiff( const VideoLine& p1, const VideoLine& p2, int left, int right ){
	if( right - left <= 1 )
		return make_pair( diffLines( p1, p2, left ), left );
	else{
	//	cout << "In: " << left << " - " << right << endl;
		auto middle = (right - left) / 2 + left;
		auto newLeft = (middle - left) / 2 + left;
		auto newRight = (right - middle) / 2 + middle;
		
	//	cout << "Out: " << newLeft << " - " << newRight << endl;
		auto leftVal  = diffLines( p1, p2, newLeft );
		auto rightVal = diffLines( p1, p2, newRight );
		
		if( leftVal < rightVal )
			return recursiveDiff( p1, p2, left, middle );
		else
			return recursiveDiff( p1, p2, middle, right );
	}
}

/*
bool bestDiffEx( const VideoLine& p1, const VideoLine& p2, int& best_x2, unsigned& best_val, int amount=10 ){
	bool improved = false;
	
	unsigned best = -1;
	int best_x = -1;
	for( int dx=-amount; dx<=amount; dx++ ){
		auto current = diffLines( p1, p2, dx );
		if( current < best ){
			best = current;
			best_x = dx;
		}
	}
	
	if( best < best_val ){
		improved = true;
		best_val = best;
		best_x2 = best_x;
	}
	return improved;
}
/*/
bool bestDiffEx( const VideoLine& p1, const VideoLine& p2, int& best_x2, unsigned& best_val, int amount=10 ){
	auto result = recursiveDiff( p1, p2, -amount, amount );
	if( result.first < best_val ){
		best_x2 = result.second;
		best_val = result.first;
		return true;
	}
	return false;
}
//*/

int bestDiff( vector<uint8_t>& line1, vector<uint8_t>& line2, int amount ){
	VideoLine l1( line1 ), l2( line2 );
	
	unsigned best_val = -1;
	int best_x = 0;
	bestDiffEx( l1, l2, best_x, best_val, amount );
	return best_x;
}

void moveLine( VideoLine& p, VideoLine& out, int dx ){
	for( unsigned ix=0; ix<out.getWidth(); ix++ ){
		unsigned pos = unsigned(ix+dx+p.getWidth()) % p.getWidth();
		out[ix] = p[pos];
	}
}

void moveLine( vector<uint8_t>& p, vector<uint8_t>& out, int dx ){
	VideoLine p1( p ), out1( out );
	moveLine( p1, out1, dx );
}

void writeLine( const vector<uint8_t>& p, VideoFrame& out, unsigned y, int dx ){
	auto row2 = out.scanline( y );
	
	for( unsigned ix=0; ix<out.width(); ix++ ){
		unsigned pos = unsigned(ix+dx+p.size()) % p.size();
		row2[ix] = p[pos];
	}
}
void swapLine( const VideoFrame& p, VideoFrame& out, unsigned y, unsigned y_out ){
	auto row1 = p.constScanline( y );
	auto row2 = out.scanline( y_out );
	
	for( unsigned ix=0; ix<p.width(); ix++ )
		row2[ix] = row1[ix];
}
/* DumpPlane separateFrames( const DumpPlane& p ){
	DumpPlane out( p );
	
	for( unsigned iy=0; iy<p.getHeight(); iy+=2 )
		swapLine( p, out, iy, iy/2 );
	for( unsigned iy=1; iy<p.getHeight(); iy+=2 )
		swapLine( p, out, iy, iy/2+p.getHeight()/2 );
	
	return out;
}
*/
DumpPlane& blurFrames( DumpPlane& p ){
	for( unsigned iy=0; iy<p.getHeight(); iy+=2 ){
		auto row1 = p.scanline( iy );
		auto row2 = p.scanline( iy+1 );
		for( unsigned ix=0; ix<p.getWidth(); ix++ ){
			auto mid = (row1[ix] + row2[ix]) / 2;
			row1[ix] = row2[ix] = mid;
		}
	}
	
	return p;
}


void VideoFrame::initFrame( ffmpeg::AVFrame& newFrame ){
	//Copy luma
	auto luma_out = getPlane( 0 );
	auto luma_in = newFrame.getPlane( 0 );
	for( unsigned iy=0; iy<luma_in.size(); iy++ ){
		auto in = luma_in[iy];
		auto out_l = luma_out[iy];
		
		for( unsigned ix=0; ix<in.size(); ix++ )
			out_l[ix] = in[ix];
	}
	
	//Blend the halved chroma to quartered chroma
	for( int plane=1; plane<=2; plane++ ){
		auto in_plane = newFrame.getPlane( plane );
		auto out_plane = getPlane( plane );
		
		for( unsigned iy=0; iy<in_plane.size(); iy += 2 ){
			auto in = in_plane[iy], in2 = in_plane[iy+1];
			auto out = out_plane[iy/2];
			for( unsigned ix=0; ix<out.size(); ix++ )
				out[ix] = (in[ix] + in2[ix]) / 2;
		}
	}
}

void VideoFrame::process(){
	double scale_factor = 5;
	auto top = scaleLine( *this, 0, scale_factor );
	auto bottom = top;
	
	/*
	//TODO: upscale 10x
	for( unsigned iy=0; iy<576-8; iy+=2 ){
		//Prepare new lines
		top = bottom;
		bottom = scaleLine( *this, iy+2, scale_factor );
		auto middle = scaleLine( *this, iy+1, scale_factor );
		
	//	unsigned base = diffLines( middle, top, 0 ); // 1  0
		
		int best_x = bestDiff( top, middle, 10 ); // 0  1
		int best_x2 = bestDiff( bottom, middle, 10 ); // 2  1
		if( iy == 0 )
			best_x = best_x2;
		if( iy == 576-8-2 )
			best_x2 = best_x;
		
	//	cout << "Best dx (" << iy << "): " << best_x << " - " << best_x2 << endl;
		
		auto middleCopy = middle;
		moveLine( middleCopy, middle, (best_x+best_x2)/2 );
		auto output = scaleLineDown( middle, scale_factor );
		writeLine( output, *this, iy+1, 0 );
		//moveLine( p, out, iy+1, (best_x+best_x2)/2 );
		//TODO: downscale again
	}
	
	//*/
	
	
	//* Lines in bottom fix
	VideoLine base( *this, 576-8-2 );
	for( unsigned iy=576-8; iy<height(); iy++ ){
		double best_scale = 1.0;
		int best_x = 0;
		unsigned best_val = -1;
		
		for( double iz = 1.000; iz<1.03; iz += 0.001 ){
			scaleLineEx( *this, iy, iz, scaled );
			if( bestDiffEx( base, scaled, best_x, best_val, 200 ) )
				best_scale = iz;
		}
		scaleLineEx( *this, iy, best_scale, scaled );
		writeLine( scaled, *this, iy, best_x );
	//	cout << "scale: " << best_scale << endl;
	//	cout << "best_x: " << best_x << endl;
	}
	
	//*/
	
	/*/ Blend
	
	for( unsigned iy=0; iy<576-8; iy+=2 ){
		VideoLine top( *this, iy ), bottom( *this, iy+1 );
		for( unsigned ix=0; ix<720; ix++ )
			top[ix] = bottom[ix] = (top[ix] + bottom[ix]) / 2;
	}
	
	//*/
}


class VideoEncode{
	private:
		const char* filename;
		AVCodec *codec{ nullptr };
		AVCodecContext *context{ nullptr };
		AVPacket packet;
		int index{ 0 };
		FILE *f;
		
	public:
		VideoEncode( const char* filename ) : filename(filename) {
		}
		
		~VideoEncode(){
			uint8_t endcode[] = { 0, 0, 1, 0xb7 };
			fwrite( endcode, 1, sizeof(endcode), f );
			fclose(f);
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
				fwrite( packet.data, 1, packet.size, f );
				av_free_packet( &packet );
			}
		}
};

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
	ffmpeg::AVFrame frame( av_frame_alloc() );
	
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

DumpPlane scalePlane( const DumpPlane& p, double x_scale ){
	unsigned width = unsigned(p.getWidth() * x_scale);
	vector<uint8_t> data( width * p.getHeight(), 0 );
	DumpPlane out( width, p.getHeight(), p.getDepth(), data );
	
	for( unsigned iy=0; iy<out.getHeight(); iy++ ){
		auto row_out = out.scanline( iy );
		auto row_in = p.constScanline( iy );
		
		for( unsigned ix=0; ix<out.getWidth(); ix++ ){
			double pos = ix * (p.getWidth()-1) / ((p.getWidth()-1) * x_scale);
			int left = floor( pos - 2 );
			unsigned right = ceil( pos + 2 );
			
			//Limit
			left = max( left, 0 );
			right = min( right, p.getWidth() );
			
			double sum = 0.0;
			double amount = 0.0;
			for( unsigned jx=left; jx<=right; jx++ ){
				double w = scale_func( jx - pos );
				sum += w * row_in[jx];
				amount += w;
			}
			row_out[ix] = sum / amount;
		}
	}
	
	return out;
}

DumpPlane scalePlaneDown( const DumpPlane& p, double x_scale ){
	x_scale = 1.0 / x_scale;
	unsigned width = unsigned(p.getWidth() * x_scale);
	vector<uint8_t> data( width * p.getHeight(), 0 );
	DumpPlane out( width, p.getHeight(), p.getDepth(), data );
	
	for( unsigned iy=0; iy<out.getHeight(); iy++ ){
		auto row_out = out.scanline( iy );
		auto row_in = p.constScanline( iy );
		
		for( unsigned ix=0; ix<out.getWidth(); ix++ ){
			
			unsigned left_big = max( ix, 2u ) - 2;
			unsigned right_big = min( ix+2, p.getWidth()-1 );
			
			//Scale
			unsigned left = left_big * (p.getWidth()-1) / (out.getWidth()-1);
			unsigned right = right_big * (p.getWidth()-1) / (out.getWidth()-1);
			double center = ix * (p.getWidth()-1.0) / (out.getWidth()-1.0);
			
			double sum = 0.0;
			double amount = 0.0;
			for( unsigned jx=left; jx<=right; jx++ ){
				double w = scale_func( (jx - center)*x_scale );
				sum += w * row_in[jx];
				amount += w;
			}
			row_out[ix] = sum / amount;
		}
	}
	
	return out;
}

int showHelp( int return_code=0 ){
	cout << "vhsfix filename unused" << endl;
	
	return return_code;
}

int main(int argc, char *argv[]){
	av_register_all();
	
	QCoreApplication a(argc, argv);
	auto args = a.arguments();
	
	if( args.size() < 3 )
		return showHelp( -1 );
	
	VideoFile file( args[1] );
	
	VideoEncode encode( "test.h264" );
	if( !encode.open() ){
		cout << "Could not create output file" << endl;
		return -1;
	}
	
	//Open video file
	if( !(file.open()) ){
		cout << "Couldn't open file!";
		return -1;
	}
	
	file.run( encode );
	
	return 0;
}
