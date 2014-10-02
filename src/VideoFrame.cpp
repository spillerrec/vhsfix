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

#include "VideoFrame.hpp"

#include "dump/DumpPlane.hpp"

using namespace std;

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


void VideoFrame::initFrame( ffmpeg::Frame& newFrame ){
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



