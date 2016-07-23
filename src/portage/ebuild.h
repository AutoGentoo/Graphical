/*
 * ebuild.h
 * 
 * Copyright 2016 Andrei Tumbar <atuser@Kronos-Ubuntu>
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

#ifndef __EBUILD__
#define __EBUILD__

#include <iostream>
#include <string>
#include <cstdlib>
#include <map>
#include "parse_config.h"
#include "package.h"
#include "use.h"

class ebuild
{
  public:
  string REQUIRED_USE; //!< A list of assertions that must be met by the configuration of Use flags to be valid for this ebuild. (Requires EAPI>=4.)
  string SUGGEST_USE; //!< A string of suggested changes to the current use flags, empty if no suggestions
  vector < string > CURRENT_USE; //!< Found by executing the 'equery uses' command to get the currently enabled strings, this vectors only holds the name of the Use flag
  
  ebuild ( Package package );
};

#endif