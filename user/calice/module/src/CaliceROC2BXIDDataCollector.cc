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

      void AhcalRoc2Bxid(std::deque<eudaq::EventSPC> &queue_roc, std::deque<eudaq::EventSPC> &queue_bxid);
      std::vector<uint32_t> vectorU8toU32(const std::vector<uint8_t> &data);
      inline int ldaTS2BXID(const uint64_t triggerTS, const uint64_t startTS, const int bxid0TSOffer, const int bxidLength);

      int m_ldabxid0offset = 285;
      int m_ahcalBxidLength = 8;
      int m_require_LDAtrigger = 0;

      std::mutex m_mutex;
      //to be edited according to the name of the processes. TODO implement as parameter to the configuration
      std::string mc_name_ahcal = "AHCAL1";
      std::string mc_name_ecal = "ECAL1";
      std::string mc_name_bif = "BIF1";
      std::string mc_name_desytable = "desytable1";

      //correction factor to be added to the ROC tag. This can eliminate differences between starting ROC numbers in different producer - some can start from 1, some from 0
      int m_roc_offset_ahcal = 0;
      int m_roc_offset_ecal = 0;
      int m_roc_offset_bif = 0;

      static const int mc_roc_invalid = INT32_MAX;
      static const int mc_bxid_invalid = INT32_MAX;

      //ts
      std::deque<eudaq::EventSPC> m_que_ahcal_ROC; //Queue for AHCAL ROC events
      std::deque<eudaq::EventSPC> m_que_ahcal_BXID; //Queue for AHCAL BXID events
      std::deque<eudaq::EventSPC> m_que_ecal_ROC;
      std::deque<eudaq::EventSPC> m_que_ecal_BXID;
      std::deque<eudaq::EventSPC> m_que_bif_ROC;
      std::deque<eudaq::EventSPC> m_que_bif_BXID;
      std::deque<eudaq::EventSPC> m_que_desytable;
      bool m_active_ahcal = false;
      bool m_active_ecal = false;
      bool m_active_bif = false;
      bool m_active_desytable = false;

      //final test whether the event will be thrown away if a mandatory subevents are not present
      bool m_evt_mandatory_ahcal = true;
      bool m_evt_mandatory_ecal = true;
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
   m_que_ecal_ROC.clear();
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
   if (idx->GetName() == mc_name_ecal) {
      m_active_ecal = true;
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
   if (idx->GetName() == mc_name_ecal) m_active_ecal = false;
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
   else if (con_name == mc_name_ecal)
      m_que_ecal_ROC.push_back(std::move(ev));
   else if (con_name == mc_name_bif)
      m_que_bif_ROC.push_back(std::move(ev));
   else if (con_name == mc_name_desytable)
      m_que_desytable.push_back(std::move(ev));
   else {
      EUDAQ_WARN("Receive event from unkonwn Producer");
      return;
   }
   while (!m_que_ahcal_ROC.empty())
      AhcalRoc2Bxid(m_que_ahcal_ROC, m_que_ahcal_BXID);

   //quit if not enough events to merge
   if (m_que_ahcal_BXID.empty() && m_active_ahcal) return;
   if (m_que_ecal_BXID.empty() && m_active_ecal) return;
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

inline void CaliceROC2BCIDDataCollector::BuildEvent_roc_bxid() {

   while (true) {
      SetStatusTag("Queue", std::string("(") + std::to_string(m_que_ahcal_ROC.size())
            + "," + std::to_string(m_que_ecal_ROC.size())
            + "," + std::to_string(m_que_bif_ROC.size())
            //            + "," + std::to_string(m_que_hodoscope1.size())
            //            + "," + std::to_string(m_que_hodoscope2.size())
            + "," + std::to_string(m_que_desytable.size()) + ")");
      if (std::chrono::system_clock::now() - lastprinttime > std::chrono::milliseconds(5000)) {
         lastprinttime = std::chrono::system_clock::now();
         std::cout << "Queue sizes: ";
         if (m_active_ahcal) std::cout << "AHCAL:" << m_que_ahcal_ROC.size();
         if (m_active_ecal) std::cout << " ECAL:" << m_que_ecal_ROC.size();
         if (m_active_bif) std::cout << " BIF:" << m_que_bif_ROC.size();
         if (m_active_desytable) std::cout << " table:" << m_que_desytable.size();
         std::cout << std::endl;
      }
//      int bxid_ahcal = mc_bxid_invalid;
//      int bxid_bif = mc_bxid_invalid;
      int roc_ahcal = mc_roc_invalid;
      int roc_ecal = mc_roc_invalid;
      int roc_bif = mc_roc_invalid;

      int bxid_ahcal = mc_bxid_invalid;
      int bxid_ecal = mc_bxid_invalid;
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
      if (m_que_ecal_BXID.empty()) {
         if (m_active_ecal) return; //more will come
      } else {
         roc_ecal = m_que_ecal_BXID.front()->GetTag("ROC", mc_roc_invalid);
         bxid_ecal = m_que_ecal_BXID.front()->GetTag("BXID", mc_bxid_invalid);
         if ((roc_ecal == mc_roc_invalid) || (bxid_ecal == mc_roc_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_ecal_BXID.front()->GetEventN()) + " without ECAL ROC(" + ") or BXID("
                  + std::to_string(bxid_ecal) + std::to_string(roc_ecal) + ") in run " + std::to_string(m_que_ecal_BXID.front()->GetRunNumber()),
                  std::cout, std::cerr);
            m_que_ecal_BXID.pop_front();
            continue;
         }
         roc_ecal += m_roc_offset_ahcal;
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
      bool present_ecal = false;
      bool present_bif = false;
      bool present_desytable = false; //whether event is present in the merged event

      int processedRoc = mc_roc_invalid;
      if (roc_ahcal < processedRoc) processedRoc = roc_ahcal;
      if (roc_ecal < processedRoc) processedRoc = roc_ecal;
      if (roc_bif < processedRoc) processedRoc = roc_bif;

      int processedBxid = mc_bxid_invalid;
      if ((roc_ahcal == processedRoc) && (bxid_ahcal < processedBxid)) processedBxid = bxid_ahcal;
      if ((roc_bif == processedRoc) && (bxid_bif < processedBxid)) processedBxid = bxid_bif;
      if ((roc_ecal == processedRoc) && (bxid_ecal < processedBxid)) processedBxid = bxid_ecal;
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
      if ((roc_ecal == processedRoc) && (bxid_ecal == processedBxid)) {
         if (!m_que_ecal_BXID.empty()) {
            ev_sync->AddSubEvent(std::move(m_que_ecal_BXID.front()));
            m_que_ecal_BXID.pop_front();
            present_ecal = true;
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
      if (m_evt_mandatory_ecal && (!present_ecal) && (m_active_ecal)) continue; //throw awway incomplete event
      if (m_evt_mandatory_bif && (!present_bif) && (m_active_bif)) continue; //throw awway incomplete event
      m_thrown_incomplete -= 1; //and decrease back.
      ev_sync->SetEventN(m_ev_n++);
      if (!m_disable_print) ev_sync->Print(std::cout);
      WriteEvent(std::move(ev_sync));
   }
}
