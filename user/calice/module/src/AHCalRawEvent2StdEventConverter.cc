#include "eudaq/StdEventConverter.hh"
#include "eudaq/RawEvent.hh"
#ifdef _WIN32
//TODO remove the _WIN32 if not necessary in linux
#include <array>
#endif

//parameters to be later provided by a configuration file
#define planesXsize 24
#define planesYsize 24
#define planeCount 2
#define pedestalLimit 400

class AHCalRawEvent2StdEventConverter: public eudaq::StdEventConverter {
   public:
      bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
      static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceObject");

   private:
      int getPlaneNumberFromCHIPID(int chipid) const;
      int getXcoordFromChipChannel(int chipid, int channelNr) const;
      int getYcoordFromChipChannel(int chipid, int channelNr) const;
      const std::map<int, std::tuple<int, int, int>> mapping = { //tuple: layer, xcoordinate, ycoordinate
            //layer 1: single HBU
                  { 185, std::make_tuple(0, 18, 18) },
                  { 186, std::make_tuple(0, 18, 12) },
                  { 187, std::make_tuple(0, 12, 18) },
                  { 188, std::make_tuple(0, 12, 12) },
                  //layer 10: former layer 12 - full HBU
                  { 169, std::make_tuple(1, 18, 18) },
                  { 170, std::make_tuple(1, 18, 12) },
                  { 171, std::make_tuple(1, 12, 18) },
                  { 172, std::make_tuple(1, 12, 12) },
                  { 173, std::make_tuple(1, 18, 6) },
                  { 174, std::make_tuple(1, 18, 0) },
                  { 175, std::make_tuple(1, 12, 6) },
                  { 176, std::make_tuple(1, 12, 0) },
                  { 177, std::make_tuple(1, 6, 18) },
                  { 178, std::make_tuple(1, 6, 12) },
                  { 179, std::make_tuple(1, 0, 18) },
                  { 180, std::make_tuple(1, 0, 12) },
                  { 181, std::make_tuple(1, 6, 6) },
                  { 182, std::make_tuple(1, 6, 0) },
                  { 183, std::make_tuple(1, 0, 6) },
                  { 184, std::make_tuple(1, 0, 0) }
            };
//      const int planeCount = 2;
//      const int pedestalLimit = 400;
};

namespace {
auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
      Register<AHCalRawEvent2StdEventConverter>(AHCalRawEvent2StdEventConverter::m_id_factory);
}

bool AHCalRawEvent2StdEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const {
   std::string sensortype = "Calice"; //TODO ?? "HBU"
   auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
   size_t nblocks = ev->NumBlocks();
   std::vector<std::unique_ptr<eudaq::StandardPlane>> planes;
   std::vector<int> HBUHits;
   std::vector<std::array<int, planesXsize * planesYsize>> HBUs;         //HBU(aka plane) index, x*12+y
   for (int i = 0; i < planeCount; ++i) {
      std::array<int, planesXsize * planesYsize> HBU;
      HBU.fill(-1); //fill all channels to -1
      HBUs.push_back(HBU); //add the HBU to the HBU
      HBUHits.push_back(0);
   }
   unsigned int nblock = 7; // the first 7 blocks contain other information
   std::cout << ev->GetEventNumber() << "<" << std::flush;

   while (nblock < ev->NumBlocks()) {         //iterate over all asic packets from (hopefully) same BXID
      std::vector<int> data;
      const auto & bl = ev->GetBlock(nblock++);
      data.resize(bl.size() / sizeof(int));
      memcpy(&data[0], &bl[0], bl.size());
      if (data.size() != 77) std::cout << "vector has size : " << bl.size() << "\tdata : " << data.size() << std::endl;
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
      if (data[1] == 0) continue; //don't store dummy trigger
      int chipid = data[3];
      int planeNumber = getPlaneNumberFromCHIPID(chipid);

      for (int ichan = 0; ichan < data[4]; ichan++) {
         int adc = data[5 + data[4] + ichan] & 0x0FFF; // extract adc
         int gainbit = (data[5 + data[4] + ichan] & 0x2000) ? 1 : 0; //extract gainbit
         int hitbit = (data[5 + data[4] + ichan] & 0x1000) ? 1 : 0;  //extract hitbit
         if (planeNumber >= 0) {  //plane, which is not found, has index -1
            if (hitbit) {
               if (adc < pedestalLimit) continue;
               int coordIndex = getXcoordFromChipChannel(chipid, ichan) * planesXsize + getYcoordFromChipChannel(chipid, ichan); //get the index from the HBU array
               if (HBUs[planeNumber][coordIndex] >= 0) std::cout << "ERROR: channel already has a value" << std::endl;
               HBUs[planeNumber][coordIndex] = gainbit ? adc : 10 * adc;
               //HBUs[planeNumber][coordIndex] = 1;
               if (HBUs[planeNumber][coordIndex] < 0) HBUs[planeNumber][coordIndex] = 0;
               HBUHits[planeNumber]++;
            }
         }
      }
      std::cout << "." << std::flush;
   }
   for (int i = 0; i < HBUs.size(); ++i) {
      std::unique_ptr<eudaq::StandardPlane> plane(new eudaq::StandardPlane(i, "CaliceObject", sensortype));
      //plane->SetSizeRaw(13, 13, 1, 0);
      int pixindex = 0;
      plane->SetSizeZS(planesXsize, planesYsize, HBUHits[i], 1, 0);
      for (int x = 0; x < planesXsize; x++) {
         for (int y = 0; y < planesYsize; y++) {
            if (HBUs[i][x * planesXsize + y] >= 0) {
               plane->SetPixel(pixindex++, x, y, HBUs[i][x * planesXsize + y]);
            }
         }
      }

      //if (ev->GetEventNumber() > 0) plane->SetTLUEvent(ev.GetEventNumber());
      //save planes with hits hits to the onlinedisplay

      d2->AddPlane(*plane);
      std::cout << ":" << std::flush;
   }
   std::cout << ">" << std::endl;
   return true;
}

int AHCalRawEvent2StdEventConverter::getPlaneNumberFromCHIPID(int chipid) const {
   auto searchIterator = mapping.find(chipid);
   if (searchIterator == mapping.end()) return -1;
   auto result = std::get<0>(searchIterator->second);
   return result;
//   return planeNumbers[chipid];

   int planeNumber = -1;
   switch ((chipid - 1) >> 2) {
      // case 48: //193
      //    planeNumber = 0;
      //    break;
      case 46:   //185
         planeNumber = 0;
         break;
      case 42:   //169
         planeNumber = 1;
         break;
      case 43:   //173
         planeNumber = 2;
         break;

         // case 59: //237
         //    planeNumber = 0;
         //    break;
         // case 60: //241
         //    planeNumber = 1;
         //    break;
         // case 61: //245
         //    planeNumber = 2;
         //    break;
         // case 30: //121
         //    planeNumber = 3;
         //    break;
         // case 29: //117
         //    planeNumber = 4;
         //    break;
         // case 62: //249
         //    planeNumber = 5;
         //    break;
         // case 56: //225
         //    planeNumber = 6;
         //    break;
         // case 54: //217
         //    planeNumber = 7;
         //    break;
         // case 53: //213
         //    planeNumber = 8;
         //    break;
         // case 51: //205
         //    planeNumber = 9;
         //    break;
         // case 55: //221
         //    planeNumber = 10;
         //    break;
         // case 50: //201
         //    planeNumber = 11;
         //    break;
      default:
         break;
   }
   return planeNumber;
}

int AHCalRawEvent2StdEventConverter::getXcoordFromChipChannel(int chipid, int channelNr) const {
   auto searchIterator = mapping.find(chipid);
   if (searchIterator == mapping.end()) return 0;
   auto asicXCoordBase = std::get<1>(searchIterator->second);

   int subx = channelNr / 6 + asicXCoordBase;
   return subx;
//   if (((chipid & 0x03) == 0x01) || ((chipid & 0x03) == 0x02)) {
//      //1st and 2nd spiroc are in the righ half of HBU
//      subx += 6;
//   }
//   return subx;
}

int AHCalRawEvent2StdEventConverter::getYcoordFromChipChannel(int chipid, int channelNr) const {
   auto searchIterator = mapping.find(chipid);
   if (searchIterator == mapping.end()) return 0;
   auto asicYCoordBase = std::get<2>(searchIterator->second);

   int suby = channelNr % 6;
   if (((chipid & 0x03) == 0x00) || ((chipid & 0x03) == 0x03)) {
      //3rd and 4th spiroc have different channel order
      suby = 5 - suby;
   }
   suby += asicYCoordBase;
//   if (((chipid & 0x03) == 0x01) || ((chipid & 0x03) == 0x03)) {
//      suby += 6;
//   }
   return suby;
}
