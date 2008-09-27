// -*- mode: c++; mode: auto-fill; mode: flyspell-prog; -*-
// Author Silvia Bonfanti, Uni. Insubria  <mailto:silviafisica@gmail.com>
// Author Loretta Negrini, Uni. Insubria  <mailto:loryneg@gmail.com>
// Version $Id: EUTelCorrelator.cc,v 1.10 2008-09-27 16:03:48 bulgheroni Exp $
/*
 *   This source code is part of the Eutelescope package of Marlin.
 *   You are free to use this source files for your own development as
 *   long as it stays in a public research context. You are not
 *   allowed to use it for commercial purpose. You must put this
 *   header with author names in all development based on this file.
 *
 */

// eutelescope includes ".h"
#include "EUTelCorrelator.h"
#include "EUTelRunHeaderImpl.h"
#include "EUTelEventImpl.h"
#include "EUTELESCOPE.h"
#include "EUTelVirtualCluster.h"
#include "EUTelFFClusterImpl.h"
#include "EUTelSparseClusterImpl.h"
#include "EUTelExceptions.h"

// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/Global.h"

// aida includes <.h>
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/IHistogram2D.h>
#include <AIDA/ITree.h>
#endif

#if defined(USE_GEAR)
// gear includes <.h>
#include <gear/GearMgr.h>
#include <gear/SiPlanesParameters.h>
#endif

// lcio includes <.h>
#include <IMPL/LCCollectionVec.h>
#include <IMPL/TrackerPulseImpl.h>
#include <IMPL/TrackerHitImpl.h>
#include <UTIL/CellIDDecoder.h>

// system includes <>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace marlin;
using namespace eutelescope;

// definition of static members mainly used to name histograms
#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)
std::string EUTelCorrelator::_clusterXCorrelationHistoName   = "ClusterXCorrelationHisto";
std::string EUTelCorrelator::_hitXCorrelationHistoName       = "HitXCorrelatioHisto";
std::string EUTelCorrelator::_hitYCorrelationHistoName       = "HitYCorrelationHisto";
std::string EUTelCorrelator::_clusterYCorrelationHistoName   = "ClusterYCorrelationHisto";
#endif

EUTelCorrelator::EUTelCorrelator () : Processor("EUTelCorrelator") {

  // modify processor description
  _description =
    "EUTelCorrelator fills histograms with correlation plots";

  registerInputCollection(LCIO::TRACKERPULSE,"InputClusterCollectionName",
                          "Cluster (pulse) collection name",
                          _inputClusterCollectionName, string ( "cluster" ));

  registerInputCollection(LCIO::TRACKERHIT,"InputHitCollectionName",
                          "Hit collection name",
                          _inputHitCollectionName, string ( "hit" ));


}


void EUTelCorrelator::init() {
  // this method is called only once even when the rewind is active
  // usually a good idea to
  printParameters ();

  // set to zero the run and event counters
  _iRun = 0;
  _iEvt = 0;

#ifndef USE_GEAR

  // GEAR is really needed only if we want to do correlation among
  // hits. At this point, we still don't know if we will have to do
  // hit correlation or not, keep the possibility still open.

#else

  // check if the GEAR manager pointer is not null!
  if ( Global::GEAR == 0x0 ) {
    streamlog_out ( ERROR4 ) <<  "The GearMgr is not available, for an unknown reason." << endl;
    exit(-1);
  }

  _siPlanesParameters  = const_cast<gear::SiPlanesParameters* > (&(Global::GEAR->getSiPlanesParameters()));
  _siPlanesLayerLayout = const_cast<gear::SiPlanesLayerLayout*>
    ( &(_siPlanesParameters->getSiPlanesLayerLayout() ));

  _siPlaneZPosition = new double[ _siPlanesLayerLayout->getNLayers() ];
  for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); iPlane++ ) {
    _siPlaneZPosition[ iPlane ] = _siPlanesLayerLayout->getLayerPositionZ(iPlane);
  }

#endif



}

void EUTelCorrelator::processRunHeader (LCRunHeader * rdr) {


  EUTelRunHeaderImpl * runHeader = new EUTelRunHeaderImpl( rdr ) ;

  _noOfDetectors = runHeader->getNoOfDetector();

  // the four vectors containing the first and the last pixel
  // along both the directions
  _minX = runHeader->getMinX();
  _maxX = runHeader->getMaxX();
  _minY = runHeader->getMinY();
  _maxY = runHeader->getMaxY();


  // perform some consistency check in case we have GEAR support
#ifdef USE_GEAR

  // the run header contains the number of detectors. This number
  // should be in principle be the same as the number of layers in the
  // geometry description
  if ( runHeader->getNoOfDetector() != _siPlanesParameters->getSiPlanesNumber() ) {
    streamlog_out ( ERROR4 ) << "Error during the geometry consistency check: " << endl
                             << "The run header says there are " << runHeader->getNoOfDetector()
                             << " silicon detectors " << endl
                             << "The GEAR description says     " << _siPlanesParameters->getSiPlanesNumber()
                             << " silicon planes" << endl;
    exit(-1);
  }

  // this is the right place also to check the geometry ID. This is a
  // unique number identifying each different geometry used at the
  // beam test. The same number should be saved in the run header and
  // in the xml file. If the numbers are different, instead of barely
  // quitting ask the user what to do.

  if ( runHeader->getGeoID() == 0 )
    streamlog_out ( WARNING0 ) <<  "The geometry ID in the run header is set to zero." << endl
                               <<  "This may mean that the GeoID parameter was not set" << endl;


  if ( runHeader->getGeoID() != _siPlanesParameters->getSiPlanesID() ) {
    streamlog_out ( ERROR1 ) <<  "Error during the geometry consistency check: " << endl
                             << "The run header says the GeoID is " << runHeader->getGeoID() << endl
                             << "The GEAR description says is     " << _siPlanesParameters->getSiPlanesID()
                             << endl;
    string answer;
    while (true) {
      streamlog_out ( ERROR1 ) << "Type Q to quit now or C to continue using the actual GEAR description anyway [Q/C]" << endl;
      cin >> answer;
      // put the answer in lower case before making the comparison.
      transform( answer.begin(), answer.end(), answer.begin(), ::tolower );
      if ( answer == "q" ) {
        exit(-1);
      } else if ( answer == "c" ) {
        break;
      }
    }
  }

#endif // USE_GEAR


  delete runHeader;

  // increment the run counter
  ++_iRun;
}


void EUTelCorrelator::processEvent (LCEvent * event) {

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  if (_iEvt % 10 == 0)
    streamlog_out( MESSAGE4 ) << "Processing event "
                              << setw(6) << setiosflags(ios::right) << event->getEventNumber() << " in run "
                              << setw(6) << setiosflags(ios::right) << setfill('0')  << event->getRunNumber()
                              << setfill(' ') << " (Total = " << setw(10) << _iEvt << ")"
                              << resetiosflags(ios::left) << endl;
  ++_iEvt;


  EUTelEventImpl * evt = static_cast<EUTelEventImpl*> (event) ;

  if ( evt->getEventType() == kEORE ) {
    streamlog_out ( DEBUG4 ) << "EORE found: nothing else to do." << endl;
    return;
  } else if ( evt->getEventType() == kUNKNOWN ) {
    streamlog_out ( WARNING2 ) << "Event number " << evt->getEventNumber() << " in run " << evt->getRunNumber()
                               << " is of unknown type. Continue considering it as a normal Data Event."
                               << endl;
  }
  // if the Event that we are looking is the first we create files
  // with histograms.
  if ( isFirstEvent() ) {

    try {
      // let's check if we have cluster collections
      event->getCollection( _inputClusterCollectionName );

      _hasClusterCollection = true;

    } catch ( lcio::Exception& e ) {

      _hasClusterCollection = false;
    }

#ifdef USE_GEAR
    try {
      // let's check if we have hit collections

      event->getCollection( _inputHitCollectionName ) ;

      _hasHitCollection = true;

    } catch ( lcio::Exception& e ) {

      _hasHitCollection = false;
    }

#else
    // if we don't have GEAR, so we can't process hit even if the hit
    // collection is available in the file

    _hasHitCollection = false;

#endif // USE_GEAR

    bookHistos();

    _isFirstEvent = false;

  }



  try {

    if ( _hasClusterCollection ) {

      LCCollectionVec * inputClusterCollection   = static_cast<LCCollectionVec*>
        (event->getCollection( _inputClusterCollectionName ));

      CellIDDecoder<TrackerPulseImpl>  pulseCellDecoder( inputClusterCollection );

      // we have an external detector where we consider a cluster each
      // time (external cluster) that is correlated with another
      // detector's clusters (internal cluster)

      for ( size_t iExt = 0 ; iExt < inputClusterCollection->size() ; ++iExt ) {

        TrackerPulseImpl * externalPulse = static_cast< TrackerPulseImpl * >
          ( inputClusterCollection->getElementAt( iExt ) );

        EUTelVirtualCluster  * externalCluster;

        ClusterType type = static_cast<ClusterType>
          (static_cast<int>((pulseCellDecoder(externalPulse)["type"])));
        // we check that the type of cluster is ok

        if ( type == kEUTelFFClusterImpl ) {
          externalCluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl*>
                                                    ( externalPulse->getTrackerData()) );

        } else if ( type == kEUTelSparseClusterImpl ) {

          // ok the cluster is of sparse type, but we also need to know
          // the kind of pixel description used. This information is
          // stored in the corresponding original data collection.

          LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
          TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
          CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
          SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

          // now we know the pixel type. So we can properly create a new
          // instance of the sparse cluster
          if ( pixelType == kEUTelSimpleSparsePixel ) {
            externalCluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
              ( static_cast<TrackerDataImpl *> ( externalPulse->getTrackerData()  ) );
          } else {
            streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
            throw UnknownDataTypeException("Pixel type unknown");
          }

        } else {
          streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
          throw UnknownDataTypeException("Cluster type unknown");
        }

        int externalSensorID = pulseCellDecoder( externalPulse ) [ "sensorID" ] ;

        float externalXCenter;
        float externalYCenter;

        // we catch the coordinates of the external seed

        externalCluster->getCenterOfGravity( externalXCenter, externalYCenter ) ;

        for ( size_t iInt = 0;  iInt <  inputClusterCollection->size() ; ++iInt ) {

          TrackerPulseImpl * internalPulse = static_cast< TrackerPulseImpl * >
            ( inputClusterCollection->getElementAt( iInt ) );

          EUTelVirtualCluster  * internalCluster;

          ClusterType type = static_cast<ClusterType>
            (static_cast<int>((pulseCellDecoder(internalPulse)["type"])));

          // we check that the type of cluster is ok

          if ( type == kEUTelFFClusterImpl ) {
            internalCluster = new EUTelFFClusterImpl( static_cast<TrackerDataImpl*>
                                                      (internalPulse->getTrackerData()) );

          } else if ( type == kEUTelSparseClusterImpl ) {

            // ok the cluster is of sparse type, but we also need to know
            // the kind of pixel description used. This information is
            // stored in the corresponding original data collection.

            LCCollectionVec * sparseClusterCollectionVec = dynamic_cast < LCCollectionVec * > (evt->getCollection("original_zsdata"));
            TrackerDataImpl * oneCluster = dynamic_cast<TrackerDataImpl*> (sparseClusterCollectionVec->getElementAt( 0 ));
            CellIDDecoder<TrackerDataImpl > anotherDecoder(sparseClusterCollectionVec);
            SparsePixelType pixelType = static_cast<SparsePixelType> ( static_cast<int> ( anotherDecoder( oneCluster )["sparsePixelType"] ));

            // now we know the pixel type. So we can properly create a new
            // instance of the sparse cluster
            if ( pixelType == kEUTelSimpleSparsePixel ) {
              internalCluster = new EUTelSparseClusterImpl< EUTelSimpleSparsePixel >
                ( static_cast<TrackerDataImpl *> ( internalPulse->getTrackerData()  ) );
            } else {
              streamlog_out ( ERROR4 ) << "Unknown pixel type.  Sorry for quitting." << endl;
              throw UnknownDataTypeException("Pixel type unknown");
            }

          } else {
            streamlog_out ( ERROR4 ) <<  "Unknown cluster type. Sorry for quitting" << endl;
            throw UnknownDataTypeException("Cluster type unknown");
          }
          int internalSensorID = pulseCellDecoder( internalPulse ) [ "sensorID" ] ;

          if ( internalSensorID != externalSensorID ) {

            float internalXCenter;
            float internalYCenter;

            // we catch the coordinates of the internal seed

            internalCluster->getCenterOfGravity( internalXCenter, internalYCenter ) ;

            streamlog_out ( DEBUG ) << "Filling histo " << externalSensorID << " " << internalSensorID << endl;


            // we input the coordinates in the correlation matrix, one
            // for each type of coordinate: X and Y

            _clusterXCorrelationMatrix[ externalSensorID ][ internalSensorID ]->
              fill( externalXCenter, internalXCenter );
            _clusterYCorrelationMatrix[ externalSensorID ][ internalSensorID ]->
              fill( externalYCenter, internalYCenter );

          } // endif

          delete internalCluster;

        } // internal loop

        delete externalCluster;
      } // external loop
    } // endif hasCluster



#ifdef USE_GEAR

    if ( _hasHitCollection ) {

      LCCollectionVec * inputHitCollection = static_cast< LCCollectionVec *>
        ( event->getCollection( _inputHitCollectionName )) ;

      for ( size_t iExt = 0 ; iExt < inputHitCollection->size(); ++iExt ) {

        // this is the external hit
        TrackerHitImpl * externalHit = static_cast< TrackerHitImpl * > ( inputHitCollection->
                                                                         getElementAt( iExt ) );

        int externalSensorID = guessSensorID( externalHit );

        double * externalPosition;
        externalPosition = (double *) externalHit->getPosition();

        for ( size_t iInt = 0; iInt < inputHitCollection->size(); ++iInt ) {

          TrackerHitImpl  * internalHit = static_cast< TrackerHitImpl * > ( inputHitCollection->
                                                                            getElementAt( iInt ) );

          int internalSensorID = guessSensorID( internalHit );

          if ( internalSensorID != externalSensorID ) {

            double * internalPosition;
            internalPosition = (double *) internalHit->getPosition(  );

            _hitXCorrelationMatrix[ externalSensorID ] [ internalSensorID ] ->
              fill ( externalPosition[0] , internalPosition[0] ) ;

            _hitYCorrelationMatrix[ externalSensorID ] [ internalSensorID ] ->
              fill( externalPosition[1], internalPosition[1] );

          }

        }

      }
    }

#endif // USE_GEAR


  } catch (DataNotAvailableException& e  ) {

    streamlog_out  ( WARNING2 ) <<  "No input collection found on event " << event->getEventNumber()
                                << " in run " << event->getRunNumber() << endl;
  }

#endif

}

void EUTelCorrelator::end() {

  streamlog_out ( MESSAGE4 )  << "Successfully finished" << endl;

}

void EUTelCorrelator::bookHistos() {

  if ( !_hasClusterCollection && !_hasHitCollection ) return ;

#if defined(USE_AIDA) || defined(MARLIN_USE_AIDA)

  try {

    streamlog_out ( MESSAGE4 ) <<  "Booking histograms" << endl;

    // create all the directories first
    vector< string > dirNames;

    if ( _hasClusterCollection ) {
      dirNames.push_back ("ClusterX");
      dirNames.push_back ("ClusterY");
    }

    if ( _hasHitCollection ) {
      dirNames.push_back ("HitX");
      dirNames.push_back ("HitY");
    }

    for ( size_t iPos = 0 ; iPos < dirNames.size() ; iPos++ ) {

      AIDAProcessor::tree(this)->mkdir( dirNames[iPos].c_str() ) ;

    }


    string tempHistoName;
    string tempHistoTitle ;


    for ( int row = 0 ; row < _noOfDetectors; ++row ) {

      vector< AIDA::IHistogram2D * > innerVectorXCluster;
      vector< AIDA::IHistogram2D * > innerVectorYCluster;

      map< unsigned int , AIDA::IHistogram2D * > innerMapXHit;
      map< unsigned int , AIDA::IHistogram2D * > innerMapYHit;


      for ( int col = 0 ; col < _noOfDetectors; ++col ) {

        if ( col != row ) {

          //we create histograms for X and Y Cluster correlation
          if ( _hasClusterCollection ) {
            {
              stringstream ss;
              ss << "ClusterX/" <<  _clusterXCorrelationHistoName << "_d" << row
                 << "_d" << col ;

              tempHistoName = ss.str();
            }

            streamlog_out( DEBUG ) << "Booking histo " << tempHistoName << endl;

            int     xBin = _maxX[ col ] - _minX[ col ] + 1;
            double  xMin = static_cast<double >(_minX[ col ]) - 0.5;
            double  xMax = static_cast<double >(_maxX[ col ]) + 0.5;
            int     yBin = _maxX[ row ] - _minX[ row ] + 1;
            double  yMin = static_cast<double >(_minX[ row ]) - 0.5;
            double  yMax = static_cast<double >(_maxX[ row ]) + 0.5;

            AIDA::IHistogram2D * histo2D =
              AIDAProcessor::histogramFactory(this)->createHistogram2D( tempHistoName.c_str(),
                                                                        xBin, xMin, xMax, yBin, yMin, yMax );

            {

              stringstream tt ;
              tt << "XClusterCorrelation" << "_d" << row
                 << "_d" << col ;
              tempHistoTitle = tt.str();

            }

            histo2D->setTitle( tempHistoTitle.c_str() );
            innerVectorXCluster.push_back ( histo2D );


            {
              stringstream ss;
              ss << "ClusterY/" <<  _clusterYCorrelationHistoName << "_d" << row
                 << "_d" << col ;

              tempHistoName = ss.str();
            }

            streamlog_out( DEBUG ) << "Booking histo " << tempHistoName << endl;

            xBin = _maxY[ col ] - _minY[ col ] + 1;
            xMin = static_cast<double >(_minY[ col ]) - 0.5;
            xMax = static_cast<double >(_maxY[ col ]) + 0.5;
            yBin = _maxY[ row ] - _minY[ row ] + 1;
            yMin = static_cast<double >(_minY[ row ]) - 0.5;
            yMax = static_cast<double >(_maxY[ row ]) + 0.5;


            histo2D =
              AIDAProcessor::histogramFactory(this)->createHistogram2D( tempHistoName.c_str(),
                                                                        xBin, xMin, xMax, yBin, yMin, yMax );
            {
              stringstream tt ;
              tt << "YClusterCorrelation" << "_d" << row
                 << "_d" << col ;
              tempHistoTitle = tt.str();
            }

            histo2D->setTitle( tempHistoTitle.c_str()) ;

            innerVectorYCluster.push_back ( histo2D );

          }


#ifdef USE_GEAR

          // the idea of using ICloud2D instead of H2D is interesting,
          // but because of a problem when the clouds is converted, I
          // prefer to use an histogram with a standard binning.
          //
          // the boundaries of the histos can be read from the GEAR
          // description and for safety multiplied by a safety factor
          // to take into account possible misalignment.

          if ( _hasHitCollection ) {

            double safetyFactor = 2.0; // 2 should be enough because it
            // means that the sensor is wrong
            // by all its size.
            double rowMin = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionX( row ) -
                                             ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeX ( row ) ));
            double rowMax = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionX( row ) +
                                             ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeX ( row )));

            double colMin = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionX( col ) -
                                             ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeX ( col )));
            double colMax = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionX( col ) +
                                             ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeX ( col )) );

            int colNBin = static_cast< int > ( safetyFactor ) * _siPlanesLayerLayout->getSensitiveNpixelX( col );
            int rowNBin = static_cast< int > ( safetyFactor ) * _siPlanesLayerLayout->getSensitiveNpixelX( row );

            {
              stringstream ss;
              ss << "HitX/" << _hitXCorrelationHistoName << "_d" << row << "_d" << col;
              tempHistoName = ss.str();

            }

            streamlog_out( DEBUG ) << "Booking histo " << tempHistoName << endl;

            string tempHistoTitle ;
            stringstream tt ;
            tt << "XHitCorrelation" << "_d" << row
               << "_d" << col ;
            tempHistoTitle = tt.str();

            AIDA::IHistogram2D * histo2D =
              AIDAProcessor::histogramFactory( this )->createHistogram2D( tempHistoName.c_str(), colNBin, colMin, colMax,
                                                                          rowNBin, rowMin, rowMax);
            histo2D->setTitle( tempHistoTitle.c_str() );

            innerMapXHit[ _siPlanesLayerLayout->getID( col ) ] =  histo2D ;

            // now the hit on the Y direction
            rowMin = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionY( row ) -
                                      ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeY ( row ) ));
            rowMax = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionY( row ) +
                                      ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeY ( row )));

            colMin = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionY( col ) -
                                      ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeY ( col )));
            colMax = safetyFactor * ( _siPlanesLayerLayout->getSensitivePositionY( col ) +
                                      ( 0.5 * _siPlanesLayerLayout->getSensitiveSizeY ( col )) );

            colNBin = static_cast< int > ( safetyFactor ) * _siPlanesLayerLayout->getSensitiveNpixelY( col );
            rowNBin = static_cast< int > ( safetyFactor ) * _siPlanesLayerLayout->getSensitiveNpixelY( row );

            {
              stringstream ss;
              ss << "HitY/" << _hitYCorrelationHistoName << "_d" << row << "_d" << col;
              tempHistoName = ss.str();

            }


            streamlog_out( DEBUG ) << "Booking cloud " << tempHistoName << endl;

            {
              stringstream tt ;
              tt << "YHitCorrelation" << "_d" << row
                 << "_d" << col ;
              tempHistoTitle = tt.str();
            }

            histo2D =
              AIDAProcessor::histogramFactory( this )->createHistogram2D( tempHistoName.c_str(), colNBin, colMin, colMax,
                                                                          rowNBin, rowMin, rowMax);
            histo2D->setTitle( tempHistoTitle.c_str() );

            innerMapXHit[ _siPlanesLayerLayout->getID( col ) ] =  histo2D ;

          }

#endif // USE_GEAR

        } else {

          if ( _hasClusterCollection ) {
            innerVectorXCluster.push_back( NULL );
            innerVectorYCluster.push_back( NULL );
          }

          if ( _hasHitCollection ) {
#ifdef USE_GEAR
            innerMapXHit[ _siPlanesLayerLayout->getID( col )  ] = NULL ;
            innerMapYHit[ _siPlanesLayerLayout->getID( col )  ] = NULL ;
#endif
          }

        }

      }

      if ( _hasClusterCollection ) {
        _clusterXCorrelationMatrix.push_back( innerVectorXCluster ) ;
        _clusterYCorrelationMatrix.push_back( innerVectorYCluster ) ;
      }

      if ( _hasHitCollection ) {
#ifdef USE_GEAR
        _hitXCorrelationMatrix[ _siPlanesLayerLayout->getID( row ) ] = innerMapXHit;
        _hitYCorrelationMatrix[ _siPlanesLayerLayout->getID( row ) ] = innerMapYHit;
#endif
      }
    }

  } catch (lcio::Exception& e ) {

    streamlog_out ( ERROR1 ) << "No AIDAProcessor initialized. Sorry for quitting..." << endl;
    exit( -1 );

  }
#endif
}

#ifdef USE_GEAR
int EUTelCorrelator::guessSensorID( TrackerHitImpl * hit ) {

  int sensorID = 0;
  double minDistance =  numeric_limits< double >::max() ;
  double * hitPosition = const_cast<double * > (hit->getPosition());

  for ( int iPlane = 0 ; iPlane < _siPlanesLayerLayout->getNLayers(); ++iPlane ) {
    double distance = std::abs( hitPosition[2] - _siPlaneZPosition[ iPlane ] );
    if ( distance < minDistance ) {
      minDistance = distance;
      sensorID = _siPlanesLayerLayout->getID( iPlane );
    }
  }


  return sensorID;
}
#endif // USE_GEAR