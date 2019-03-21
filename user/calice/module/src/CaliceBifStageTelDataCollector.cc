#include "eudaq/DataCollector.hh"
#include "eudaq/Event.hh"

#include <deque>
#include <map>
#include <string>

namespace eudaq {
class CaliceBifStageTelDataCollector: public DataCollector {
   public:
      using DataCollector::DataCollector;
      void DoConfigure() override;
      void DoStartRun() override;
      void DoConnect(ConnectionSPC /*id*/) override;
      void DoDisconnect(ConnectionSPC /*id*/) override;
      void DoReceive(ConnectionSPC id, EventSP ev) override;
      static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceBifStageTelDataCollector");
   private:
      std::map<std::string, std::deque<EventSPC>> m_que_event; //queues for different producers
      std::mutex m_mtx_map;
      bool m_bif_mandatory;
      std::string m_name_desytable;
      std::string m_name_bif;
      std::string m_name_ahcal;
};

namespace {
auto dummy0 = Factory<DataCollector>::Register<CaliceBifStageTelDataCollector, const std::string&, const std::string&>(CaliceBifStageTelDataCollector::m_id_factory);
}

void CaliceBifStageTelDataCollector::DoConfigure() {
   std::unique_lock<std::mutex> lk(m_mtx_map);
   auto conf = GetConfiguration();
   if (conf) {
      conf->Print();
   }
   int MandatoryBif = conf->Get("MandatoryBif", 0); //this will break the event numbering
   m_bif_mandatory = (MandatoryBif == 1) ? true : false;
   std::cout << "#MandatoryBif=" << MandatoryBif << std::endl;
   m_name_desytable = "desytable1";
   m_name_bif = "BIF1";
   m_name_ahcal = "AHCAL1";
}

void CaliceBifStageTelDataCollector::DoStartRun() {
   std::unique_lock<std::mutex> lk(m_mtx_map);
   for (auto &que : m_que_event) {
      que.second.clear();
   }
}

void CaliceBifStageTelDataCollector::DoConnect(ConnectionSPC id) {
   std::unique_lock<std::mutex> lk(m_mtx_map);
   std::string pdc_name = id->GetName();
   EUDAQ_INFO("Producer." + pdc_name + " is connecting");
   if (m_que_event.find(pdc_name) != m_que_event.end()) {
      EUDAQ_WARN_STREAMOUT("DataCollector::DoConnect, multiple producers are sharing a same name", std::cout, std::cerr);
   }
   m_que_event[pdc_name].clear();
}

void CaliceBifStageTelDataCollector::DoDisconnect(ConnectionSPC id) {
   std::unique_lock<std::mutex> lk(m_mtx_map);
   std::string pdc_name = id->GetName();
   if (m_que_event.find(pdc_name) == m_que_event.end())
   EUDAQ_THROW("DataCollector::DisDoconnect, the disconnecting producer was not existing in list");
   EUDAQ_WARN("Producer." + pdc_name + " is disconnected, the remaining events are erased. (" + std::to_string(m_que_event[pdc_name].size()) + " Events)");
   m_que_event.erase(pdc_name);
}

void CaliceBifStageTelDataCollector::DoReceive(ConnectionSPC id, EventSP ev) {
   std::unique_lock<std::mutex> lk(m_mtx_map);
   std::string pdc_name = id->GetName();
   m_que_event[pdc_name].push_back(std::move(ev));

   static const int mc_roc_invalid = INT32_MAX;
   static const int mc_bxid_invalid = INT32_MAX;
   int32_t bxid_ahcal = mc_bxid_invalid;
   int32_t bxid_bif = mc_bxid_invalid;
   int32_t roc_ahcal = mc_roc_invalid;
   int32_t roc_bif = mc_roc_invalid;
   //throw away unmatched ahcal/bif events
   while (true) {
      //keep checking whether there are events, even after throwing out
      if (m_que_event[m_name_ahcal].size() == 0) return;
      if (m_que_event[m_name_bif].size() == 0) return;
      roc_ahcal = m_que_event[m_name_ahcal].front()->GetTag("ROC", mc_roc_invalid);
      bxid_ahcal = m_que_event[m_name_ahcal].front()->GetTag("BXID", mc_bxid_invalid);
      roc_bif = m_que_event[m_name_bif].front()->GetTag("ROC", mc_roc_invalid);
      bxid_bif = m_que_event[m_name_bif].front()->GetTag("BXID", mc_bxid_invalid);
      if ((roc_ahcal == mc_roc_invalid) | (bxid_ahcal == mc_bxid_invalid)) break; //this event doesn't have a bxid and roc. dummy event for example
      if ((roc_bif == mc_roc_invalid) | (bxid_bif == mc_bxid_invalid)) {
         m_que_event[m_name_bif].pop_front(); //not treating invalid BIF events
         break; //this event doesn't have a bxid and roc. dummy event for example
      }
      if (((roc_ahcal == roc_bif) & (bxid_bif == bxid_ahcal))) break; //successful match in any case
      if ((roc_bif < roc_ahcal) | ((roc_ahcal == roc_bif) & (bxid_bif < bxid_ahcal))) { //bif event is from past and has no matching ahcal event
         m_que_event[m_name_bif].pop_front(); //standalone BIF event is never wanted
         //std::cout << "Removing BIF ROC=" << roc_bif << ", BXID=" << bxid_bif << ". AHCAL roc=" << roc_ahcal << ",bxid=" << bxid_ahcal << std::endl;
         continue;
      }
      //Now bif event can only be from the future.
      if (m_bif_mandatory) {
         std::cout << "Removing AHCAL ROC=" << roc_ahcal << ", BXID=" << bxid_ahcal << ". BIF roc=" << roc_bif << ",bxid=" << bxid_bif << std::endl;
         m_que_event[m_name_ahcal].pop_front(); //no matching bif event for ahcal -> remove it
         continue;
      }
      break; //bif event is from the future. doesn't matter if bif is not mandatory
   }

   uint32_t n = 0; //count number of queues, from which an event can be merged
   for (auto &que : m_que_event) {
      if (que.first.compare(m_name_desytable) == 0) { //desy table is asynchronous and doesn't have to be in the merge event
         n++;
         continue;
      }
      if (!que.second.empty()) n++;
   }
   if (n != m_que_event.size()) return;

   auto ev_wrap = Event::MakeUnique(GetFullName());
   ev_wrap->SetFlagPacket();
   uint32_t ev_c = m_que_event[m_name_ahcal].front()->GetEventN();
   bool match = true;
   for (auto &que : m_que_event) {
      if (que.first.compare(m_name_desytable) == 0) {
         ev_wrap->AddSubEvent(que.second.front());
         if (que.second.size() > 1) que.second.pop_front(); //desy table event are deleted only if there is a new coordinate
         continue;
      }
      if (que.first.compare(m_name_bif) == 0) {
         if ((roc_ahcal == roc_bif) & (bxid_ahcal == bxid_bif)) {
            ev_wrap->AddSubEvent(que.second.front());
            que.second.pop_front();
         }
         continue;
      }
      //only TLU, ahcal and mimosa. DESYTABLE and BIF must not compare the eventnumber
      if (ev_c != que.second.front()->GetEventN()) match = false;
      ev_wrap->AddSubEvent(que.second.front());
      que.second.pop_front();
   }
   if (!match) {
      EUDAQ_ERROR("EventNumbers are Mismatched");
   }
   WriteEvent(std::move(ev_wrap));
}
}
