/*
 * config.hpp
 * 
 * Copyright 2015 Andrei Tumbar <atadmin@Helios>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <iostream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include "file.hpp"
#include "_misc_tools.hpp"

using namespace std;

class Config
{
	public:
	
	vector<string> writeLines;
	string path;
	ofstream file;
	
	Config ( vector<string> in, string pkgName = "unknown" )
	{
		string path_raw = in[1];
		misc::remove( path_raw, " in the portage(5) man page for more details)" );
		misc::remove( path_raw, " (see " );
		misc::removechar( path_raw, '"' );
		misc::removechar( path_raw, '"' );
		path = "/etc/portage/" + path_raw;
		if ( boost::filesystem::is_directory(path) )
		{
			path = path + "/autogentoo";
			system ( string ("touch " + path ).c_str() );
		}
		
		for ( size_t y = 2; y <= in.size (); y++ )
		{
			writeLines.push_back ( in[y] );
		}
		
		string commentLine = "# Config for the " + pkgName + " emerge set or package\n";
		writeLines.insert( writeLines.begin(), commentLine );
	}
	
	void write ( void )
	{
		file.open ( path.c_str() );
		for ( size_t x; x <= writeLines.size(); x++ )
		{
			file << writeLines[x];
		}
	}
};
