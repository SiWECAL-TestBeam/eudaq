#include "eudaq/DataCollector.hh"
#include <mutex>
#include <deque>
#include <map>
#include <set>
#include <math.h>       /* fabs */

#define debug 0

class CaliceSiECALBxidDataCollector:public eudaq::DataCollector{
public:
  CaliceSiECALBxidDataCollector(const std::string &name,
		      const std::string &runcontrol);

  void DoStartRun() override;
  void DoConfigure() override;
  void DoConnect(eudaq::ConnectionSPC id) override;
  void DoDisconnect(eudaq::ConnectionSPC id) override;
  void DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev) override;


  static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceSiECALBxidDataCollector");
private:
  uint64_t m_ts_bore_siecal;
  std::deque<eudaq::EventSPC> m_que_siecal;

  int retrigger_th=3;
  int bcid_th_siecal=2;
  
  // datablocks BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
  std::map<int,std::vector<std::vector<int>> > BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
  std::vector<std::vector<int>> FilterSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal);
 
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::DataCollector>::
    Register<CaliceSiECALBxidDataCollector, const std::string&, const std::string&>
    (CaliceSiECALBxidDataCollector::m_id_factory);
}

CaliceSiECALBxidDataCollector::CaliceSiECALBxidDataCollector(const std::string &name,
					 const std::string &runcontrol):
  DataCollector(name, runcontrol),m_ts_bore_siecal(0){
  
}



void CaliceSiECALBxidDataCollector::DoConnect(eudaq::ConnectionSPC id){
  std::cout<<"new producer connection: "<<id;
  std::string name = id->GetName();
  if(name!="SiWECALProducer")
    EUDAQ_WARN("unsupported producer is connecting.");
}

void CaliceSiECALBxidDataCollector::DoDisconnect(eudaq::ConnectionSPC id){
  std::cout<<"disconnect producer connection: "<<id;
}


void CaliceSiECALBxidDataCollector::DoStartRun(){
  m_que_siecal.clear();
}

void CaliceSiECALBxidDataCollector::DoConfigure(){
  //Nothing to do
}

void CaliceSiECALBxidDataCollector::DoReceive(eudaq::ConnectionSPC id, eudaq::EventSP ev){


  //how do we deal with BORE s ?
  if(id->GetName() == "SiWECALProducer"){
    //if(ev->IsBORE())
    //  m_ts_bore_cal = ev->GetTag("ROCStartTS", m_ts_bore_cal);
    m_que_siecal.push_back(ev);
  } else{
    EUDAQ_WARN("Event from unknown producer");
  }  

  //  std::cout<<"  ---------- NAME "<<id->GetName()<<std::endl;
  // while(!m_que_bif.empty() && !m_que_cal.empty()){
  while(!m_que_siecal.empty()){

    auto ev_siecal = m_que_siecal.front();
    
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
	auto ev_sub_siecal =  eudaq::Event::MakeUnique("CaliceObject");
	ev_sub_siecal->SetTag("SiECAL_ROC",ev_siecal->GetTag("ROC", -1));
	ev_sub_siecal->SetTag("SiECAL_NSLBs",ev_siecal->GetTag("NSLBs", -1));
	//ev_sub_siecal->SetTag("SiECAL_StartAcqTime",ev_siecal->GetTag("StartAcqTime", -1)); // DOESN'T WORK -- to be checked
	ev_sub_siecal->SetTag("SiECAL_BXID",it->first);
	//copy all slowcontrol and info blocks
	for(int iblock=0; iblock<(4+nslabs); iblock++) ev_sub_siecal->AddBlock(iblock,ev_siecal->GetBlock(iblock));
	for(int i=0; i<it->second.size(); i++) {
	  ev_sub_siecal->AddBlock(ev_sub_siecal->NumBlocks(),it->second.at(i));
	}
	WriteEvent(std::move(ev_sub_siecal));
      }
    }
    m_que_siecal.pop_front();

    //WriteEvent(ev);

  }
  if(m_que_siecal.size() > 1000 ){
    EUDAQ_WARN("m_que_siecal.size()  > 1000");
  }
}

std::map<int, std::vector<std::vector<int>>> CaliceSiECALBxidDataCollector::BuildSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal) {
  
 std::map<int, std::vector<std::vector<int>>> sorted_bybcid;
  //INFO
  if(debug==1) std::cout<<"BuildSiECAL"<<" "<<vector_of_blocks_siecal.size()<<std::endl;
  //info of data structure
  //"i:CycleNr,i:BunchXID,i:sca,i:Layer,i:SkirocID,i:NChannels,i:hit_low[NC],i:gain_low[NC],ADC_low[NC],i:hit_high[NC],i:gain_high[NC],ADC_high[NC]";

  //******************
  //First I filter the retrigger events
  std::vector<std::vector<int>> vector_of_blocks_siecal_filtered=FilterSiECAL(vector_of_blocks_siecal);//result
  if(debug==1) if(vector_of_blocks_siecal.size()!=vector_of_blocks_siecal_filtered.size()) std::cout<< "RETRIGGER FOUND "<<std::endl;

  
  //** SECOND the EVENT BUILDING PART

  //I create a map of bcid,datablocks with a merging of +-1, after filtering for retriggers
  // 1) I check the first bcid that I find, later I check the rest of blocks and if I have a bcid+-1, I added to the bcid and remove those elements
  if(vector_of_blocks_siecal_filtered.size()==0) return sorted_bybcid;
  
  for(int i=0; i<(vector_of_blocks_siecal_filtered.size()); i++) {
    
    int bcid_ref=vector_of_blocks_siecal_filtered.at(i).at(1);

    // WORK IN PROGRESS
    // naive solution so far... to be improved
    std::map<int,std::vector<std::vector<int>> > ::iterator it;
    it=sorted_bybcid.find(bcid_ref);

    if(it == sorted_bybcid.end()) {
      std::vector<std::vector<int>> new_vector_of_blocks;
      new_vector_of_blocks.push_back(vector_of_blocks_siecal_filtered.at(i));
      sorted_bybcid[bcid_ref]=new_vector_of_blocks;
    } else {
      sorted_bybcid[bcid_ref].push_back(vector_of_blocks_siecal_filtered.at(i));
    }
    //cluster in bcid_ref all bcid+-bcid_th_siecal
    for(int j=i+1; j<vector_of_blocks_siecal_filtered.size(); j++) {
      int bcid_ref2=vector_of_blocks_siecal_filtered.at(i).at(1);
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
    sorted_bybcid2[bcid_av]=it->second;
  }
  
  return sorted_bybcid2;
  
}

std::vector<std::vector<int>> CaliceSiECALBxidDataCollector::FilterSiECAL(std::vector<std::vector<int>> vector_of_blocks_siecal) {

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
      } else {
	bcid_scaminus1=it->second.at(i-1).at(1);
	if( fabs(bcid_scaplus1-bcid)<retrigger_th ) {
	  badbcid[i]=1;
	  badbcid[i-1]=1;
	}
      }

      if(debug==1) std::cout<<"BADBCID:"<<badbcid[i]<<std::endl;
      
    }
    for(int i=0; i<it->second.size(); i++) {
      if(badbcid[i]==0) {
      
	vector_of_blocks_siecal_filtered.push_back(it->second.at(i));//vect2);
	int n=vector_of_blocks_siecal_filtered.size();
	//	for(int j=0; j<vector_of_blocks_siecal_filtered.at(n-1).size(); j++)
	//  std::cout<<vector_of_blocks_siecal_filtered.at(n-1).at(j)<<" ";
	//std::cout<<std::endl;
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

 
