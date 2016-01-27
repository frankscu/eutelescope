#ifndef MIMOSA28_H
#define	MIMOSA28_H

  /** @class Mimosa28
	* This class is the implementation of  @class EUTelGenericPixGeoDescr
	* for the Mimosa28 which is the standard telescope reference plane of
	* the DESY pixel telescope.
	* The geoemtry is as following: the 19.8 x 19.2 mm**2 are is divided
	* into a 960 x 928 pixel matrix. All pixels are of the same dimension. 
    */

//STL
#include <string> //std::string
#include <utility> //std::pair

//EUTELESCOPE
#include "EUTelGenericPixGeoDescr.h"

//ROOT
#include "TGeoMaterial.h"
#include "TGeoMedium.h"
#include "TGeoVolume.h"

namespace eutelescope {
namespace geo {

class Mimosa28 : public EUTelGenericPixGeoDescr {
	
	public:
		Mimosa28();
		~Mimosa28();

		void createRootDescr(char const *);
		std::string getPixName(int, int);
		std::pair<int, int> getPixIndex(char const *);

	protected:
		TGeoMaterial* matSi;
		TGeoMedium* Si;
		TGeoVolume* plane;
};

extern "C"
{
	EUTelGenericPixGeoDescr* maker();
}

} //namespace geo
} //namespace eutelescope

#endif	//MIMOSA28_H
