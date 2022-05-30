#include "eudaq/DataCollector.hh"
#include "eudaq/Logger.hh"
#include <mutex>
#include <deque>
#include <map>
#include <set>
#include "eudaq/Utils.hh"
#include <iostream>
#include <ostream>
#include <ctime>
#include <iomanip>
#include <cxxabi.h>
#include <math.h>       /* fabs */

#include "eudaq/CommandReceiver.hh"
#include "eudaq/FileWriter.hh"
#include "eudaq/DataSender.hh"
#include "eudaq/DataReceiver.hh"
#include "eudaq/Event.hh"
#include "eudaq/RawEvent.hh"
#include "eudaq/Configuration.hh"
#include "eudaq/Utils.hh"
#include "eudaq/Platform.hh"
#include "eudaq/Factory.hh"

#define DAQ_ERRORS_INCOMPLETE        0x0001
#define DAQ_ERRORS_OUTSIDE_ACQ       0x0002
#define DAQ_ERRORS_MULTIPLE_TRIGGERS 0x0004
#define DAQ_ERRORS_MISSED_DUMMY      0x0008
#define DAQ_ERRORS_MISSING_START     0x0010
#define DAQ_ERRORS_MISSING_STOP      0x0020

//ecal stuff
#define retrigger_th 4 //if -1, we do not perform filtering
#define bcid_th_siecal 2 
#define th_coincidences 0 //if 0, we save all events
#define debug 0

using namespace eudaq;
using DataCollectorSP = Factory<DataCollector>::SP_BASE;

class CaliceROC2BCIDDataCollector: public eudaq::DataCollector {
   public:
      CaliceROC2BCIDDataCollector(const std::string &name, const std::string &runcontrol);
      void DoStartRun() override;
      void DoConfigure() override;
      void DoConnect(eudaq::ConnectionSPC id) override;
      void DoDisconnect(eudaq::ConnectionSPC id) override;
      void DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev) override;

      static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceROC2BCIDDataCollector");

   private:
      void AddEvent_TimeStamp(uint32_t id, eudaq::EventSPC ev);
      void BuildEvent_roc_bxid();

  //ahcal stuff
      void AhcalRoc2Bxid(std::deque<eudaq::EventSPC> &queue_roc, std::deque<eudaq::EventSPC> &queue_bxid);
      std::vector<uint32_t> vectorU8toU32(const std::vector<uint8_t> &data);
      inline int ldaTS2BXID(const uint64_t triggerTS, const uint64_t startTS, const int bxid0TSOffer, const int bxidLength);

      int m_ldabxid0offset = 285;
      int m_ahcalBxidLength = 8;
      int m_require_LDAtrigger = 0;

  //ecal stuff
  // datablocks BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
  void SiecalRoc2Bxid(std::deque<eudaq::EventSPC> &queue_roc, std::deque<eudaq::EventSPC> &queue_bxid);
  std::map<int,std::vector<std::vector<int>> > BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
  std::vector<std::vector<int>> FilterSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
  eudaq::EventUP SiECALBxidEvent(int bxid,std::vector<std::vector<int>> vector_of_blocks_siecal);

      std::mutex m_mutex;
      //to be edited according to the name of the processes. TODO implement as parameter to the configuration
      std::string mc_name_ahcal = "AHCAL1";
      std::string mc_name_siecal = "ECAL1";
      std::string mc_name_bif = "BIF1";
      std::string mc_name_desytable = "desytable1";

      //correction factor to be added to the ROC tag. This can eliminate differences between starting ROC numbers in different producer - some can start from 1, some from 0
      int m_roc_offset_ahcal = 0;
      int m_roc_offset_siecal = 0;
      int m_roc_offset_bif = 0;

      static const int mc_roc_invalid = INT32_MAX;
      static const int mc_bxid_invalid = INT32_MAX;

      //ts
      std::deque<eudaq::EventSPC> m_que_ahcal_ROC; //Queue for AHCAL ROC events
      std::deque<eudaq::EventSPC> m_que_ahcal_BXID; //Queue for AHCAL BXID events
      std::deque<eudaq::EventSPC> m_que_siecal_ROC;
      std::deque<eudaq::EventSPC> m_que_siecal_BXID;
      std::deque<eudaq::EventSPC> m_que_bif_ROC;
      std::deque<eudaq::EventSPC> m_que_bif_BXID;
      std::deque<eudaq::EventSPC> m_que_desytable;
      bool m_active_ahcal = false;
      bool m_active_siecal = false;
      bool m_active_bif = false;
      bool m_active_desytable = false;

      //final test whether the event will be thrown away if a mandatory subevents are not present
      bool m_evt_mandatory_ahcal = false;
      bool m_evt_mandatory_siecal = true;
      bool m_evt_mandatory_bif = true;
      bool m_evt_mandatory_desytable = false;

      int m_thrown_incomplete = 0; //how many event were thrown away because there were incomplete

      std::chrono::time_point<std::chrono::system_clock> lastprinttime;
      eudaq::EventSPC m_ev_last_cal;
      eudaq::EventSPC m_ev_last_bif;
      uint64_t m_ts_end_last_cal;
      uint64_t m_ts_end_last_bif;
      bool m_offset_ahcal2bif_done;
      int64_t m_ts_offset_ahcal2bif;
      uint32_t m_ev_n;
      bool m_disable_print;
};

namespace {
   auto dummy0 = eudaq::Factory<eudaq::DataCollector>::
         Register<CaliceROC2BCIDDataCollector, const std::string&, const std::string&>
         (CaliceROC2BCIDDataCollector::m_id_factory);
}

CaliceROC2BCIDDataCollector::CaliceROC2BCIDDataCollector(const std::string &name, const std::string &runcontrol) :
      DataCollector(name, runcontrol) {
}

void CaliceROC2BCIDDataCollector::DoStartRun() {
   m_que_ahcal_ROC.clear();
   m_que_siecal_ROC.clear();
   m_que_bif_ROC.clear();
   m_que_desytable.clear();

   m_ts_end_last_cal = 0;
   m_ts_end_last_bif = 0;
   m_offset_ahcal2bif_done = false;
   m_ts_offset_ahcal2bif = 0;
   m_ev_n = 0;
   m_thrown_incomplete = 0;

}

void CaliceROC2BCIDDataCollector::DoConfigure() {
   auto conf = GetConfiguration();
   if (conf) {
      conf->Print();
      // m_pri_ts = conf->Get("PRIOR_TIMESTAMP", m_pri_ts?1:0);
   }
   int MandatoryBif = conf->Get("MandatoryBif", 1);
   m_evt_mandatory_bif = (MandatoryBif == 1) ? true : false;
   m_disable_print = conf->Get("DISABLE_PRINT", 1) == 1 ? true : false;
   std::cout << "#MandatoryBif=" << MandatoryBif << std::endl;
   lastprinttime = std::chrono::system_clock::now();
   m_ldabxid0offset = conf->Get("LdaBxid0Offset", 285);
   m_ahcalBxidLength = conf->Get("AhcalBxidLength", 8);
   m_require_LDAtrigger = conf->Get("RequireLdaTrigger", 0);
}

void CaliceROC2BCIDDataCollector::DoConnect(eudaq::ConnectionSPC idx) {
   bool recognizedProducer = false;
   std::cout << "connecting " << idx << std::endl;
   if (idx->GetName() == mc_name_ahcal) {
      m_active_ahcal = true;
      recognizedProducer = true;
   }
   if (idx->GetName() == mc_name_siecal) {
      m_active_siecal = true;
      recognizedProducer = true;
   }
   if (idx->GetName() == mc_name_bif) {
      m_active_bif = true;
      recognizedProducer = true;
   }
   if (idx->GetName() == mc_name_desytable) {
      m_active_desytable = true;
      recognizedProducer = true;
   }
   if (!recognizedProducer) {
      std::cout << "Producer " << idx->GetName() << " is not recognized by this datacollector" << std::endl;
      EUDAQ_ERROR_STREAMOUT("Producer " + idx->GetName() + " is not recognized by this datacollector", std::cout, std::cerr);
   }
}

void CaliceROC2BCIDDataCollector::DoDisconnect(eudaq::ConnectionSPC idx) {
   std::cout << "disconnecting " << idx << std::endl;
   if (idx->GetName() == mc_name_ahcal) m_active_ahcal = false;
   if (idx->GetName() == mc_name_siecal) m_active_siecal = false;
   if (idx->GetName() == mc_name_bif) m_active_bif = false;
   if (idx->GetName() == mc_name_desytable) m_active_desytable = false;
   std::cout << m_thrown_incomplete << " incomplete events thrown away" << std::endl;
   if (!(m_active_ahcal && m_active_bif && m_active_desytable)) {
//      BuildEvent_bxid();
      EUDAQ_INFO_STREAMOUT("Saved events: " + std::to_string(m_ev_n), std::cout, std::cerr);
   }
}

void CaliceROC2BCIDDataCollector::DoReceive(eudaq::ConnectionSPC idx, eudaq::EventSP ev) {
   if (ev->IsFlagFake()) {
      EUDAQ_WARN("Receive event fake");
      return;
   }
   std::lock_guard<std::mutex> lock(m_mutex); //only 1 process should be allowed to make events
//   if (ev->GetTag("BXID", mc_bxid_invalid) == mc_bxid_invalid) {
//      EUDAQ_ERROR_STREAMOUT("Received event without BXID", std::cout, std::cerr);
//      return;
//   }
   std::string con_name = idx->GetName();
   if (con_name == mc_name_ahcal)
      m_que_ahcal_ROC.push_back(std::move(ev));
   else if (con_name == mc_name_siecal)
      m_que_siecal_ROC.push_back(std::move(ev));
   else if (con_name == mc_name_bif)
      m_que_bif_ROC.push_back(std::move(ev));
   else if (con_name == mc_name_desytable)
      m_que_desytable.push_back(std::move(ev));
   else {
      EUDAQ_WARN("Receive event from unkonwn Producer");
      return;
   }
   //   while (!m_que_ahcal_ROC.empty() )
   //   AhcalRoc2Bxid(m_que_ahcal_ROC, m_que_ahcal_BXID);

   while (!m_que_siecal_ROC.empty() )
      SiecalRoc2Bxid(m_que_siecal_ROC, m_que_siecal_BXID);

   //quit if not enough events to merge
   if (m_que_ahcal_BXID.empty() && m_active_ahcal) return;
   if (m_que_siecal_BXID.empty() && m_active_siecal) return;
   if (m_que_bif_BXID.empty() && m_active_bif) return;
   BuildEvent_roc_bxid();
// std::cout<<"p 1 \n";
}

std::vector<uint32_t> CaliceROC2BCIDDataCollector::vectorU8toU32(const std::vector<uint8_t> &data) {
   const uint32_t *ptr = reinterpret_cast<const uint32_t*>(data.data());
   if (data.size() & 0x03) std::cout << "ERROR: unaligned data block, cannot convert u8 to u32." << std::endl;
   return std::vector<uint32_t>(ptr, ptr + data.size() / 4);
}

inline int CaliceROC2BCIDDataCollector::ldaTS2BXID(const uint64_t triggerTS, const uint64_t startTS, const int bxid0TSOffer, const int bxidLength) {
   return (triggerTS - startTS - bxid0TSOffer) / bxidLength;
}

void CaliceROC2BCIDDataCollector::AhcalRoc2Bxid(std::deque<eudaq::EventSPC> &queue_roc, std::deque<eudaq::EventSPC> &queue_bxid) {
   eudaq::EventSPC ahcalEventROC = queue_roc.front();
   int evtROC = ahcalEventROC->GetTag("ROC", -1);
   int evtErrorStatus = ahcalEventROC->GetTag("DAQ_ERROR_STATUS", 0);
   uint64_t evtTBTimstamp = ahcalEventROC->GetTag("tbTimestamp", 0); //timestamp for reprocessing and setting the slcio date
   uint64_t evtStartTs = ahcalEventROC->GetTag("ROCStartTS", 0);
   auto streamNr = ahcalEventROC->GetStreamN();
   auto block0 = ahcalEventROC->GetBlock(0); //"EUDAQDataScCAL";
   auto block1 = ahcalEventROC->GetBlock(1); //      s = "i:CycleNr,i:BunchXID,i:EvtNr,i:ChipID,i:NChannels,i:TDC14bit[NC],i:ADC14bit[NC]";
   auto block2 = ahcalEventROC->GetBlock(2); //unixtimestamp
   auto block3 = ahcalEventROC->GetBlock(3); // dummy block to be filled later with slowcontrol files
   auto block4 = ahcalEventROC->GetBlock(4); // dummy block to be filled later with LED information (only if LED run)
   auto block5 = ahcalEventROC->GetBlock(5); // dummy block to be filled later with temperature
   std::vector<uint32_t> LDATSData = vectorU8toU32(ahcalEventROC->GetBlock(6)); //lda timestamps
   auto block7 = ahcalEventROC->GetBlock(7); //stopping bxids
   auto block8 = ahcalEventROC->GetBlock(8); //hv adjust
   auto block9 = ahcalEventROC->GetBlock(9); //future
   uint64_t startTS = ((uint64_t) LDATSData[0] | ((uint64_t) LDATSData[1] << 32));
   uint64_t stopTS = ((uint64_t) LDATSData[2] | ((uint64_t) LDATSData[3] << 32));
   std::multimap<int, uint64_t> LdaTrigTSs;
   for (int i = 4; i < LDATSData.size(); i = i + 2) {
      uint64_t triggerTs = ((uint64_t) LDATSData[i] | ((uint64_t) LDATSData[i + 1] << 32));
      int trigBxid = ldaTS2BXID(triggerTs, startTS, m_ldabxid0offset, m_ahcalBxidLength);
      if (triggerTs)
         LdaTrigTSs.insert( { trigBxid, triggerTs });
   }
   int lastValidBXID = 65535; //the lowest bxid in memory cells 15 (the last) in the whole detector
   std::map<int, std::vector<std::vector<uint32_t> > > bxids; //map <bxid,vector<asicpackets>>
   for (int numblock = 10; numblock < ahcalEventROC->GetNumBlock(); numblock++) {
      //structure of AHCAL block: [0]=_cycle_no, [1]=bxid, [2]=memcell, [3]=chipid, [4]=nchannel, [5..(5+36-1)]=TDC, [(5+36)..(5+72-1)]=ADC
      std::vector<uint32_t> block = vectorU8toU32(ahcalEventROC->GetBlock(numblock));
      // std::cout << "numblock=" << numblock << ", size=" << block.size() << std::endl;
      int blockBXID = block[1];
      //std::cout << std::endl << "blockBXID=" << blockBXID << std::endl;
      int blockMemCell = block[2];
      //std::cout << std::endl << "blockMemCell=" << blockMemCell << std::endl;
      if ((blockMemCell == 15) && (blockBXID < lastValidBXID)) lastValidBXID = blockBXID;
      std::vector<std::vector<uint32_t> > &sameBxidPackets = bxids.insert( { blockBXID, std::vector<std::vector<uint32_t> >() }).first->second;
      sameBxidPackets.push_back(block);
   }
   std::multimap<int, uint64_t>::iterator LdaTrigIt = LdaTrigTSs.begin();
   for (std::pair<const int, std::vector<std::vector<uint32_t> > > &sameBxidPackets : bxids) {
      int bxid = sameBxidPackets.first;
      eudaq::EventUP nev = eudaq::Event::MakeUnique("CaliceObject");
      eudaq::RawEvent *nev_raw = dynamic_cast<RawEvent*>(nev.get());
      nev->SetStreamN(streamNr);
      int ErrorStatus = 0;
      if (!startTS) ErrorStatus |= DAQ_ERRORS_MISSING_START;
      if (!stopTS) ErrorStatus |= DAQ_ERRORS_MISSING_STOP;
      nev_raw->AddBlock(0, block0);
      nev_raw->AddBlock(1, block1);
      nev_raw->AddBlock(2, block2);
      nev_raw->AddBlock(3, block3);
      nev_raw->AddBlock(4, block4);
      nev_raw->AddBlock(5, block5);
      nev_raw->AddBlock(7, block7);
      nev_raw->AddBlock(8, block8);
      nev_raw->AddBlock(9, block9);
      nev->SetTag("ROCStartTS", startTS);
      nev->SetTag("ROC", evtROC);
      nev->SetTag("BXID", bxid);
      nev->SetTimestamp(startTS + m_ldabxid0offset + bxid * m_ahcalBxidLength, startTS + m_ldabxid0offset + (bxid + 1) * m_ahcalBxidLength, 1);
      std::vector<uint32_t> cycledata;
      cycledata.push_back((uint32_t) (startTS));
      cycledata.push_back((uint32_t) (startTS >> 32));
      cycledata.push_back((uint32_t) (stopTS));
      cycledata.push_back((uint32_t) (stopTS >> 32));
      uint64_t triggerTs = 0ULL;
      int matchingTriggers = 0;
      while (LdaTrigIt != LdaTrigTSs.end()) { //pick the first trigger TS for given bxid
         int trigBxid = LdaTrigIt->first;
         if (trigBxid > bxid) break;
         if (trigBxid < bxid) {
            LdaTrigIt++;
            continue;
         }
         triggerTs = LdaTrigIt->second;
         matchingTriggers++;
         LdaTrigIt++;
      }
      if (m_require_LDAtrigger && (matchingTriggers == 0)) continue; //skip bxid events that are not validated
      cycledata.push_back((uint32_t) (triggerTs));
      cycledata.push_back((uint32_t) (triggerTs >> 32));
      nev->SetTag("TrigBxidTdc", (int) ((triggerTs - startTS - m_ldabxid0offset) % m_ahcalBxidLength));

      if (matchingTriggers > 1) ErrorStatus |= DAQ_ERRORS_MISSING_START;
      if (bxid > lastValidBXID) ErrorStatus |= DAQ_ERRORS_INCOMPLETE;
      nev_raw->AppendBlock(6, std::move(cycledata));
      for (auto &minipacket : sameBxidPackets.second) {
         if (minipacket.size()) {
            nev_raw->AddBlock(nev_raw->NumBlocks(), std::move(minipacket));
         }
      }
      nev->SetTag("DAQ_ERROR_STATUS", ErrorStatus);
      nev->SetTag("tbTimestamp", evtTBTimstamp);
      queue_bxid.push_back(std::move(nev));

   }
   queue_roc.pop_front();
}

//****************************** ECAL Part

void CaliceROC2BCIDDataCollector::SiecalRoc2Bxid(std::deque<eudaq::EventSPC> &queue_roc, std::deque<eudaq::EventSPC> &queue_bxid) {

  eudaq::EventSPC ev_siecal = queue_roc.front();
    
    int nslabs=ev_siecal->GetTag("NSLBs",0);
    //    std::cout<<" nslabs "<< nslabs<<std::endl;
    
    std::vector<std::vector<int>> siecal_blocks;
    for(int iblock=(4+nslabs+1); iblock<ev_siecal->NumBlocks(); iblock++){
      auto bl = ev_siecal->GetBlock(iblock);
      std::vector<int> v;
      if(bl.size()==0) continue;
      v.resize(bl.size() / sizeof(int));
      memcpy(&v[0], &bl[0], bl.size());
      siecal_blocks.push_back(v);
    }

    if(siecal_blocks.size()>0) {
      std::map<int,std::vector<std::vector<int>> > sorted_bybcid= BuildSiECAL(siecal_blocks);
      std::map<int,std::vector<std::vector<int>> > ::iterator it;
      for (it=sorted_bybcid.begin(); it!=sorted_bybcid.end(); ++it) {
	eudaq::EventUP ev_sub_siecal =  SiECALBxidEvent(it->first,it->second);
	if(  ev_sub_siecal->GetTag("Layers_in_coincidence",-1) > th_coincidences) {
	  ev_sub_siecal->SetEventN(m_ev_n++);
	  // WriteEvent(std::move(ev_sub_siecal));
	  queue_bxid.push_back(std::move(ev_sub_siecal));

	}
      }
    }

   queue_roc.pop_front();
}


//--------------------------------
eudaq::EventUP CaliceROC2BCIDDataCollector::SiECALBxidEvent(int first, std::vector<std::vector<int>> second) {

  auto ev_siecal = m_que_siecal_ROC.front();
  auto streamNr = ev_siecal->GetStreamN();
  int nslabs=ev_siecal->GetTag("NSLBs",0);

  eudaq::EventUP ev_sub_siecal =  eudaq::Event::MakeUnique("CaliceObject");
  eudaq::RawEvent *nev_raw = dynamic_cast<RawEvent*>(ev_sub_siecal.get());

  ev_sub_siecal->SetStreamN(streamNr);
  ev_sub_siecal->SetTag("ROC",ev_siecal->GetTag("ROC", -1));
  ev_sub_siecal->SetTag("NSLBs",ev_siecal->GetTag("NSLBs", -1));
  ev_sub_siecal->SetTag("SiECAL_StartAcqTime",ev_siecal->GetTag("SiECAL_StartAcqTime", -1)); // DOESN'T WORK -- to be checked
  ev_sub_siecal->SetTag("BXID",first);
	
  //copy all initial blocks
  auto bl0 = ev_siecal->GetBlock(0);
  std::string colName((char *) &bl0.front(), bl0.size());
  nev_raw->AddBlock(0, colName.c_str(), colName.length());
  
  auto bl1 = ev_siecal->GetBlock(1);
  std::string coldesc((char *) &bl1.front(), bl1.size());
  nev_raw->AddBlock(1, coldesc.c_str(), coldesc.length());
  
  auto bl2 = ev_siecal->GetBlock(2);
  std::vector<int> times;
  times.resize(bl2.size() / sizeof(int));
  memcpy(&times[0], &bl2[0], bl2.size());
  nev_raw->AddBlock(2, bl2);//v, sizeof(v));
  
  auto bl3 = ev_siecal->GetBlock(3);
  std::string colName2((char *) &bl3.front(), bl3.size());
  nev_raw->AddBlock(3, colName2.c_str(), colName2.length());
  //copy all slowcontrol and info blocks
  for(int iblock=4; iblock<(4+nslabs); iblock++) nev_raw->AddBlock(nev_raw->NumBlocks(),ev_siecal->GetBlock(iblock));
  int coincidences_per_layer[100]={0};
  for(int i=0; i<second.size(); i++) {
    nev_raw->AddBlock(nev_raw->NumBlocks(),second.at(i));
    coincidences_per_layer[int(second.at(i).at(3))]++;
  }
  int ncoincidences=0;
  for(int ilayer=0; ilayer<ev_siecal->GetTag("NSLBs", 0); ilayer ++)
    if(coincidences_per_layer[ilayer]>0) ncoincidences++;

  nev_raw->SetTag("Layers_in_coincidence",ncoincidences);

  return ev_sub_siecal;
}

//--------------------------------
std::map<int, std::vector<std::vector<int>>> CaliceROC2BCIDDataCollector::BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal) {
  
 std::map<int, std::vector<std::vector<int>>> sorted_bybcid;
  //INFO
  if(debug==1) std::cout<<"BuildSiECAL"<<" "<<vector_of_blocks_siecal.size()<<std::endl;
  //info of data structure
  //"i:CycleNr,i:BunchXID,i:sca,i:Layer,i:SkirocID,i:NChannels,i:hit_low[NC],i:gain_low[NC],ADC_low[NC],i:hit_high[NC],i:gain_high[NC],ADC_high[NC]";

  //******************
  //First I filter the retrigger events
  std::vector<std::vector<int>> vector_of_blocks_siecal_filtered;
  if(retrigger_th==-1)
    vector_of_blocks_siecal_filtered=vector_of_blocks_siecal;
  else
    vector_of_blocks_siecal_filtered=FilterSiECAL(vector_of_blocks_siecal);//result
  if(debug==1) if(vector_of_blocks_siecal.size()!=vector_of_blocks_siecal_filtered.size()) std::cout<< "RETRIGGERS FOUND "<<std::endl;

  
  //** SECOND the EVENT BUILDING PART

  //I create a map of bcid,datablocks with a merging of +-bcid_th_sieca, after filtering for retriggers
  // 1) I check the first bcid that I find, later I check the rest of blocks and if I have a bcid+-1, I added to the bcid and remove those elements
  if(vector_of_blocks_siecal_filtered.size()==0) return sorted_bybcid;
  
  for(int i=0; i<(vector_of_blocks_siecal_filtered.size()); i++) {
    
    int bcid_ref=vector_of_blocks_siecal_filtered.at(i).at(1);

    // WORK IN PROGRESS
    // naive solution so far... to be improved
    std::map<int,std::vector<std::vector<int>> > ::iterator it;

    int bcid_it=-1;
    for(int i=bcid_ref-bcid_th_siecal; i<bcid_ref+bcid_th_siecal+1; i++) {
      it=sorted_bybcid.find(i);
      if(it != sorted_bybcid.end()) {
	bcid_it=i;
      }
    }
    
    if(bcid_it>0) {
      bcid_ref=bcid_it;
      sorted_bybcid[bcid_ref].push_back(vector_of_blocks_siecal_filtered.at(i));
    } else {
      std::vector<std::vector<int>> new_vector_of_blocks;
      new_vector_of_blocks.push_back(vector_of_blocks_siecal_filtered.at(i));
      sorted_bybcid[bcid_ref]=new_vector_of_blocks;
    }
    
    //cluster in bcid_ref all bcid+-bcid_th_siecal
    for(int j=i+1; j<vector_of_blocks_siecal_filtered.size(); j++) {
      int bcid_ref2=vector_of_blocks_siecal_filtered.at(j).at(1);
      if( fabs(bcid_ref-bcid_ref2)<bcid_th_siecal) {
	sorted_bybcid[bcid_ref].push_back(vector_of_blocks_siecal_filtered.at(j));
	vector_of_blocks_siecal_filtered.erase(vector_of_blocks_siecal_filtered.begin()+j-1);
      }
    }  
  }

  std::map<int, std::vector<std::vector<int>>> sorted_bybcid2;

  //recluster the bcid, to use the most probable bcid as the one in the map index
  std::map<int,std::vector<std::vector<int>> > ::iterator it;
  for (it=sorted_bybcid.begin(); it!=sorted_bybcid.end(); ++it) {
    int bcid_map=it->first;
    float bcid_av=0;
    float n_av=0;
    for(int j=0; j<it->second.size(); j++) {
      n_av++;
      bcid_av+=it->second.at(j).at(1);
    }
    bcid_av/=n_av;
    sorted_bybcid2[int(bcid_av)]=it->second;
  }
  
  return sorted_bybcid2;
  
}

//--------------------------------
std::vector<std::vector<int>> CaliceROC2BCIDDataCollector::FilterSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal) {

  //info of data structure
  //"i:CycleNr,i:BunchXID,i:sca,i:Layer,i:SkirocID,i:NChannels,i:hit_low[NC],i:gain_low[NC],ADC_low[NC],i:hit_high[NC],i:gain_high[NC],ADC_high[NC]";
  
  std::vector<std::vector<int>> vector_of_blocks_siecal_filtered;
  
   //sort by layer
  std::map<int,std::vector<std::vector<int>> >  sorted_bylayerchip;
  
  for(int i=0; i<vector_of_blocks_siecal.size(); i++) {
    if(debug==1) std::cout<<i<<" "<<vector_of_blocks_siecal.size()<<" map size"<<sorted_bylayerchip.size()<<std::endl;

    int layerid=vector_of_blocks_siecal.at(i).at(3);
    int chipid=vector_of_blocks_siecal.at(i).at(4);
    int index=10000*layerid+chipid;
    if(debug==1) std::cout<<"layerid:"<<layerid<<" chipid:"<<chipid<<" index:"<<index<<std::endl;

    std::map<int,std::vector<std::vector<int>> > ::iterator it;
    it=sorted_bylayerchip.find(index);
    if( it == sorted_bylayerchip.end() ) {
      //new layer
      std::vector<std::vector<int>> new_vector_of_blocks;
      new_vector_of_blocks.push_back(vector_of_blocks_siecal.at(i));
      sorted_bylayerchip[index]=new_vector_of_blocks;
      if(debug==1) std::cout<<"new map entry"<<std::endl;
    } else {
      //existing layer
      sorted_bylayerchip[index].push_back(vector_of_blocks_siecal.at(i));
      if(debug==1) std::cout<<"existing entry"<<std::endl;
    }
  }


  
  std::map<int,std::vector<std::vector<int>> > ::iterator it;
  for (it=sorted_bylayerchip.begin(); it!=sorted_bylayerchip.end(); ++it) {

    int badbcid[100]={0};//15scas is the maximum... we use 100 just in case
    int nfilledscas=it->second.size();
    if(debug==1) {
      std::cout<<"LayerChip:"<<it->first<<" size of vector of blocks:"<<it->second.size()<<" |";
      for(int i=0; i<it->second.size(); i++) {
	std::cout<<" cycle:"<<it->second.at(i).at(0)<<" BXID:"<<it->second.at(i).at(1)<<" layer:"<<it->second.at(i).at(3)<<" chip:"<<it->second.at(i).at(4)<<" size of block:"<<it->second.at(i).size();
      }
      std::cout<<std::endl;
    }
    for(int i=0; i<it->second.size(); i++) {
      if(badbcid[i]>0) continue;
      badbcid[i]=1;
      int bcid=it->second.at(i).at(1);
      int bcid_scaplus1=-1;
      int bcid_scaplus2=-1;
      int bcid_scaminus1=-1;
      int bcid_scaminus2=-1;
      
      //case a) sca=0
      if(i==0) {
	if(nfilledscas > (i+1) )bcid_scaplus1=it->second.at(i+1).at(1);
	if(nfilledscas > (i+2)  ) bcid_scaplus2=it->second.at(i+2).at(1);
	if(debug==1) std::cout<<"case a, i=0: bcid:"<<bcid<<" +1"<<bcid_scaplus1<<" +2"<<bcid_scaplus2<<std::endl;
	if(bcid_scaplus2<0) badbcid[i]=0;
	else if( fabs(bcid_scaplus1-bcid)<retrigger_th  && fabs(bcid_scaplus2-bcid_scaplus1)<retrigger_th ) {
	  badbcid[i]=1;
	  badbcid[i+1]=1;
	  badbcid[i+2]=1;
	  continue;
	} else badbcid[i]=0;
      } else if(i< (nfilledscas-1)) { //case b, in the middle
	bcid_scaplus1=it->second.at(i+1).at(1);
	bcid_scaminus1=it->second.at(i-1).at(1);
	if( fabs(bcid_scaplus1-bcid)<retrigger_th  && fabs(bcid-bcid_scaminus1)<retrigger_th ) {
	  badbcid[i]=1;
	  badbcid[i-1]=1;
	  badbcid[i+1]=1;
	} else badbcid[i]=0;
      } else {//lastsca
	bcid_scaminus1=it->second.at(i-1).at(1);
	if( fabs(bcid_scaminus1-bcid)<retrigger_th ) {
	  badbcid[i]=1;
	  badbcid[i-1]=1;
	}
      }

      if(debug==1) std::cout<<"BADBCID:"<<badbcid[i]<<std::endl;
    }

    //I will treat the first retrigger as a "good trigger"... just in case.
    for(int i=0; i<it->second.size(); i++) {
      if(badbcid[i]==1) {
	badbcid[i]=0;
	break;
      }
    }
      
    for(int i=0; i<it->second.size(); i++) {
      if(badbcid[i]==0) {
      	vector_of_blocks_siecal_filtered.push_back(it->second.at(i));//vect2);
      }
    }
  }
  //"i:CycleNr,i:BunchXID,i:sca,i:Layer,i:SkirocID,i:NChannels,i:hit_low[NC],i:gain_low[NC],ADC_low[NC],i:hit_high[NC],i:gain_high[NC],ADC_high[NC]";

  if(debug==1) {
    std::cout<<"AFTER FILTERING ";
    for(int k=0; k<vector_of_blocks_siecal_filtered.size(); k++) {
      std::cout<<" cycle:"<<vector_of_blocks_siecal_filtered.at(k).at(0)<<" BXID:"<<vector_of_blocks_siecal_filtered.at(k).at(1)<<" layer:"<<vector_of_blocks_siecal_filtered.at(k).at(3)<<" chip:"<<vector_of_blocks_siecal_filtered.at(k).at(4)<<" | ";
    }
    std::cout<<std::endl;
  }
     
 
  return vector_of_blocks_siecal_filtered;
}

 

// ******************* Common Part
inline void CaliceROC2BCIDDataCollector::BuildEvent_roc_bxid() {

   while (true) {
      SetStatusTag("Queue", std::string("(") + std::to_string(m_que_ahcal_ROC.size())
            + "," + std::to_string(m_que_siecal_ROC.size())
            + "," + std::to_string(m_que_bif_ROC.size())
            //            + "," + std::to_string(m_que_hodoscope1.size())
            //            + "," + std::to_string(m_que_hodoscope2.size())
            + "," + std::to_string(m_que_desytable.size()) + ")");
      if (std::chrono::system_clock::now() - lastprinttime > std::chrono::milliseconds(5000)) {
         lastprinttime = std::chrono::system_clock::now();
         std::cout << "Queue sizes: ";
         if (m_active_ahcal) std::cout << "AHCAL:" << m_que_ahcal_ROC.size();
         if (m_active_siecal) std::cout << " ECAL:" << m_que_siecal_ROC.size();
         if (m_active_bif) std::cout << " BIF:" << m_que_bif_ROC.size();
         if (m_active_desytable) std::cout << " table:" << m_que_desytable.size();
         std::cout << std::endl;
      }
//      int bxid_ahcal = mc_bxid_invalid;
//      int bxid_bif = mc_bxid_invalid;
      int roc_ahcal = mc_roc_invalid;
      int roc_siecal = mc_roc_invalid;
      int roc_bif = mc_roc_invalid;

      int bxid_ahcal = mc_bxid_invalid;
      int bxid_siecal = mc_bxid_invalid;
      int bxid_bif = mc_bxid_invalid;
//      const eudaq::EventSP ev_front_cal;
//      const eudaq::EventSP ev_front_bif;

//      uint64_t ts_beg_bif = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_end_bif = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_beg_cal = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_end_cal = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_beg = UINT64_C(0); //timestamps in 0.78125 ns steps
//      uint64_t ts_end = UINT64_C(0); //timestamps in 0.78125 ns steps

      if (m_que_ahcal_BXID.empty()) {
         if (m_active_ahcal) return; //more will come
      } else {
         roc_ahcal = m_que_ahcal_BXID.front()->GetTag("ROC", mc_roc_invalid);
         bxid_ahcal = m_que_ahcal_BXID.front()->GetTag("BXID", mc_bxid_invalid);
         if ((roc_ahcal == mc_roc_invalid) || (bxid_ahcal == mc_bxid_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_ahcal_BXID.front()->GetEventN()) + " without AHCAL ROC(" + ") or BXID("
                  + std::to_string(bxid_ahcal) + std::to_string(roc_ahcal) + ") in run " + std::to_string(m_que_ahcal_BXID.front()->GetRunNumber()), std::cout,
                  std::cerr);
            m_que_ahcal_BXID.pop_front();
            continue;
         }
         roc_ahcal += m_roc_offset_ahcal;
      }
      if (m_que_siecal_BXID.empty()) {
         if (m_active_siecal) return; //more will come
      } else {
         roc_siecal = m_que_siecal_BXID.front()->GetTag("ROC", mc_roc_invalid);
         bxid_siecal = m_que_siecal_BXID.front()->GetTag("BXID", mc_bxid_invalid);
         if ((roc_siecal == mc_roc_invalid) || (bxid_siecal == mc_roc_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_siecal_BXID.front()->GetEventN()) + " without ECAL ROC(" + ") or BXID("
                  + std::to_string(bxid_siecal) + std::to_string(roc_siecal) + ") in run " + std::to_string(m_que_siecal_BXID.front()->GetRunNumber()),
                  std::cout, std::cerr);
            m_que_siecal_BXID.pop_front();
            continue;
         }
         roc_siecal += m_roc_offset_ahcal;
      }
      if (m_que_bif_BXID.empty()) {
         if (m_active_bif) return; //more will come
      } else {
         roc_bif = m_que_bif_BXID.front()->GetTag("ROC", mc_roc_invalid);
         bxid_bif = m_que_bif_BXID.front()->GetTag("BXID", mc_bxid_invalid);
         if ((roc_bif == mc_roc_invalid) || (bxid_bif == mc_bxid_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_bif_BXID.front()->GetEventN()) + " without BIF ROC(" + ") or BXID("
                  + std::to_string(bxid_bif) + std::to_string(roc_bif) + ") in run " + std::to_string(m_que_bif_BXID.front()->GetRunNumber()), std::cout,
                  std::cerr);
            m_que_bif_BXID.pop_front();
            continue;
         }
         roc_bif += m_roc_offset_bif;
      }

      // at this point all producers have something in the buffer (if possible)
      bool present_ahcal = false;
      bool present_siecal = false;
      bool present_bif = false;
      bool present_desytable = false; //whether event is present in the merged event

      int processedRoc = mc_roc_invalid;
      if (roc_ahcal < processedRoc) processedRoc = roc_ahcal;
      if (roc_siecal < processedRoc) processedRoc = roc_siecal;
      if (roc_bif < processedRoc) processedRoc = roc_bif;

      int processedBxid = mc_bxid_invalid;
      if ((roc_ahcal == processedRoc) && (bxid_ahcal < processedBxid)) processedBxid = bxid_ahcal;
      if ((roc_bif == processedRoc) && (bxid_bif < processedBxid)) processedBxid = bxid_bif;
      if ((roc_siecal == processedRoc) && (bxid_siecal < processedBxid)) processedBxid = bxid_siecal;
//      int processedBxid = mc_bxid_invalid;
//      if ((roc_ahcal == processedRoc) && (bxid_ahcal < processedBxid)) processedBxid = bxid_ahcal;
//      if ((roc_bif == processedRoc) && (bxid_bif < processedBxid)) processedBxid = bxid_bif;

      // std::cout << "Trying to put together event with ROC=" << processedRoc << " BXID=" << processedBxid;
      // std::cout << "\tAHCALROC=" << roc_ahcal << ",AHCALBXID=" << bxid_ahcal << "\tBIFROC=" << roc_bif << ",BIFBXID=" << bxid_bif;
      // std::cout << "\tH1ROC=" << roc_hodoscope1 << ",H1BXID=" << bxid_hodoscope1;
      // std::cout << "\tH2ROC=" << roc_hodoscope2 << ",H2BXID=" << bxid_hodoscope2 << std::endl;
      auto ev_sync = eudaq::Event::MakeUnique("CaliceBxid");
      ev_sync->SetFlagPacket();
      uint64_t timestampBegin, timestampEnd;
      //Reordered: BIF might need to go first, as it overwrites the slcio timestamp
      if ((roc_bif == processedRoc) && (bxid_bif == processedBxid)) {
         if (!m_que_bif_BXID.empty()) {
            ev_sync->AddSubEvent(std::move(m_que_bif_BXID.front()));
            m_que_bif_BXID.pop_front();
            present_bif = true;
         }
      }
      if ((roc_siecal == processedRoc) && (bxid_siecal == processedBxid)) {
         if (!m_que_siecal_BXID.empty()) {
            ev_sync->AddSubEvent(std::move(m_que_siecal_BXID.front()));
            m_que_siecal_BXID.pop_front();
            present_siecal = true;
         }
      }
      if (!m_que_desytable.empty()) {
         ev_sync->AddSubEvent(std::move(m_que_desytable.front()));
         m_que_desytable.pop_front();
         present_desytable = true;
      }
      //Reordered: AHCAL should go last: the last events overwrites the timestamp in slcio
      if ((roc_ahcal == processedRoc) && (bxid_ahcal == processedBxid)) {
         if (!m_que_ahcal_BXID.empty()) {
            timestampBegin = m_que_ahcal_BXID.front()->GetTimestampBegin();
            timestampEnd = m_que_ahcal_BXID.front()->GetTimestampEnd();
            ev_sync->SetTimestamp(timestampBegin, timestampEnd);
            ev_sync->AddSubEvent(std::move(m_que_ahcal_BXID.front()));
            m_que_ahcal_BXID.pop_front();
            present_ahcal = true;
         }
      }
      m_thrown_incomplete += 1; //increase in case the loop is exit in following lines
      if (m_evt_mandatory_ahcal && (!present_ahcal) && (m_active_ahcal)) continue; //throw awway incomplete event
      if (m_evt_mandatory_siecal && (!present_siecal) && (m_active_siecal)) continue; //throw awway incomplete event
      if (m_evt_mandatory_bif && (!present_bif) && (m_active_bif)) continue; //throw awway incomplete event
      m_thrown_incomplete -= 1; //and decrease back.
      ev_sync->SetEventN(m_ev_n++);
      if (!m_disable_print) ev_sync->Print(std::cout);
      WriteEvent(std::move(ev_sync));
   }
}
