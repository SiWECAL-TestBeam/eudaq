// SiReader.cc
#include "eudaq/Event.hh"
#include "SiReader.hh"
#include "SiWECALProducer.hh"

#include "eudaq/Logger.hh"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

using namespace eudaq;
using namespace std;
using std::cout;
using std::endl;
using std::dec;
using std::hex;

namespace eudaq {

  SiReader::SiReader(SiWECALProducer *r) :
    SiWECALReader(r), _runNo(-1), _buffer_inside_acquisition(false), _cycleNo(0),
    _unfinishedPacketState(UnfinishedPacketStates::DONE), length(0) {
  }

  SiReader::~SiReader() {
  }

  void SiReader::OnStart(int runNo) {
    _runNo = runNo;
    _cycleNo = -1;
    //      _tempmode = false;
    //    cycleData.resize(6);
    //    _LDATimestampData.clear();
    _RunTimesStatistics.clear();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    initRawFrame(true);
    
    // set the connection and send "start runNo"
    cout << "opening connection" << endl;
    _producer->OpenConnection();
    cout << "connection opened" << endl;
    // using characters to send the run number
    ostringstream os;
    os << "RUN_START"; //newLED
    // os << "START"; //newLED
    os.width(8);
    os.fill('0');
    os << runNo;
    os << "\r\n";
    cout << "Sending command" << endl;
    _producer->SendCommand(os.str().c_str());
    cout << "command sent" << endl;
    _buffer_inside_acquisition = false;
  }

  //newLED
  void SiReader::OnConfig(std::string msg) {

    
    if (!msg.empty()) {
      cout << " opening OnConfigCheck send msg:" <<msg<< endl;
      bool connected = _producer->OpenConnection();
      ostringstream os;
      os << "RUN_CONFIGURE"; //newLED
      os << msg;
      os << "\r\n";
      cout << "Sending command" << endl;
      _producer->SendCommand(os.str().c_str());
      cout << "command sent" << endl;
      if (connected) {
	_producer->SendCommand(os.str().c_str());
	cout << " wait 2s OnConfigCheck " << endl;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	cout << " Start CloseConnection OnConfigCheck " << endl;
	_producer->CloseConnection();
	cout << " End CloseConnection OnConfigCheck " << endl;
      } else {
	cout << " connexion failed, try configurating again"
	     << endl;
      }
    } else {
      cout << " opening OnConfigCheck with no message" << endl;
      bool connected = _producer->OpenConnection();
      ostringstream os;
      os << "HELLO"; //newLED
      os.width(8);
      os.fill('0');
      os << 0;
      os << "\r\n";
      cout << "Sending command" << endl;
      _producer->SendCommand(os.str().c_str());
      cout << "command sent" << endl;

      if(connected) {
	cout << " OpenConnection success, proceed to close it" << endl;
	cout << " Start CloseConnection OnConfigCheck " << endl;
	_producer->CloseConnection();
	cout << " End CloseConnection OnConfigCheck " << endl;
      } else cout << " OpenConnection FAILED, try configurating again" << endl;

    }
    cout << " ###################################################  "
	 << endl;
    cout << " SYSTEM READY " << endl;
    cout << " ###################################################  "
	 << endl;

  }

  void SiReader::OnStop(int waitQueueTimeS) {
    cout << "SiREader::OnStop sending STOP command" << endl;
    const char *msg = "STOP\r\n";
    _producer->SendCommand(msg);
    cout << "SiREader::OnStop before going to sleep()" << endl;
    std::this_thread::sleep_for(std::chrono::seconds(waitQueueTimeS));
    cout << "SiREader::OnStop after sleep()... " << endl;
    // _producer->CloseConnection();
    _RunTimesStatistics.clear();
    
  }
  
  void SiReader::initRawFrame(bool first) {

    if(first==true) {
      previous_cycleID=-1;
      firstdummy=false;
      slabAdd=-1;
    }
    //coreDaughterIndex=-1;
    chipId=-1;
    asuIndex=-1; 
    //slabIndex=-1;
    skirocIndex=-1;
    nbOfSingleSkirocEventsInFrame=0;
    frame=0; n=0 ; i=0;
    rawData=0;
    index=0;
    channel=0;
    rawValue=-1;
    //slabAdd=-1;
    trailerWord=0;
    rawTSD =-1;
    rawAVDD0=-1; rawAVDD1=-1;
    cycleID=-1;
    transmitID=-1;
    startAcqTimeStamp=-1;
    temperature=0; // in °C
    AVDD0 = 0.0; // in Volts
    AVDD1 = 0.0; // in Volts
    sca=-1;
    for(int j=0; j<SINGLE_SKIROC_EVENT_SIZE; j++) {
      bcid[j]=0;
      for(int i=0; i<2; i++) {
	for(int k=0; k<NB_OF_CHANNELS_IN_SKIROC; k++) {
	  chargevalue[i][j][k]=0;
	  gainvalue[i][j][k]=0;
	  hitvalue[i][j][k]=0;
	}
      }
    }
    valid_frame=0;

  }

  
  int SiReader::cycleIDDecoding(std::vector<unsigned char> ucharValFrameVec ) {

    i=0;
    n=0;
    // metadata
    int result=0;
    for(n= 0; n < 16; n++)
      {
	result += ((unsigned int)(((ucharValFrameVec.at(2*n+1+2)& 0xC0)>> 6) << (30-2*i)));
	i++;
      }
    if(_debug) std::cout<<"cycleIDDecoding:"<<dec<<result<<std::endl;
    i=0;
    n=0;
    
    return result;
  }
  
  void SiReader::DecodeRawFrame(std::vector<unsigned char> ucharValFrameVec) {

    initRawFrame(false);

    coreDaughterIndex=0;//buf[5];// hard coded!!1 where is the coreDaughter Index information ?? IMPORTANT to be checked
    slabAdd=ucharValFrameVec.at(0);
    slabIndex=ucharValFrameVec.at(1);
    ucharValFrameVec.erase (ucharValFrameVec.begin());
    ucharValFrameVec.erase (ucharValFrameVec.begin());

    datasize=ucharValFrameVec.size();
    
    if(_ASCIIOUT)cout<<" New DECODERAWFRAME "<<endl;
    chipId = ucharValFrameVec.at(datasize -2-2);
    if(_debug)  std::cout<<"chipId:"<<dec<<chipId<<std::endl;
    if(_debug) std::cout<<"AsuIndex:"<<dec<<(int)(chipId/NB_OF_SKIROCS_PER_ASU)<<std::endl;
    asuIndex = (int)(chipId/NB_OF_SKIROCS_PER_ASU);
    if(_debug) std::cout<<"SkirocIndex:"<<dec<<chipId - asuIndex*NB_OF_SKIROCS_PER_ASU<<std::endl;
    skirocIndex = chipId -asuIndex*NB_OF_SKIROCS_PER_ASU;
    
    nbOfSingleSkirocEventsInFrame =  (int)((datasize-2-2)/SINGLE_SKIROC_EVENT_SIZE);
    if(_debug) std::cout<<"nbOfSingleSkirocEventsInFrame: "<<dec<<nbOfSingleSkirocEventsInFrame<<std::endl;

    cycleID  = 0;
    transmitID = 0;
    startAcqTimeStamp = 0;
    rawTSD = 0;
    rawAVDD0 = 0;
    rawAVDD1 = 0;
    temperature = 0.0; // in °C
    AVDD0 = 0.0; // in Volts
    AVDD1 = 0.0; // in Volts
    i=0;
    n=0;
    

  // metadata
  for(n= 0; n < 16; n++)
    {
      cycleID += ((unsigned int)(((ucharValFrameVec.at(2*n+1)& 0xC0)>> 6) << (30-2*i)));
      i++;
    }
      if(_debug) std::cout<<"cycleID:"<<dec<<cycleID<<std::endl;
	    
  i=0;
  for(n= 16; n < 32; n++)
    {
      startAcqTimeStamp += ((unsigned int)(((ucharValFrameVec.at(2*n+1)& 0xC0)>> 6) << (30-2*n)));
      i++;
   }
      if(_debug) std::cout<<"startAcqTimeStamp:"<<dec<<startAcqTimeStamp<<std::endl;

  // TSD Value  12 bits value
  i=0; 	
  for(n= 32; n < 38; n++)
    {
		
      rawTSD += ((unsigned int)(((ucharValFrameVec.at(2*n+1)& 0xC0)>> 6) << (10-2*i)));
      i++;
    }

      if(_debug) std::cout<<"rawTSD:"<<dec<<rawTSD<<std::endl;
	    
      temperature = -0.00035207* pow((float)rawTSD, 2)+2.102526*rawTSD-2946.38;
  // AVDD0 value
	    
  i=0; 	
  for(n= 38; n < 44; n++)
    {
		
      rawAVDD0 += ((unsigned int)(((ucharValFrameVec.at(2*n+1)& 0xC0)>> 6) << (10-2*i)));
      i++;

    }
  if(_debug) std::cout<<"rawAVDD0:"<<dec<<rawAVDD0<<std::endl;

  AVDD0 = 1.212766*(rawAVDD0*3.3)/4095.0;
	    
  // AVDD1 value
  i=0; 	
  for(n= 44; n < 50; n++)
    {
		
      rawAVDD1 += ((unsigned int)(((ucharValFrameVec.at(2*n+1)& 0xC0)>> 6) << (10-2*i)));
      i++;
	
    }
        if(_debug) std::cout<<"rawAVDD1:"<<dec<<rawAVDD1<<std::endl;

  AVDD1 = 1.212766*(rawAVDD1*3.3)/4095.0; 

  //TransmitID
  i=0;
  for(n= 50; n <54 ; n++)  // mot sur 8 Bits
    {
      transmitID += ((unsigned int)(((ucharValFrameVec.at(2*n + 1)& 0xC0)>> 6) << (6-2*i)));   // MSB
      i++;
    }
      if(_debug) std::cout<<"transmitID:"<<dec<<transmitID<<std::endl;

  // actual decoding
  for(n= 0; n < nbOfSingleSkirocEventsInFrame; n++)
    {

      rawValue = (int)ucharValFrameVec.at(datasize -2*(n+1)-2-2) + ((int)(ucharValFrameVec.at(datasize -1 -2*(n+1)-2) & 0x0F)<<8) ;   
      int sca_ascii = nbOfSingleSkirocEventsInFrame-n-1;
      sca=nbOfSingleSkirocEventsInFrame-(sca_ascii+1);
  
      bcid[sca]=Convert_FromGrayToBinary(rawValue , 12);
      if(_debug) std::cout<<"sca:"<<sca<<" sca_ascii:"<<sca_ascii<<" bcid:"<<bcid[sca]<<std::endl;

      for(channel = 0; channel < NB_OF_CHANNELS_IN_SKIROC; channel++)
	{
	  rawData = (unsigned short)ucharValFrameVec.at(index+2*channel) + ((unsigned short)ucharValFrameVec.at(index+1+2*channel) << 8);
	  rawValue = (int)(rawData & 0xFFF);
	  hitvalue[1][sca][channel] =  (rawData & 0x1000)>>12;
	  gainvalue[1][sca][channel] =  (rawData & 0x2000)>>13;
	  int chargeValuetemp =  Convert_FromGrayToBinary(rawValue , 12); 
	  chargevalue[1][sca][channel] = chargeValuetemp;
	  if(_debug) std::cout<<"chn:"<<channel<<" gainvalue_1:"<<gainvalue[1][sca][channel]<<" hitvalue_1:"<<hitvalue[1][sca][channel]<<" "<<"chargeValue_1:"<<chargevalue[1][sca][channel]<<std::endl;

	}
		
      index += (NB_OF_CHANNELS_IN_SKIROC*2);

      nhits[sca]=0;
      for(channel = 0; channel < NB_OF_CHANNELS_IN_SKIROC; channel++)
	{
	  rawData = (unsigned short)ucharValFrameVec.at(index+2*channel) + ((unsigned short)ucharValFrameVec.at(index+1+2*channel) << 8);
	  rawValue = (int)(rawData & 0xFFF);
	  hitvalue[0][sca][channel] =  (rawData & 0x1000)>>12;
	  gainvalue[0][sca][channel] =  (rawData & 0x2000)>>13;  
	  int chargeValuetemp = Convert_FromGrayToBinary(rawValue , 12);
	  chargevalue[0][sca][channel] = chargeValuetemp;
	  if(_debug) std::cout<<"chn:"<<channel<<" gainvalue_0:"<<gainvalue[0][sca][channel]<<" hitvalue_0:"<<hitvalue[0][sca][channel]<<" "<<"chargeValue_0:"<<chargevalue[0][sca][channel]<<std::endl;
	  if(hitvalue[0][sca][channel]>0) nhits[sca]++;
	}
				
      index += (NB_OF_CHANNELS_IN_SKIROC*2);
      //if((nbOfSingleSkirocEventsInFrame-sca_ascii-1) == 0)
      //	_event++;

      if(_ASCIIOUT){
	//#0 Size 1 ChipID 9 coreIdx 0 slabIdx 0 slabAdd 0 Asu 0 SkirocIndex 9 transmitID 0 cycleID 2 StartTime 56717 rawTSD 3712 rawAVDD0 2017 rawAVDD1 2023 tsdValue 36.02 avDD0 1.971 aVDD1 1.977
	if( (nbOfSingleSkirocEventsInFrame-sca_ascii-1) == 0) std::cout<<"#"<<skirocEventNumber<<" Size "<<nbOfSingleSkirocEventsInFrame<<" ChipID "<<chipId<<" coreIdx "<<coreDaughterIndex<<" slabIdx "<<slabIndex<<" slabAdd "<<slabAdd<<
	  " Asu "<<asuIndex<<" SkirocIndex "<<skirocIndex<<" transmitID "<<transmitID<<" cycleID "<<cycleID<<" StartTime "<<startAcqTimeStamp<<
	  " rawTSD "<<rawTSD<<" rawAVDD0 "<<rawAVDD0<<" rawAVDD1 "<<rawAVDD1<<" tsdValue "<<temperature<<
	  " avDD0 "<<AVDD0<<" aVDD1 "<<AVDD1<<endl;
	////##0 BCID 53 SCA 0 #Hits 1
	////Ch 0 LG 282 0 0 HG 296 0 0
	std::cout<<"##"<<nbOfSingleSkirocEventsInFrame-sca_ascii-1<<" BCID "<<bcid[sca]<<" SCA "<<sca_ascii<<" #Hits "<<nhits[sca]<<std::endl;
	for(channel = 0; channel < NB_OF_CHANNELS_IN_SKIROC; channel++) 
	  std::cout<<"Ch "<<channel<<" LG "<< chargevalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]<<" "<<hitvalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]<<" "<<gainvalue[0][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]<<" HG "<< chargevalue[1][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]<<" "<<hitvalue[1][sca][NB_OF_CHANNELS_IN_SKIROC-channel-1]<<" "<<gainvalue[1][sca][channel-1]<<std::endl;
      }
      skirocEventNumber++;
    }
  
  }
  
  void SiReader::Read(std::deque<unsigned char> & buf, std::deque<eudaq::EventUP> & deqEvent) {
    static const unsigned int magic_sc = 0xCDAB;    // find start of data frame

    try {
      while (1){
	//	if(buf.size() > 1){
	  int bufsize = buf.size();
	  if (bufsize < 10) throw BufferProcessigExceptions::OK_NEED_MORE_DATA; 

          if(buf[0]==0xAB) {
	    if(_debug) cout<<" --cycleID:"<<cycleID<<" previous:"<<previous_cycleID<<endl;

            //buf.pop_front();
	    if(buf[1]==0xCD) {
	      if(_debug) cout<<"ibuf:"<<0<<"hex: "<<hex<<buf[1]<<endl;
	      if(_debug) cout<<" ---- dec:"<<std::dec<<buf[1]<<endl;
	      if(_debug) cout<<"FIRST FRAME FOUND "<<endl;
	      //	      buf.pop_front();

	      unsigned char a=buf[2];
	      unsigned char b=buf[3];
	      datasize=0;
	      unsigned short framesize =   ((unsigned short)b << 8) + a;
	      datasize=int(framesize);
	      if(_debug) cout<<" ---- dec:"<<std::dec<<framesize<<" "<<datasize<<endl;
	      
	      if(buf.size()<(datasize+10) ) {
		throw BufferProcessigExceptions::OK_NEED_MORE_DATA;
	      }
	      coreDaughterIndex=0;//buf[5];
	      slabAdd=buf[8];
	      slabIndex=buf[9];

	      //if(_debug)
	      //cout<<"---"<<coreDaughterIndex<<" "<<slabAdd<<" "<<slabIndex<<endl;
	     
	      
	      unsigned short trailerWord =   ((unsigned short)buf[8+datasize+2 -1] << 8) + buf[8+datasize+2 -2];
	      if(_debug) cout<<"Trailer Word hex:"<<hex<<trailerWord<<" dec:"<<dec<<trailerWord<<std::endl;

	      if(trailerWord == 0x9697) {

		std::vector<unsigned char> ucharValFrameVec;
		for(int ibuf=8; ibuf<datasize+10; ibuf++) {
		  ucharValFrameVec.push_back(buf[ibuf]);
		  if(_debug) cout<<datasize<<" LOOP, ibuf="<<ibuf<<" hex:"<<hex<<buf[ibuf]<<"  dec:"<<dec<<buf[ibuf]<<std::endl;
		}
		//cout<<ucharValFrameVec.size()<<endl;

		int latestfoundcycle=cycleIDDecoding(ucharValFrameVec);
		lastcycleid=latestfoundcycle;
		if(firstcycleid==-1) firstcycleid=latestfoundcycle;
		std::map<int, std::vector<std::vector<unsigned char>>>::iterator it;
		it=map_of_cycles_and_frames.find(latestfoundcycle);
		if( it == map_of_cycles_and_frames.end() ) {
		  //new cycle
		  std::vector<std::vector<unsigned char> > new_vector_of_frames;
		  new_vector_of_frames.push_back(ucharValFrameVec);
		  map_of_cycles_and_frames[latestfoundcycle]=new_vector_of_frames;
		  vector_with_dumped_cycles.push_back(latestfoundcycle);
		} else {
		  //existing cycle ID
		  it->second.push_back(ucharValFrameVec);
		}
		for(int ibuf=0; ibuf<datasize+10; ibuf++) {
                  buf.pop_front();
                }
	      
		if(map_of_cycles_and_frames.size()> _maxReadOutCycleJump) {

     	  
		  if(_debug) std::cout<<"MapSize:"<<map_of_cycles_and_frames.size()<<endl;
		  DumpCycle(deqEvent,false);

		}//dump cycle if map file is large enough
	      
	      }else {
		buf.pop_front();
		buf.pop_front();
	      }
	    } else buf.pop_front();
	  } else {
	    buf.pop_front();
	  }
      }//buf size>0
      //    }		// }//while1
    }  catch (BufferProcessigExceptions &e) {
      //all data in buffer processed (or not enough data in the buffer)
      //      if(_debug) cout << "CATCH "<<e<<endl;
      //  DumpCycle(deqEvent,true);
      //deqEvent.push_back(std::move(nev));
      //if(_debug) cout << "CATCH2 "<<endl;

	//if(_debug) cout << "\t last ROC: " << _LDAAsicData.rbegin()->first << "\t" << _LDATimestampData.rbegin()->first << endl;
	//         printLDATimestampCycles(_LDATimestampData);
      /*	switch (e) {
	case BufferProcessigExceptions::ERR_INCOMPLETE_INFO_CYCLE:
	  break;
	default:

	  //	buildEvents(deqEvent, false);

	  break;
	  }*/
    } // throw if data short
  }
  
    void SiReader::RunTimeStatistics::clear() {
      cycles = 0;
    }  
    const SiReader::RunTimeStatistics& SiReader::getRunTimesStatistics() const {
      return _RunTimesStatistics;
    }

    unsigned int SiReader::getCycleNo() const {
      return _cycleNo;
    }

  void SiReader::prepareEudaqRawPacket(eudaq::RawEvent * ev) {
	string s = "EUDAQDataSiECAL";
	ev->AddBlock(0, s.c_str(), s.length());
	s = "i:CycleNr,i:BunchXID,i:sca,i:Layer,i:SkirocID,i:NChannels,i:hit_low[NC],i:gain_low[NC],ADC_low[NC],i:hit_high[NC],i:gain_high[NC],ADC_high[NC]";
	ev->AddBlock(1, s.c_str(), s.length());
	unsigned int times[1];
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
	times[0] = std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
	//times[0]=1526342400;
	ev->AddBlock(2, times, sizeof(times));

	s="i:rawTSD,i:temperature,i:rawAVDD0,i:AVDDO,i:rawAVDD1,i:AVDD1";
	ev->AddBlock(3, s.c_str(), s.length());
	for( int iSLB=0; iSLB<SLBDEPTH; iSLB++) {
	  ev->AddBlock(4+iSLB, vector<int>()); // SiWECAL dummy block to be filled later with slowcontrol info  rawTSD rawAVDD0 rawAVDD1 tsdValue avDD0 aVDD1
	}

  }
  
  void SiReader::insertDummyEvent( std::deque<eudaq::EventUP> & deqEvent,  int rocnumber, int triggerid=-1, bool triggeridFlag=-1) {
    nev = eudaq::Event::MakeUnique("CaliceObject");
    nev_raw = dynamic_cast<RawEvent*>(nev.get());
    prepareEudaqRawPacket(nev_raw);
    if (rocnumber > 0) nev->SetEventN(rocnumber);//roc ?? AIQ
    if (triggerid > 0) nev->SetTriggerN(triggerid, triggeridFlag); //bcid ?? AIQ
    nev->SetTag("Dummy", 1);
    deqEvent.push_back(std::move(nev));
    if(_debug) cout<<"INSERTING DUMMY EVENT: "<<rocnumber<<endl;
      
  }

  void SiReader::DumpCycle(std::deque<eudaq::EventUP> &deqEvent, bool dumpAll){

    //sorting the map
    std::vector<pair<int, std::vector<std::vector<unsigned char> > > > map_dumped_into_a_vector;
    copy(map_of_cycles_and_frames.begin(),
	 map_of_cycles_and_frames.end(),
	 back_inserter<vector<pair<int, std::vector<std::vector<unsigned char> > > > >(map_dumped_into_a_vector));

    int ncycles=1;
    if(dumpAll==true) ncycles=map_dumped_into_a_vector.size();
    
    for(int icycles=0; icycles<ncycles; icycles++) {
      
      buildEvent(map_dumped_into_a_vector[icycles],deqEvent);

      std::map<int, std::vector<std::vector<unsigned char>>>::iterator it;
      it=map_of_cycles_and_frames.find(map_dumped_into_a_vector[icycles].first);
      if (it != map_of_cycles_and_frames.end())
	map_of_cycles_and_frames.erase(it++);
    }
  }

  void SiReader::buildEvent(pair<int, std::vector<std::vector<unsigned char> > > map_dumped_into_a_vector_element, std::deque<eudaq::EventUP> &deqEvent) {

     //AHCAL stuff if (_producer->getDebugKeepBuffered()) return;
     std::lock_guard<std::mutex> lock(_eventBuildingQueueMutex); //minimal lock for pushing new event
     //    switch (_producer->getEventMode()) {
     //    case SiWECALProducer::EventBuildingMode::ROC:
     buildROCEvents(map_dumped_into_a_vector_element,deqEvent);
     //       break;
     /* case SiWECALProducer::EventBuildingMode::BUILD_BXID_ALL:
	buildBXIDEvents(deqEvent, dumpAll);
	break;
	case SiWECALProducer::EventBuildingMode::BUILD_BXID_VALIDATED:
	buildValidatedBXIDEvents(deqEvent, dumpAll);
	break;
	default:
	break;*/
   }
  
  void SiReader::buildROCEvents(pair<int, std::vector<std::vector<unsigned char> > > map_dumped_into_a_vector_element, std::deque<eudaq::EventUP> &deqEvent) {

    int currentcycleID=map_dumped_into_a_vector_element.first;
    if(_debug) cout<<" cycleID:"<<currentcycleID<<" sca:"<<sca<<" prev:"<<previous_cycleID<<std::endl;

    if(currentcycleID == -1 && previous_cycleID==-1 && firstdummy==false) {
      insertDummyEvent(deqEvent,0);
      firstdummy=true;
    }
    if((currentcycleID-previous_cycleID)>1 && previous_cycleID==-1 && currentcycleID>1) {
      for(int idummy=0; idummy<currentcycleID; idummy++) insertDummyEvent(deqEvent,idummy+1);
    }
    if((currentcycleID-previous_cycleID)>1 && previous_cycleID>-1) {
      for(int idummy=previous_cycleID; idummy<currentcycleID-1; idummy++) insertDummyEvent(deqEvent,idummy+1);
    }

    //eudaq::EventUP nev;
    //eudaq::RawEvent *nev_raw;

    int nframes=map_dumped_into_a_vector_element.second.size();
    if(_debug)   std::cout<<"storing cycleID:"<<map_dumped_into_a_vector_element.first<<" that has "<<nframes<<" nframes"<<endl;

    int slab_with_data[100]={0};
    
    for(int jframes=0; jframes<nframes; jframes++) {
      /*  if( map_dumped_into_a_vector_element.second.at(jframes).size()<datasize) {
	cout<<" ERROR ? size of the frame:"<<map_dumped_into_a_vector_element.second.at(jframes).size()<<" is smaller than the datasize "<<datasize<<endl;
	std::cout<<jframes<<" storing cycleID:"<<map_dumped_into_a_vector_element.first<<" that has "<<nframes<<" nframes"<<endl;
	//	continue;
	}*/
      DecodeRawFrame(map_dumped_into_a_vector_element.second.at(jframes));

      if(jframes==0) {
	nev = eudaq::Event::MakeUnique("CaliceObject");
	nev_raw = dynamic_cast<RawEvent*>(nev.get());
	prepareEudaqRawPacket(nev_raw);
	nev->SetTag("ROC", cycleID);
	nev->SetTag("NSLBs",SLBDEPTH);
	nev->SetTag("StartAcqTime",startAcqTimeStamp);
	//	nev->SetTag("BXID", bcid); //only for an eventual buildBXIDnot to be used with 
      }

      if(slab_with_data[slabAdd]==0) {
	std::vector<int> slowcontroldata;
	
	slowcontroldata.push_back(rawTSD);
	slowcontroldata.push_back(temperature);
	slowcontroldata.push_back(rawAVDD0);
	slowcontroldata.push_back(AVDD0);
	slowcontroldata.push_back(rawAVDD1);
	slowcontroldata.push_back(AVDD1);

	nev_raw->AppendBlock(4+slabAdd, slowcontroldata);//
	slab_with_data[slabAdd]=1;
	//	cout<<"cycleID" <<cycleID<<" "<<rawTSD<<" "<<AVDD1<<endl;
      }
      //      cout<<"slabAdd:"<<slabAdd<<endl;

      for(int isca=0; isca<sca; isca++) {
	std::vector<int> cycledata;
	cycledata.push_back((int) (cycleID));
	cycledata.push_back((int) (bcid[isca]));
	cycledata.push_back((int) (isca));
	cycledata.push_back((int) (slabAdd));
	cycledata.push_back((int) (skirocIndex));
	cycledata.push_back((int) (NB_OF_CHANNELS_IN_SKIROC));
	for(channel = 0; channel < NB_OF_CHANNELS_IN_SKIROC; channel++) {
	  cycledata.push_back((int) (chargevalue[0][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	  cycledata.push_back((int) (hitvalue[0][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	  cycledata.push_back((int) (gainvalue[0][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	  cycledata.push_back((int) (chargevalue[1][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	  cycledata.push_back((int) (hitvalue[1][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	  cycledata.push_back((int) (gainvalue[1][isca][NB_OF_CHANNELS_IN_SKIROC-channel-1]));
	}
	nev_raw->AddBlock(nev_raw->NumBlocks(), cycledata);//
      }
      
    }
    
    // if(previous_cycleID>0) {
    deqEvent.push_back(std::move(nev));
    if(_debug) cout<<"INSERTING GOOD EVENT: "<<cycleID<<endl;
    //}
    previous_cycleID=cycleID;
    _cycleNo=cycleID;
  }

}

