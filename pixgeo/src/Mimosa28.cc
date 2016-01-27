#include "Mimosa28.h"

namespace eutelescope {
namespace geo {

Mimosa28::Mimosa28(): EUTelGenericPixGeoDescr(	19.8, 19.2, 0.02,	//size X, Y, Z
						0, 959, 0, 927,	//min max X,Y
						9.3660734 )		//rad length
{
	//Create the material for the sensor
	matSi = new TGeoMaterial( "Si", 28.0855 , 14.0, 2.33, -_radLength, 45.753206 );
	Si = new TGeoMedium("MimosaSilicon",1, matSi);

	//Create a plane for the sensitive area
	plane = _tGeoManager->MakeBox( "sensarea_mimosa", Si, 9.9, 9.6, 0.01 );
	//Divide the regions to create pixels
  	TGeoVolume* row = plane->Divide("mimorow", 1 , 960 , 0 , 1, 0, "N"); 
	row->Divide("mimopixel", 2 , 928, 0 , 1, 0, "N");

}

Mimosa28::~ Mimosa28()
{
	//It appears that ROOT will take ownership and delete that stuff! 
	//delete matSi,
	//delete Si;
}

void  Mimosa28::createRootDescr(char const * planeVolume)
{
	//Get the plane as provided by the EUTelGeometryTelescopeGeoDescription
	TGeoVolume* topplane =_tGeoManager->GetVolume(planeVolume);
	//Add the sensitive area to the plane
	topplane->AddNode(plane, 1);
    
}

std::string Mimosa28::getPixName(int x , int y)
{
	char buffer [100];
	//return path to the pixel, don't forget to shift indices by +1+
	snprintf( buffer, 100, "/sensarea_mimosa_1/mimorow_%d/mimopixel_%d", x+1, y+1);
	return std::string( buffer ); 
}
	/*TODO*/ std::pair<int, int>  Mimosa28::getPixIndex(char const*){return std::make_pair(0,0); }

EUTelGenericPixGeoDescr* maker()
{
	Mimosa28* mPixGeoDescr = new Mimosa28();
	return dynamic_cast<EUTelGenericPixGeoDescr*>(mPixGeoDescr);
}

} //namespace geo
} //namespace eutelescope

