#include "eudaq/StdEventConverter.hh"
#include "eudaq/RawEvent.hh"

#ifdef _WIN32
//TODO remove the _WIN32 if not necessary in linux
#include <array>
#endif

//parameters to be later provided by a configuration file
//common
#define eventSizeLimit 1 //minimum size of the event which will be displayed

//ahcal
#define planeCount 3//number of ahcal layers
#define pedestalLimit 0 //minimum ahcal adc value, that will be displayed

//siwecal
#define planeCount_siecal 15//number of layers
//should it be read from the data ??--> int planeCount_siecal = rawev->GetTag("NSLBs",0);
//the problem is that the planes must be defined from the beginning, even if no data is received yet
#define pedestalLimit_siecal 0

#define bcid_and_sca_monitoring

#ifdef bcid_and_sca_monitoring 
//ahcal
#define planesXsize 4096
#define planesYsize 16
//siwecal
#define planesXsize_siecal 4096
#define planesYsize_siecal 15
#endif

#ifndef bcid_and_sca_monitoring 
//ahcal
#define planesXsize 12
#define planesYsize 12
//siwecal
#define planesXsize_siecal 32
#define planesYsize_siecal 32
#endif



class CaliceRawEvent2StdEventConverter: public eudaq::StdEventConverter {
public:
  bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
  static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceObject");

private:

  //**********************siwecal
  // mapping
  int isCOB_siecal(int slabadd) const;
  int getPlaneNumberFromSlabAdd_siecal(int slabadd) const;
  int getXcoordFromChipChannel_siecal(int slabadd, int chipid, int channelNr) const;
  int getYcoordFromChipChannel_siecal(int slabadd, int chipid, int channelNr) const;
  //decoding
  void SiECALConverting(eudaq::EventSPC d1, std::vector<int> &SLABHits, std::vector<std::array<int, planesXsize_siecal * planesYsize_siecal>> &SLABs) const;
    
  //********************ahcal
  //mapping
  int getPlaneNumberFromCHIPID(int chipid) const;
  int getXcoordFromChipChannel(int chipid, int channelNr) const;
  int getYcoordFromChipChannel(int chipid, int channelNr) const;
  //decoding
  void ScCALConverting(eudaq::EventSPC d1, std::vector<int> &HBUHits, std::vector<std::array<int, planesXsize * planesYsize>> &HBUs) const;
    
  std::string sensortype = "CALICE (X=x, Y=y)";
  std::string sensortype2 = "CALICE (X=BCID, Y-SCA)";

  //void fillPlanesSiECAL( eudaq::EventSPC d1, eudaq::StandardEvent *result) const;
  //void fillPlanesScCAL( eudaq::EventSPC d1,eudaq::StandardEvent *result) const;


  const std::map<int, int> layerOrder = { //{module,layer}
					 { 1, 0 }, { 2, 1 }, { 3, 2 }
					 //         { 4, 3 }, { 5, 4 }
  };

  const std::map<int, std::tuple<int, int>> mapping = { //chipid to tuple: layer, xcoordinate, ycoordinate
						       //layer 1: single HBU
						       { 0, std::make_tuple(6, 6) },
						       { 1, std::make_tuple(6, 0) },
						       { 2, std::make_tuple(0, 6) },
						       { 3, std::make_tuple(0, 0) }
						       //                  { 185, std::make_tuple(0, 18, 18) },
						       //                  { 186, std::make_tuple(0, 18, 12) },
						       //                  { 187, std::make_tuple(0, 12, 18) },
						       //                   { 188, std::make_tuple(0, 12, 12) },
						       //layer 10: former layer 12 - full HBU
						       //shell script for big layer:
						       //chip0=129 ; layer=1 ; for i in `seq 0 15` ; do echo "{"`expr ${i} + ${chip0}`", std::make_tuple("${layer}", "`expr 18 - \( ${i} / 8 \) \* 12 - \( ${i} / 2 \) \% 2 \* 6`", "`expr 18 - \( ${i} \% 8 \) / 4 \* 12 - ${i} \% 2 \* 6`") }," ; done

						       //                  { 0, std::make_tuple(18, 18) },
						       //                  { 1, std::make_tuple(18, 12) },
						       //                  { 2, std::make_tuple(12, 18) },
						       //                  { 3, std::make_tuple(12, 12) },
						       //                  { 4, std::make_tuple(18, 6) },
						       //                  { 5, std::make_tuple(18, 0) },
						       //                  { 6, std::make_tuple(12, 6) },
						       //                  { 7, std::make_tuple(12, 0) },
						       //                  { 8, std::make_tuple(6, 18) },
						       //                  { 9, std::make_tuple(6, 12) },
						       //                  { 10, std::make_tuple(0, 18) },
						       //                  { 11, std::make_tuple(0, 12) },
						       //                  { 12, std::make_tuple(6, 6) },
						       //                  { 13, std::make_tuple(6, 0) },
						       //                  { 14, std::make_tuple(0, 6) },
						       //                  { 15, std::make_tuple(0, 0) }
  };

  //siwecal mapping
  std::vector<int> cobs={5,6};//where are the cobs located?
  //## TYPE: fev11_cob_rotate FLIPX: 1 FLIPY: 1
  //#chip channel I J
  const int siwecal_I_cob[16][64]={
{29,30,28,31,31,30,29,31,30,31,31,29,30,31,31,30,29,28,28,28,30,29,31,28,29,27,27,27,27,27,30,30,26,29,28,27,26,29,28,26,26,26,26,28,27,25,27,24,25,24,26,24,26,24,25,24,25,24,25,24,25,24,25,25}, 
{25,25,25,24,25,24,24,25,24,25,24,26,24,26,24,25,24,27,25,28,27,26,26,26,26,29,28,27,26,29,30,30,28,27,26,27,27,27,27,28,29,29,31,28,30,28,28,29,30,31,31,30,29,31,31,30,31,30,31,29,29,31,28,30}, 
{22,22,22,22,22,23,22,23,23,23,23,21,22,23,23,22,23,21,21,21,21,21,21,21,20,20,20,20,20,20,19,20,20,19,19,19,19,19,19,19,18,18,18,18,18,18,18,17,18,16,16,16,17,16,16,16,16,16,17,17,17,17,17,17}, 
{17,17,17,17,17,17,16,16,16,16,17,16,16,16,16,18,17,18,18,18,18,18,18,19,18,19,19,19,19,19,19,20,19,20,20,20,20,20,20,21,20,21,21,21,21,21,21,23,22,23,23,23,21,22,23,23,23,23,22,22,22,22,22,22}, 
{14,14,14,14,14,15,14,15,15,15,15,13,14,15,15,14,15,13,13,13,13,13,13,13,12,12,12,12,12,12,11,12,12,11,11,11,11,11,11,11,10,10,10,10,10,10,10,9,10,8,8,8,9,8,8,8,8,8,9,9,9,9,9,9}, 
{9,9,9,9,9,9,8,8,8,8,9,8,8,8,8,10,9,10,10,10,10,10,10,11,10,11,11,11,11,11,11,12,11,12,12,12,12,12,12,13,12,13,13,13,13,13,13,15,14,15,15,15,13,14,15,15,15,15,14,14,14,14,14,14}, 
{6,6,6,6,6,6,7,5,7,5,7,6,7,7,7,7,7,6,4,5,4,5,5,5,3,5,5,4,2,3,2,4,4,3,4,4,2,4,3,1,0,3,0,3,1,3,3,0,2,0,1,0,2,1,0,0,1,0,2,1,2,1,1,2}, 
{1,2,1,0,1,2,2,1,0,0,2,1,0,1,0,2,0,3,3,3,1,3,0,1,0,4,3,4,2,3,2,4,4,3,4,4,2,5,5,5,3,5,5,5,4,4,6,7,7,7,7,7,6,7,5,7,5,7,6,6,6,6,6,6}, 
{30,29,30,31,30,29,29,30,31,31,29,30,31,30,31,29,31,28,28,28,30,28,31,30,31,27,28,27,29,28,29,27,27,28,27,27,29,26,26,26,28,26,26,26,27,27,25,24,24,24,24,24,25,24,26,24,26,24,25,25,25,25,25,25}, 
{25,25,25,25,25,25,24,26,24,26,24,25,24,24,24,24,24,25,27,26,27,26,26,26,28,26,26,27,29,28,29,27,27,28,27,27,29,27,28,30,31,28,31,28,30,28,28,31,29,31,30,31,29,30,31,31,30,31,29,30,29,30,30,29}, 
{22,22,22,22,22,22,23,23,23,23,22,23,23,23,23,21,22,21,21,21,21,21,21,20,21,20,20,20,20,20,20,19,20,19,19,19,19,19,19,18,19,18,18,18,18,18,18,16,17,16,16,16,18,17,16,16,16,16,17,17,17,17,17,17}, 
{17,17,17,17,17,16,17,16,16,16,16,18,17,16,16,17,16,18,18,18,18,18,18,18,19,19,19,19,19,19,20,19,19,20,20,20,20,20,20,20,21,21,21,21,21,21,21,22,21,23,23,23,22,23,23,23,23,23,22,22,22,22,22,22}, 
{14,14,14,14,14,14,15,15,15,15,14,15,15,15,15,13,14,13,13,13,13,13,13,12,13,12,12,12,12,12,12,11,12,11,11,11,11,11,11,10,11,10,10,10,10,10,10,8,9,8,8,8,10,9,8,8,8,8,9,9,9,9,9,9}, 
{9,9,9,9,9,8,9,8,8,8,8,10,9,8,8,9,8,10,10,10,10,10,10,10,11,11,11,11,11,11,12,11,11,12,12,12,12,12,12,12,13,13,13,13,13,13,13,14,13,15,15,15,14,15,15,15,15,15,14,14,14,14,14,14}, 
{6,6,6,7,6,7,7,6,7,6,7,5,7,5,7,6,7,4,6,3,4,5,5,5,5,2,3,4,5,2,1,1,3,4,5,4,4,4,4,3,2,2,0,3,1,3,3,2,1,0,0,1,2,0,0,1,0,1,0,2,2,0,3,1}, 
{2,1,3,0,0,1,2,0,1,0,0,2,1,0,0,1,2,3,3,3,1,2,0,3,2,4,4,4,4,4,1,1,5,2,3,4,5,2,3,5,5,5,5,3,4,6,4,7,6,7,5,7,5,7,6,7,6,7,6,7,6,7,6,6} 
};
  const int siwecal_J_cob[16][64]={
{28,27,28,25,26,28,29,27,29,28,29,30,30,30,31,31,31,30,31,29,26,27,24,27,26,30,31,29,28,27,25,24,31,25,26,26,30,24,25,29,28,27,26,24,25,30,24,31,31,25,25,30,24,26,28,27,29,28,27,29,26,24,25,24}, 
{22,23,21,23,20,18,19,18,20,19,21,23,17,22,22,16,16,23,17,23,22,20,21,18,19,23,22,21,17,22,22,23,21,20,16,18,19,17,16,20,21,20,23,18,21,16,17,16,16,16,17,17,17,18,19,18,20,19,21,18,19,22,19,20}, 
{24,25,26,27,28,24,29,25,26,27,28,30,30,29,30,31,31,31,27,29,28,25,24,26,31,30,24,26,25,28,24,27,29,25,31,26,27,30,29,28,24,25,26,27,28,29,30,31,31,31,30,29,30,28,27,26,25,24,24,29,25,27,28,26}, 
{23,22,21,20,18,19,23,22,21,20,17,19,18,17,16,16,16,17,18,20,19,22,21,19,23,17,18,21,20,22,23,20,16,19,18,21,22,17,23,23,16,22,21,19,18,20,16,16,16,17,18,19,17,17,20,21,22,23,23,18,19,20,21,22}, 
{24,25,26,27,28,24,29,25,26,27,28,30,30,29,30,31,31,31,27,29,28,25,24,26,31,30,24,26,25,28,24,27,29,25,31,26,27,30,29,28,24,25,26,27,28,29,30,31,31,31,30,29,30,28,27,26,25,24,24,29,25,27,28,26}, 
{23,22,21,20,18,19,23,22,21,20,17,19,18,17,16,16,16,17,18,20,19,22,21,19,23,17,18,21,20,22,23,20,16,19,18,21,22,17,23,23,16,22,21,19,18,20,16,16,16,17,18,19,17,17,20,21,22,23,23,18,19,20,21,22}, 
{26,25,27,28,29,24,29,24,28,25,30,30,27,26,25,24,31,31,24,26,25,27,28,29,24,30,31,26,24,25,25,27,31,26,28,29,26,30,27,24,31,28,24,29,31,31,30,30,31,29,30,28,30,29,27,26,28,25,29,27,28,26,25,27}, 
{22,20,21,22,20,18,19,19,21,20,17,18,19,17,18,16,17,17,16,18,16,19,23,23,16,17,20,18,21,21,22,20,19,22,16,21,23,17,16,18,23,20,19,21,22,23,16,16,23,22,21,20,17,17,22,19,23,18,18,23,19,20,21,22}, 
{9,11,10,9,11,13,12,12,10,11,14,13,12,14,13,15,14,14,15,13,15,12,8,8,15,14,11,13,10,10,9,11,12,9,15,10,8,14,15,13,8,11,12,10,9,8,15,15,8,9,10,11,14,14,9,12,8,13,13,8,12,11,10,9}, 
{5,6,4,3,2,7,2,7,3,6,1,1,4,5,6,7,0,0,7,5,6,4,3,2,7,1,0,5,7,6,6,4,0,5,3,2,5,1,4,7,0,3,7,2,0,0,1,1,0,2,1,3,1,2,4,5,3,6,2,4,3,5,6,4}, 
{8,9,10,11,13,12,8,9,10,11,14,12,13,14,15,15,15,14,13,11,12,9,10,12,8,14,13,10,11,9,8,11,15,12,13,10,9,14,8,8,15,9,10,12,13,11,15,15,15,14,13,12,14,14,11,10,9,8,8,13,12,11,10,9}, 
{7,6,5,4,3,7,2,6,5,4,3,1,1,2,1,0,0,0,4,2,3,6,7,5,0,1,7,5,6,3,7,4,2,6,0,5,4,1,2,3,7,6,5,4,3,2,1,0,0,0,1,2,1,3,4,5,6,7,7,2,6,4,3,5}, 
{8,9,10,11,13,12,8,9,10,11,14,12,13,14,15,15,15,14,13,11,12,9,10,12,8,14,13,10,11,9,8,11,15,12,13,10,9,14,8,8,15,9,10,12,13,11,15,15,15,14,13,12,14,14,11,10,9,8,8,13,12,11,10,9}, 
{7,6,5,4,3,7,2,6,5,4,3,1,1,2,1,0,0,0,4,2,3,6,7,5,0,1,7,5,6,3,7,4,2,6,0,5,4,1,2,3,7,6,5,4,3,2,1,0,0,0,1,2,1,3,4,5,6,7,7,2,6,4,3,5}, 
{9,8,10,8,11,13,12,13,11,12,10,8,14,9,9,15,15,8,14,8,9,11,10,13,12,8,9,10,14,9,9,8,10,11,15,13,12,14,15,11,10,11,8,13,10,15,14,15,15,15,14,14,14,13,12,13,11,12,10,13,12,9,12,11}, 
{3,4,3,6,5,3,2,4,2,3,2,1,1,1,0,0,0,1,0,2,5,4,7,4,5,1,0,2,3,4,6,7,0,6,5,5,1,7,6,2,3,4,5,7,6,1,7,0,0,6,6,1,7,5,3,4,2,3,4,2,5,7,6,7} 
};

//## TYPE: fev10 FLIPX: 1 FLIPY: 1
//#chip channel I J
const int siwecal_I_bga[16][64]={
{31,31,31,30,29,30,31,30,30,31,31,29,30,29,30,29,31,29,29,29,28,31,30,28,30,28,29,28,28,28,28,27,27,27,28,26,27,27,25,24,27,26,26,27,25,26,27,24,26,26,25,24,26,24,25,25,24,24,25,26,24,24,25,25}, 
{24,24,24,25,26,25,24,25,25,24,24,26,25,26,25,26,24,26,26,26,27,24,25,27,25,27,26,27,27,27,27,28,28,28,27,29,28,28,30,31,28,29,29,28,30,29,28,31,29,29,30,31,29,31,30,30,31,31,30,29,31,31,30,30}, 
{23,23,23,22,21,22,23,22,22,23,23,21,22,21,22,21,23,21,21,21,20,23,22,20,22,20,21,20,20,20,20,19,19,19,20,18,19,19,17,16,19,18,18,19,17,18,19,16,18,18,17,16,18,16,17,17,16,16,17,18,16,16,17,17}, 
{16,16,16,17,18,17,16,17,17,16,16,18,17,18,17,18,16,18,18,18,19,16,17,19,17,19,18,19,19,19,19,20,20,20,19,21,20,20,22,23,20,21,21,20,22,21,20,23,21,21,22,23,21,23,22,22,23,23,22,21,23,23,22,22}, 
{15,15,15,14,13,14,15,14,14,15,15,13,14,13,14,13,15,13,13,13,12,15,14,12,14,12,13,12,12,12,12,11,11,11,12,10,11,11,9,8,11,10,10,11,9,10,11,8,10,10,9,8,10,8,9,9,8,8,9,10,8,8,9,9}, 
{8,8,8,9,10,9,8,9,9,8,8,10,9,10,9,10,8,10,10,10,11,8,9,11,9,11,10,11,11,11,11,12,12,12,11,13,12,12,14,15,12,13,13,12,14,13,12,15,13,13,14,15,13,15,14,14,15,15,14,13,15,15,14,14}, 
{7,7,7,6,5,6,7,6,6,7,7,5,6,5,6,5,7,5,5,5,4,7,6,4,6,4,5,4,4,4,4,3,3,3,4,2,3,3,1,0,3,2,2,3,1,2,3,0,2,2,1,0,2,0,1,1,0,0,1,2,0,0,1,1}, 
{0,0,0,1,2,1,0,1,1,0,0,2,1,2,1,2,0,2,2,2,3,0,1,3,1,3,2,3,3,3,3,4,4,4,3,5,4,4,6,7,4,5,5,4,6,5,4,7,5,5,6,7,5,7,6,6,7,7,6,5,7,7,6,6}, 
{31,31,31,30,29,30,31,30,30,31,31,29,30,29,30,29,31,29,29,29,28,31,30,28,30,28,29,28,28,28,28,27,27,27,28,26,27,27,25,24,27,26,26,27,25,26,27,24,26,26,25,24,26,24,25,25,24,24,25,26,24,24,25,25}, 
{24,24,24,25,26,25,24,25,25,24,24,26,25,26,25,26,24,26,26,26,27,24,25,27,25,27,26,27,27,27,27,28,28,28,27,29,28,28,30,31,28,29,29,28,30,29,28,31,29,29,30,31,29,31,30,30,31,31,30,29,31,31,30,30}, 
{23,23,23,22,21,22,23,22,22,23,23,21,22,21,22,21,23,21,21,21,20,23,22,20,22,20,21,20,20,20,20,19,19,19,20,18,19,19,17,16,19,18,18,19,17,18,19,16,18,18,17,16,18,16,17,17,16,16,17,18,16,16,17,17}, 
{16,16,16,17,18,17,16,17,17,16,16,18,17,18,17,18,16,18,18,18,19,16,17,19,17,19,18,19,19,19,19,20,20,20,19,21,20,20,22,23,20,21,21,20,22,21,20,23,21,21,22,23,21,23,22,22,23,23,22,21,23,23,22,22}, 
{15,15,15,14,13,14,15,14,14,15,15,13,14,13,14,13,15,13,13,13,12,15,14,12,14,12,13,12,12,12,12,11,11,11,12,10,11,11,9,8,11,10,10,11,9,10,11,8,10,10,9,8,10,8,9,9,8,8,9,10,8,8,9,9}, 
{8,8,8,9,10,9,8,9,9,8,8,10,9,10,9,10,8,10,10,10,11,8,9,11,9,11,10,11,11,11,11,12,12,12,11,13,12,12,14,15,12,13,13,12,14,13,12,15,13,13,14,15,13,15,14,14,15,15,14,13,15,15,14,14}, 
{7,7,7,6,5,6,7,6,6,7,7,5,6,5,6,5,7,5,5,5,4,7,6,4,6,4,5,4,4,4,4,3,3,3,4,2,3,3,1,0,3,2,2,3,1,2,3,0,2,2,1,0,2,0,1,1,0,0,1,2,0,0,1,1}, 
{0,0,0,1,2,1,0,1,1,0,0,2,1,2,1,2,0,2,2,2,3,0,1,3,1,3,2,3,3,3,3,4,4,4,3,5,4,4,6,7,4,5,5,4,6,5,4,7,5,5,6,7,5,7,6,6,7,7,6,5,7,7,6,6} 
};
  const int siwecal_J_bga[16][64]={
{24,25,26,29,27,27,27,31,26,29,30,26,28,30,24,29,31,25,28,31,27,28,30,26,25,31,24,29,30,25,28,31,30,29,24,31,28,26,31,28,27,30,27,25,30,29,24,31,28,26,29,25,25,30,28,25,27,26,27,24,29,24,24,26}, 
{23,22,21,18,20,20,20,16,21,18,17,21,19,17,23,18,16,22,19,16,20,19,17,21,22,16,23,18,17,22,19,16,17,18,23,16,19,21,16,19,20,17,20,22,17,18,23,16,19,21,18,22,22,17,19,22,20,21,20,23,18,23,23,21}, 
{24,25,26,29,27,27,27,31,26,29,30,26,28,30,24,29,31,25,28,31,27,28,30,26,25,31,24,29,30,25,28,31,30,29,24,31,28,26,31,28,27,30,27,25,30,29,24,31,28,26,29,25,25,30,28,25,27,26,27,24,29,24,24,26}, 
{23,22,21,18,20,20,20,16,21,18,17,21,19,17,23,18,16,22,19,16,20,19,17,21,22,16,23,18,17,22,19,16,17,18,23,16,19,21,16,19,20,17,20,22,17,18,23,16,19,21,18,22,22,17,19,22,20,21,20,23,18,23,23,21}, 
{24,25,26,29,27,27,27,31,26,29,30,26,28,30,24,29,31,25,28,31,27,28,30,26,25,31,24,29,30,25,28,31,30,29,24,31,28,26,31,28,27,30,27,25,30,29,24,31,28,26,29,25,25,30,28,25,27,26,27,24,29,24,24,26}, 
{23,22,21,18,20,20,20,16,21,18,17,21,19,17,23,18,16,22,19,16,20,19,17,21,22,16,23,18,17,22,19,16,17,18,23,16,19,21,16,19,20,17,20,22,17,18,23,16,19,21,18,22,22,17,19,22,20,21,20,23,18,23,23,21}, 
{24,25,26,29,27,27,27,31,26,29,30,26,28,30,24,29,31,25,28,31,27,28,30,26,25,31,24,29,30,25,28,31,30,29,24,31,28,26,31,28,27,30,27,25,30,29,24,31,28,26,29,25,25,30,28,25,27,26,27,24,29,24,24,26}, 
{23,22,21,18,20,20,20,16,21,18,17,21,19,17,23,18,16,22,19,16,20,19,17,21,22,16,23,18,17,22,19,16,17,18,23,16,19,21,16,19,20,17,20,22,17,18,23,16,19,21,18,22,22,17,19,22,20,21,20,23,18,23,23,21}, 
{8,9,10,13,11,11,11,15,10,13,14,10,12,14,8,13,15,9,12,15,11,12,14,10,9,15,8,13,14,9,12,15,14,13,8,15,12,10,15,12,11,14,11,9,14,13,8,15,12,10,13,9,9,14,12,9,11,10,11,8,13,8,8,10}, 
{7,6,5,2,4,4,4,0,5,2,1,5,3,1,7,2,0,6,3,0,4,3,1,5,6,0,7,2,1,6,3,0,1,2,7,0,3,5,0,3,4,1,4,6,1,2,7,0,3,5,2,6,6,1,3,6,4,5,4,7,2,7,7,5}, 
{8,9,10,13,11,11,11,15,10,13,14,10,12,14,8,13,15,9,12,15,11,12,14,10,9,15,8,13,14,9,12,15,14,13,8,15,12,10,15,12,11,14,11,9,14,13,8,15,12,10,13,9,9,14,12,9,11,10,11,8,13,8,8,10}, 
{7,6,5,2,4,4,4,0,5,2,1,5,3,1,7,2,0,6,3,0,4,3,1,5,6,0,7,2,1,6,3,0,1,2,7,0,3,5,0,3,4,1,4,6,1,2,7,0,3,5,2,6,6,1,3,6,4,5,4,7,2,7,7,5}, 
{8,9,10,13,11,11,11,15,10,13,14,10,12,14,8,13,15,9,12,15,11,12,14,10,9,15,8,13,14,9,12,15,14,13,8,15,12,10,15,12,11,14,11,9,14,13,8,15,12,10,13,9,9,14,12,9,11,10,11,8,13,8,8,10}, 
{7,6,5,2,4,4,4,0,5,2,1,5,3,1,7,2,0,6,3,0,4,3,1,5,6,0,7,2,1,6,3,0,1,2,7,0,3,5,0,3,4,1,4,6,1,2,7,0,3,5,2,6,6,1,3,6,4,5,4,7,2,7,7,5}, 
{8,9,10,13,11,11,11,15,10,13,14,10,12,14,8,13,15,9,12,15,11,12,14,10,9,15,8,13,14,9,12,15,14,13,8,15,12,10,15,12,11,14,11,9,14,13,8,15,12,10,13,9,9,14,12,9,11,10,11,8,13,8,8,10}, 
{7,6,5,2,4,4,4,0,5,2,1,5,3,1,7,2,0,6,3,0,4,3,1,5,6,0,7,2,1,6,3,0,1,2,7,0,3,5,0,3,4,1,4,6,1,2,7,0,3,5,2,6,6,1,3,6,4,5,4,7,2,7,7,5} 
};



};

namespace {
  auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<CaliceRawEvent2StdEventConverter>(CaliceRawEvent2StdEventConverter::m_id_factory);
}

bool CaliceRawEvent2StdEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const {


  //  eudaq::StandardEvent& result = *(d2.get());
  //auto result = std::dynamic_pointer_cast<const eudaq::StandardEvent>(d2);
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);

  auto bl0 = ev->GetBlock(0);
  std::string colName((char *) &bl0.front(), bl0.size());

  std::vector<int> HBUHits;
  std::vector<std::array<int, planesXsize * planesYsize>> HBUs;         //HBU(aka plane) index, x*12+y
  for (int i = 0; i < planeCount; ++i) {
    std::array<int, planesXsize * planesYsize> HBU;
    HBU.fill(-1); //fill all channels to -1
    HBUs.push_back(HBU); //add the HBU to the HBU
    HBUHits.push_back(0);
  }
  
  //siecal
  std::vector<int> SLABHits;
  std::vector<std::array<int, planesXsize_siecal * planesYsize_siecal>> SLABs;  
  for (int i = 0; i < planeCount_siecal; ++i) {
    std::array<int, planesXsize_siecal * planesYsize_siecal> SLAB;
    SLAB.fill(-1); //fill all channels to -1
    SLABs.push_back(SLAB); //add the SLAB to the SLABs
    SLABHits.push_back(0);
  }
  
  if (colName == "EUDAQDataScCAL") {
    ScCALConverting(d1,HBUHits,HBUs);
  }
  
  if (colName == "EUDAQDataSiECAL") {
    SiECALConverting(d1,SLABHits,SLABs);
  }

  for (int i = 0; i <  SLABs.size(); ++i) {
    //for (int i = 0; i <  SLABs.size(); ++i) {

    std::string sensor=sensortype;
    if(planesXsize_siecal>1000) sensor=sensortype2;
    std::unique_ptr<eudaq::StandardPlane> plane(new eudaq::StandardPlane(i, "CaliceObject", sensor));
    int pixindex = 0;
    plane->SetSizeZS(planesXsize_siecal, planesYsize_siecal, SLABHits[i], 1, 0);
    
    for (int x = 0; x < planesXsize_siecal; x++) {
      for (int y = 0; y < planesYsize_siecal; y++) {
	//	    std::cout<<"y:"<<y<<" "<<planesYsize_siecal<<std::endl;
	if (SLABs[i][x * planesYsize_siecal + y] > 0 ) {
	  plane->SetPixel(pixindex++, x, y, SLABs[i][x * planesYsize_siecal + y]);
	  
	}
      }
    }
    
    d2->AddPlane(*plane);
  }

  for (int i = 0; i <  HBUs.size(); ++i) {
    //for (int i = 0; i <  SLABs.size(); ++i) {

    std::string sensor=sensortype;
    if(planesXsize_siecal>1000) sensor=sensortype2;
    std::unique_ptr<eudaq::StandardPlane> plane(new eudaq::StandardPlane(i+planeCount_siecal, "CaliceObject", sensor));
    int pixindex = 0;
    plane->SetSizeZS(planesXsize, planesYsize, HBUHits[i], 1, 0);
    for (int x = 0; x < planesXsize; x++) {
      for (int y = 0; y < planesYsize; y++) {
	if (HBUs[i][x * planesYsize + y] >= 0 ) {
	  plane->SetPixel(pixindex++, x, y, HBUs[i][x * planesYsize + y]);
	}
      }
    }
   
    d2->AddPlane(*plane);
  }

  return true;


}

//************ AHCAL
int CaliceRawEvent2StdEventConverter::getPlaneNumberFromCHIPID(int chipid) const {
  //return (chipid >> 8);
  int module = (chipid >> 8);
  //   std::cout << " CHIP  " << chipid << " is in Module " << module << std::endl;
  auto searchIterator = layerOrder.find(module);
  if (searchIterator == layerOrder.end()) {
    std::cout << "Module " << module << " is not in mapping";
    return -1;
  }
  auto Layer = searchIterator->second;
  return Layer;
  //   return result;
}

int CaliceRawEvent2StdEventConverter::getXcoordFromChipChannel(int chipid, int channelNr) const {
  auto searchIterator = mapping.find(chipid & 0x0f);
  if (searchIterator == mapping.end()) return 0;
  auto asicXCoordBase = std::get<0>(searchIterator->second);

  int subx = channelNr / 6 + asicXCoordBase;
  return subx;
  //   if (((chipid & 0x03) == 0x01) || ((chipid & 0x03) == 0x02)) {
  //      //1st and 2nd spiroc are in the righ half of HBU
  //      subx += 6;
  //   }
  //   return subx;
}

int CaliceRawEvent2StdEventConverter::getYcoordFromChipChannel(int chipid, int channelNr) const {
  auto searchIterator = mapping.find(chipid & 0x0f);
  if (searchIterator == mapping.end()) return 0;
  auto asicYCoordBase = std::get<1>(searchIterator->second);

  int suby = channelNr % 6;
  if (chipid & 0x02) {
    //3rd and 4th spiroc have different channel order
    suby = 5 - suby;
  }
  //   if (((chipid & 0x03) == 0x00) || ((chipid & 0x03) == 0x03)) {
  //      //3rd and 4th spiroc have different channel order
  //      suby = 5 - suby;
  //   }
  suby += asicYCoordBase;
  //   if (((chipid & 0x03) == 0x01) || ((chipid & 0x03) == 0x03)) {
  //      suby += 6;
  //   }
  return suby;
}

void CaliceRawEvent2StdEventConverter::ScCALConverting(eudaq::EventSPC d1, std::vector<int> &HBUHits, std::vector<std::array<int, planesXsize * planesYsize>> &HBUs) const{

    auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
    size_t nblocks = ev->NumBlocks();


    unsigned int nblock = 10; // the first 10 blocks contain other information
    // std::cout << ev->GetEventNumber() << "<" << std::flush;

    while ((nblock < ev->NumBlocks())&(nblocks > 7 + eventSizeLimit)) {         //iterate over all asic packets from (hopefully) same BXID
      std::vector<int> data;
      const auto & bl = ev->GetBlock(nblock++);
      data.resize(bl.size() / sizeof(int));
      memcpy(&data[0], &bl[0], bl.size());
      if (data.size() != 77) std::cout << "vector has size : " << bl.size() << "\tdata : " << data.size() << std::endl;
      int dummy=ev->GetTag("Dummy", 0);
      //data structure of packet: data[i]=
      //i=0 --> cycleNr
      //i=1 --> bunch crossing id
      //i=2 --> memcell or EvtNr (same thing, different naming)
      //i=3 --> ChipId
      //i=4 --> Nchannels per chip (normally 36)
      //i=5 to NC+4 -->  14 bits that contains TDC and hit/gainbit
      //i=NC+5 to NC+NC+4  -->  14 bits that contains ADC and again a copy of the hit/gainbit
      //debug prints:
      //std:cout << "Data_" << data[0] << "_" << data[1] << "_" << data[2] << "_" << data[3] << "_" << data[4] << "_" << data[5] << std::endl;
      //      if (data[1] == 0) continue; //don't store dummy trigger
      if(dummy==1) continue;//don't store dummy trigger
      int chipid = data[3];
      int planeNumber = planeCount_siecal+getPlaneNumberFromCHIPID(chipid);
      //printf("ChipID %04x: plane=%d\n", chipid, planeNumber);
      int bcid = data[1];
      int sca = data[2];

      if (planeNumber >= 0) {
	//if (HBUs[planeNumber][coordIndex] >= 0) // std::cout << "ERROR: channel already has a value" << std::endl;
	//else {
	if(planesXsize_siecal>1000) {
	  int coorx=bcid;
	  int coory= sca;
	  int coordIndex = coorx * planesYsize + coory;
	  if (HBUs[planeNumber][coordIndex] <0) {
	    HBUs[planeNumber][coordIndex] = 1;
	    if (HBUs[planeNumber][coordIndex] < 0) HBUs[planeNumber][coordIndex] = 0;
	    HBUHits[planeNumber]++;
	  }
	} else {
	  for (int ichan = 0; ichan < data[4]; ichan++) {
	    int adc = data[5 + data[4] + ichan] & 0x0FFF; // extract adc
	    int gainbit = (data[5 + data[4] + ichan] & 0x2000) ? 1 : 0; //extract gainbit
	    int hitbit = (data[5 + data[4] + ichan] & 0x1000) ? 1 : 0;  //extract hitbit
	    if (hitbit) {
	      if (adc < pedestalLimit) continue;
	      //get the index from the HBU array
	      //standart view: 1st hbu in upper right corner, asics facing to the viewer, tiles in the back. Dit upper right corner:
	      int coorx = getXcoordFromChipChannel(chipid, ichan);
	      int coory = getYcoordFromChipChannel(chipid, ichan);
	      //testbeam view: side slab in the bottom, electronics facing beam line:
	      //int coory = getXcoordFromChipChannel(chipid, ichan);
	      //int coorx = planesYsize - getYcoordFromChipChannel(chipid, ichan) - 1;
	      
	      int coordIndex = coorx * planesXsize + coory;
	      if (HBUs[planeNumber][coordIndex] >= 0) std::cout << "ERROR: channel already has a value" << std::endl;
	      HBUs[planeNumber][coordIndex] = gainbit ? adc : 10 * adc;
	      //HBUs[planeNumber][coordIndex] = 1;
	      if (HBUs[planeNumber][coordIndex] < 0) HBUs[planeNumber][coordIndex] = 0;
	      HBUHits[planeNumber]++;
	    }
	  }
	}
      }//if plane number>=0
    }//while
    
}
//SIWECAL *************************************************

int CaliceRawEvent2StdEventConverter::isCOB_siecal(int slabadd) const {
  for(int i=0; i<cobs.size(); i++) if(cobs.at(i)==slabadd) return 1;
  return 0;
}


int CaliceRawEvent2StdEventConverter::getPlaneNumberFromSlabAdd_siecal(int slabadd) const {
  return slabadd;
}

int CaliceRawEvent2StdEventConverter::getXcoordFromChipChannel_siecal(int slabadd, int chipid, int channelNr) const {
  if(isCOB_siecal(slabadd) )
    return siwecal_I_cob[chipid][channelNr];
  else
    return siwecal_I_bga[chipid][channelNr];
}


int CaliceRawEvent2StdEventConverter::getYcoordFromChipChannel_siecal(int slabadd, int chipid, int channelNr) const {
  if(isCOB_siecal(slabadd) )
    return siwecal_J_cob[chipid][channelNr];
  else
    return siwecal_J_bga[chipid][channelNr];
}


void CaliceRawEvent2StdEventConverter::SiECALConverting(eudaq::EventSPC d1, std::vector<int> &SLABHits, std::vector<std::array<int, planesXsize_siecal * planesYsize_siecal>> &SLABs) const{
  
    //    std::cout<<colName<<std::endl;
    
    auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
    size_t nblocks = ev->NumBlocks();

    //std::cout<<nblocks<<std::endl;

 
    unsigned int nblock = 4+planeCount_siecal; // the first 10 blocks contain other information
    //std::cout<<nblock<<std::endl;

    while (nblock < ev->NumBlocks() ) {         //iterate over all asic packets from (hopefully) same BXID
      std::vector<int> data;
      const auto & bl = ev->GetBlock(nblock);
      nblock++;
      data.resize(bl.size() / sizeof(int));
      memcpy(&data[0], &bl[0], bl.size());
      //data structure of packet: data[i]=
      /*cycledata.push_back((int) (cycleID));
      cycledata.push_back((int) (bcid[sca]));
      cycledata.push_back((int) (sca));
      cycledata.push_back((int) (slabAdd));
      cycledata.push_back((int) (skirocIndex));
      cycledata.push_back((int) (NB_OF_CHANNELS_IN_SKIROC));
      for(channel = 0; channel < NB_OF_CHANNELS_IN_SKIROC; channel++) {
	cycledata.push_back((int) (chargevalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	cycledata.push_back((int) (hitvalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	cycledata.push_back((int) (gainvalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	cycledata.push_back((int) (chargevalue[1][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	cycledata.push_back((int) (hitvalue[1][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	cycledata.push_back((int) (gainvalue[1][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	}
      */
      int dummy=ev->GetTag("Dummy", 0);
      if(dummy==1) continue; //don't store dummy trigger
      int chipid = data[4];
      int slabadd = data[3];
      int planeNumber = getPlaneNumberFromSlabAdd_siecal(slabadd);
      int bcid = data[1];
      int sca = data[2];

      //      std::cout<<"cycle:"<<data[0]<<" sca:"<<14-data[2]<<" slabadd:"<<slabadd<<" chipid:"<<chipid <<" chns: ";
      //      if((14-data[2])>0) {
	//	std::cout<<std::endl;
	//continue; //ignore sca>0 for the moment
      //}
      if (planeNumber >= 0) {

	if(planesXsize_siecal>1000) {
	  
	  int coorx=bcid;
	  int coory= sca;
	  int coordIndex = coorx * planesYsize_siecal + coory;
	  //if (SLABs[planeNumber][coordIndex] >= 0) // std::cout << "ERROR: channel already has a value" << std::endl;
	  //else {
	  if (SLABs[planeNumber][coordIndex] <0) {
	    SLABs[planeNumber][coordIndex] = 1;
	    if (SLABs[planeNumber][coordIndex] < 0) SLABs[planeNumber][coordIndex] = 0;
	    SLABHits[planeNumber]++;
	  }
	} else {
	  for (int ichan = 0; ichan < data[5]; ichan++) {
	    int adc = data[5 + 4+ (ichan*6) ]; // extract adc
	    int hitbit = data[5 + 5+ (ichan*6) ]; // extract hitbit
	    //	std::cout<<adc<< " "<<hitbit<<", ";
	    if (hitbit) {
	      if (adc < pedestalLimit) continue;
	      //get the index from the SLAB array
	      //standart view: 1st hbu in upper right corner, asics facing to the viewer, tiles in the back. Dit upper right corner:
	      int coorx = getXcoordFromChipChannel_siecal(slabadd,chipid, ichan);
	      int coory = getYcoordFromChipChannel_siecal(slabadd,chipid, ichan);
	      
	      int coordIndex = coorx * planesYsize_siecal + coory;
	      //if (SLABs[planeNumber][coordIndex] >= 0) // std::cout << "ERROR: channel already has a value" << std::endl;
	      //else {
	      if (SLABs[planeNumber][coordIndex] <0) {
		SLABs[planeNumber][coordIndex] = adc;
		if (SLABs[planeNumber][coordIndex] < 0) SLABs[planeNumber][coordIndex] = 0;
		SLABHits[planeNumber]++;
	      }
	    }
	  }
	}//else
      }//planeNumber>=0

    }//while
}
