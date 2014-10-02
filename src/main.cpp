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

#include "ffmpeg.hpp"
#include "VideoFrame.hpp"
#include "VideoFile.hpp"

#include <QCoreApplication>
#include <QStringList>
#include <QFile>

#include <iostream>

using namespace std;


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
