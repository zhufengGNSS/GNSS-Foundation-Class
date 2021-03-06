//============================================================================
//
//  This file is part of GPSTk, the GPS Toolkit.
//
//  The GPSTk is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 3.0 of the License, or
//  any later version.
//
//  The GPSTk is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GPSTk; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
//  
//  Copyright 2004, The University of Texas at Austin
//  Dagoberto Salazar - gAGE ( http://www.gage.es ). 2006, 2007, 2008
//
//============================================================================

//============================================================================
//
//This software developed by Applied Research Laboratories at the University of
//Texas at Austin, under contract to an agency or agencies within the U.S. 
//Department of Defense. The U.S. Government retains all rights to use,
//duplicate, distribute, disclose, or release this software. 
//
//Pursuant to DoD Directive 523024 
//
// DISTRIBUTION STATEMENT A: This software has been approved for public 
//                           release, distribution is unlimited.
//
//=============================================================================

/**
 * @file SourceID.cpp
 * gpstk::SourceID - Simple index to represent the source of data.
 */

#include "SensorID.h"


namespace gfc
{

   std::map< SensorID::SensorType, std::string > SensorID::stStrings;


   SensorID::Initializer SensorIDsingleton;


   SensorID::Initializer::Initializer()
   {
      stStrings[Unknown]      = "UnknownSource";
      stStrings[GPS]          = "GPS";
      stStrings[Galileo]      = "Galileo";
      stStrings[Glonass]      = "Glonass";
      stStrings[Geosync]      = "Geosync";
      stStrings[LEO]          = "LEO";
      stStrings[Transit]      = "Transit";
      stStrings[BDS]          = "BDS";
      stStrings[Mixed]        = "Mixed";
      stStrings[UserDefined]  = "UserDefined";
      stStrings[DGPS]         = "DGPS";
      stStrings[RTK]          = "RTK";
	  stStrings[NSS]          = "GNSS";
      stStrings[INS]          = "INS";
	  stStrings[Laser]        = "Laser";
	  stStrings[VLBI]         = "VLBI";
      
   }



      // Assignment operator
   SensorID& SensorID::operator=(const SensorID& right)
   {

      if ( this == &right )
      {
         return (*this);
      }
	   	
      type = right.type;
      sensorName = right.sensorName;
	  	
      return *this;

   }  // End of 'SourceID::operator=()'



      // Convenience output method
   std::ostream& SensorID::dump(std::ostream& s) const
   {

      s << SensorID::stStrings[type] << " "
        << sensorName;

      return s;

   }  // End of method 'SourceID::dump()'



      // Returns true if this is a valid SourceID. Basically just
      // checks that none of the fields are undefined.
   bool SensorID::isValid() const
   {

      return !(type==Unknown || sensorName=="");
		
   }  // End of method 'SourceID::isValid()'



      // Method to create a new source type.
   SensorID::SensorType SensorID::newSensorType(const std::string& s)
   {
	  	
      SensorType newType =
         static_cast<SensorType>(SensorID::stStrings.rbegin()->first + 1);
	  
      SensorID::stStrings[newType] = s;
	  	
      return newType;
	
   }  // End of method 'SourceID::newSourceType()'



      // Equality operator requires all fields to be the same.
   bool SensorID::operator==(const SensorID& right) const
   {

      return (type==right.type && sensorName==right.sensorName);

   }  // End of 'SourceID::operator==()'



      // Ordering is arbitrary but required to be able to use a SourceID
      // as an index to a std::map. If an application needs
      // some other ordering, inherit and override this function.
   bool SensorID::operator<(const SensorID& right) const
   {
		
      if (type == right.type)
      {
         return sensorName < right.sensorName;
      }
      else
      {
         return type < right.type;
      }
		
   }  // End of 'SourceID::operator<()'
		


   namespace StringUtils
   {

         // convert this object to a string representation
      std::string asString(const SensorID& p)
      {
        
         std::ostringstream oss;
         p.dump(oss);
		   	
         return oss.str();
		 	
      }  // End of function 'asString()'
		
   }  // End of namespace StringUtils
	  	 	
	  	
      // Stream output for SourceID
   std::ostream& operator<<( std::ostream& s,
                             const SensorID& p )
   {
	  	
      p.dump(s);
	  	
      return s;
	   
   }  // End of 'operator<<'


}  // End of namespace gpstk
