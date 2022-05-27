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
#include "eudaq/Configuration.hh"
#include "eudaq/Utils.hh"
#include "eudaq/Platform.hh"
#include "eudaq/Factory.hh"

using namespace eudaq;
using DataCollectorSP = Factory<DataCollector>::SP_BASE;

class CaliceROCDataCollector: public eudaq::DataCollector {
   public:
      CaliceROCDataCollector(const std::string &name, const std::string &runcontrol);
      void DoStartRun() override;
      void DoConfigure() override;
      void DoConnect(eudaq::ConnectionSPC id) override;
      void DoDisconnect(eudaq::ConnectionSPC id) override;
      void DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev) override;

      static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceROCDataCollector");

   private:
      void AddEvent_TimeStamp(uint32_t id, eudaq::EventSPC ev);
      void BuildEvent_roc();

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
      std::deque<eudaq::EventSPC> m_que_ahcal;
      std::deque<eudaq::EventSPC> m_que_ecal;
      std::deque<eudaq::EventSPC> m_que_bif;
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
         Register<CaliceROCDataCollector, const std::string&, const std::string&>
         (CaliceROCDataCollector::m_id_factory);
}

CaliceROCDataCollector::CaliceROCDataCollector(const std::string &name, const std::string &runcontrol) :
      DataCollector(name, runcontrol) {
}

void CaliceROCDataCollector::DoStartRun() {
   m_que_ahcal.clear();
   m_que_ecal.clear();
   m_que_bif.clear();
   m_que_desytable.clear();

   m_ts_end_last_cal = 0;
   m_ts_end_last_bif = 0;
   m_offset_ahcal2bif_done = false;
   m_ts_offset_ahcal2bif = 0;
   m_ev_n = 0;
   m_thrown_incomplete = 0;

}

void CaliceROCDataCollector::DoConfigure() {
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
}

void CaliceROCDataCollector::DoConnect(eudaq::ConnectionSPC idx) {
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

void CaliceROCDataCollector::DoDisconnect(eudaq::ConnectionSPC idx) {
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

void CaliceROCDataCollector::DoReceive(eudaq::ConnectionSPC idx, eudaq::EventSP ev) {
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
      m_que_ahcal.push_back(std::move(ev));
   else if (con_name == mc_name_ecal)
      m_que_ecal.push_back(std::move(ev));
   else if (con_name == mc_name_bif)
      m_que_bif.push_back(std::move(ev));
   else if (con_name == mc_name_desytable)
      m_que_desytable.push_back(std::move(ev));
   else {
      EUDAQ_WARN("Receive event from unkonwn Producer");
      return;
   }

   //quit if not enough events to merge
   if (m_que_ahcal.empty() && m_active_ahcal) return;
   if (m_que_ecal.empty() && m_active_ecal) return;
   if (m_que_bif.empty() && m_active_bif) return;
// std::cout<<"p 1 \n";
 // if (!m_disable_print) ev->Print(std::cout);

   BuildEvent_roc();
}

//DataCollectorSP DataCollector::Make(const std::string &code_name,
//      const std::string &run_name,
//      const std::string &runcontrol) {
//   return Factory<DataCollector>::
//   MakeShared<const std::string&, const std::string&>
//   (eudaq::str2hash(code_name), run_name, runcontrol);
//}

inline void CaliceROCDataCollector::BuildEvent_roc() {
   while (true) {
      SetStatusTag("Queue", std::string("(") + std::to_string(m_que_ahcal.size())
            + "," + std::to_string(m_que_ecal.size())
            + "," + std::to_string(m_que_bif.size())
            //            + "," + std::to_string(m_que_hodoscope1.size())
            //            + "," + std::to_string(m_que_hodoscope2.size())
            + "," + std::to_string(m_que_desytable.size()) + ")");
      if (std::chrono::system_clock::now() - lastprinttime > std::chrono::milliseconds(5000)) {
         lastprinttime = std::chrono::system_clock::now();
         std::cout << "Queue sizes: ";
         if (m_active_ahcal) std::cout << "AHCAL:" << m_que_ahcal.size();
         if (m_active_ecal) std::cout << " ECAL:" << m_que_ecal.size();
         if (m_active_bif) std::cout << " BIF:" << m_que_bif.size();
         if (m_active_desytable) std::cout << " table:" << m_que_desytable.size();
         std::cout << std::endl;
      }
//      int bxid_ahcal = mc_bxid_invalid;
//      int bxid_bif = mc_bxid_invalid;
      int roc_ahcal = mc_roc_invalid;
      int roc_ecal = mc_roc_invalid;
      int roc_bif = mc_roc_invalid;
//      const eudaq::EventSP ev_front_cal;
//      const eudaq::EventSP ev_front_bif;

//      uint64_t ts_beg_bif = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_end_bif = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_beg_cal = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_end_cal = UINT64_MAX; //timestamps in 0.78125 ns steps
//      uint64_t ts_beg = UINT64_C(0); //timestamps in 0.78125 ns steps
//      uint64_t ts_end = UINT64_C(0); //timestamps in 0.78125 ns steps

      if (m_que_ahcal.empty()) {
         if (m_active_ahcal) return; //more will come
      } else {
         roc_ahcal = m_que_ahcal.front()->GetTag("ROC", mc_roc_invalid);
         if ((roc_ahcal == mc_roc_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_ahcal.front()->GetEventN()) + " without AHCAL ROC("
                  + std::to_string(roc_ahcal) + ") in run " + std::to_string(m_que_ahcal.front()->GetRunNumber()), std::cout, std::cerr);
            m_que_ahcal.pop_front();
            continue;
         }
         roc_ahcal += m_roc_offset_ahcal;
      }
      if (m_que_ecal.empty()) {
         if (m_active_ecal) return; //more will come
      } else {
         roc_ecal = m_que_ecal.front()->GetTag("ROC", mc_roc_invalid);
         if ((roc_ecal == mc_roc_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_ecal.front()->GetEventN()) + " without ECAL ROC("
                  + std::to_string(roc_ecal) + ") in run " + std::to_string(m_que_ecal.front()->GetRunNumber()), std::cout, std::cerr);
            m_que_ecal.pop_front();
            continue;
         }
         roc_ecal += m_roc_offset_ahcal;
      }
      if (m_que_bif.empty()) {
         if (m_active_bif) return; //more will come
      } else {
         roc_bif = m_que_bif.front()->GetTag("ROC", mc_roc_invalid);
         if ((roc_bif == mc_roc_invalid)) {
            EUDAQ_WARN_STREAMOUT("event " + std::to_string(m_que_bif.front()->GetEventN()) + " without BIF ROC(" + std::to_string(roc_bif)
                  + ") in run " + std::to_string(m_que_bif.front()->GetRunNumber()), std::cout, std::cerr);
            m_que_bif.pop_front();
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

//      int processedBxid = mc_bxid_invalid;
//      if ((roc_ahcal == processedRoc) && (bxid_ahcal < processedBxid)) processedBxid = bxid_ahcal;
//      if ((roc_bif == processedRoc) && (bxid_bif < processedBxid)) processedBxid = bxid_bif;

      // std::cout << "Trying to put together event with ROC=" << processedRoc << " BXID=" << processedBxid;
      // std::cout << "\tAHCALROC=" << roc_ahcal << ",AHCALBXID=" << bxid_ahcal << "\tBIFROC=" << roc_bif << ",BIFBXID=" << bxid_bif;
      // std::cout << "\tH1ROC=" << roc_hodoscope1 << ",H1BXID=" << bxid_hodoscope1;
      // std::cout << "\tH2ROC=" << roc_hodoscope2 << ",H2BXID=" << bxid_hodoscope2 << std::endl;
      auto ev_sync = eudaq::Event::MakeUnique("CaliceCommonObject");
      //ev_sync->SetFlagPacket();
      uint64_t timestampBegin, timestampEnd;
      //Reordered: BIF might need to go first, as it overwrites the slcio timestamp
      if (roc_bif == processedRoc) {
         if (!m_que_bif.empty()) {
            ev_sync->AddSubEvent(std::move(m_que_bif.front()));
            m_que_bif.pop_front();
            present_bif = true;
         }
      }
      if (roc_ecal == processedRoc) {
         if (!m_que_ecal.empty()) {
            ev_sync->AddSubEvent(std::move(m_que_ecal.front()));
            m_que_ecal.pop_front();
            present_ecal = true;
         }
      }
      if (!m_que_desytable.empty()) {
         ev_sync->AddSubEvent(std::move(m_que_desytable.front()));
         m_que_desytable.pop_front();
         present_desytable = true;
      }
      //Reordered: AHCAL should go last: the last events overwrites the timestamp in slcio
      if (roc_ahcal == processedRoc) {
         if (!m_que_ahcal.empty()) {
            timestampBegin = m_que_ahcal.front()->GetTimestampBegin();
            timestampEnd = m_que_ahcal.front()->GetTimestampEnd();
            ev_sync->SetTimestamp(timestampBegin, timestampEnd);
            ev_sync->AddSubEvent(std::move(m_que_ahcal.front()));
            m_que_ahcal.pop_front();
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
