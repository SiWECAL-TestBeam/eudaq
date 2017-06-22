#include "eudaq/Configuration.hh"
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"
#include <iostream>
#include <ostream>
#include <cstdlib>
#include <string>
#include <vector>


#include <TFile.h>
#include <TTree.h>

#include "CAEN_v1290.h"
#include "Unpacker.h"



static const std::string EVENT_TYPE = "DelayWireChambers";

class WireChamberProducer : public eudaq::Producer {
  public:

  WireChamberProducer(const std::string & name, const std::string & runcontrol)
    : eudaq::Producer(name, runcontrol), m_run(0), m_ev(0), stopping(false), done(false), started(0) {
      std::cout<<"Initialisation of the DWC Producer..."<<std::endl;
      tdc = new CAEN_V1290();
      tdc->Init();
      tdc_unpacker = NULL;
      outTree=NULL;
    }

  virtual void OnConfigure(const eudaq::Configuration & config) {
    std::cout << "Configuring: " << config.Name() << std::endl;

    CAEN_V1290::CAEN_V1290_Config_t _config;

    _config.baseAddress = config.Get("baseAddress", 0x00AA0000);
    _config.model = static_cast<CAEN_V1290::CAEN_V1290_Model_t>(config.Get("model", 1));
    _config.triggerTimeSubtraction = static_cast<bool>(config.Get("triggerTimeSubtraction", 1));
    _config.triggerMatchMode = static_cast<bool>(config.Get("triggerMatchMode", 1));
    _config.emptyEventEnable = static_cast<bool>(config.Get("emptyEventEnable", 1));
    _config.edgeDetectionMode = static_cast<CAEN_V1290::CAEN_V1290_EdgeDetection_t>(config.Get("edgeDetectionMode", 3));
    _config.timeResolution = static_cast<CAEN_V1290::CAEN_V1290_TimeResolution_t>(config.Get("timeResolution", 3));
    _config.maxHitsPerEvent = static_cast<CAEN_V1290::CAEN_V1290_MaxHits_t>(config.Get("maxHitsPerEvent", 8));
    _config.enabledChannels = config.Get("enabledChannels", 0x00FF);
    _config.windowWidth = config.Get("windowWidth", 0x40);
    _config.windowOffset = config.Get("windowOffset", -1);

    tdc->Config(_config);
    tdc->SetupModule();

    //read the channel map
    N_channels = config.Get("N_channels", 16);
    EUDAQ_INFO("Enabled channels:");
    for (unsigned int channel=0; channel<N_channels; channel++){
      channels_enabled[channel] = (bool)config.Get(("channel_"+std::to_string(channel)).c_str(), -1);
      std::cout<<"TDC channel "<<channel<<" enabled ? "<<channels_enabled[channel]<<std::endl;
    }
  
    //setup the synchronisation board
    int mode = config.Get("AcquisitionMode", 0);
    /*
    switch( mode ){
      case 0 : 
      default : 
    }
    */
    
    //Read the data output file prefix
    dataFilePrefix = config.Get("dataFilePrefix", "../data/dwc_run_");

    SetStatus(eudaq::Status::LVL_OK, "Configured (" + config.Name() + ")");

  }

  // This gets called whenever a new run is started
  // It receives the new run number as a parameter
  virtual void OnStartRun(unsigned param) {
    m_run = param;
    m_ev = 0;
    EUDAQ_INFO("Start Run: "+param);
  
    tdc->BufferClear();

    dwc_timestamps.clear();
    channels.clear();
    for (size_t channel=0; channel<N_channels; channel++) {
      channels.push_back(-1);
      dwc_timestamps.push_back(defaultTimestamp);
    }

    if (tdc_unpacker != NULL) delete tdc_unpacker;
    tdc_unpacker = new Unpacker(N_channels);

    if (outTree != NULL) delete outTree;
    outTree = new TTree("DelayWireChambers", "DelayWireChambers");
    outTree->Branch("run", &m_run);
    outTree->Branch("event", &m_ev);
    outTree->Branch("channels", &channels);
    outTree->Branch("dwc_timestamps", &dwc_timestamps);
    

    // It must send a BORE to the Data Collector
    eudaq::RawDataEvent bore(eudaq::RawDataEvent::BORE(EVENT_TYPE, m_run));
    SendEvent(bore);
    SetStatus(eudaq::Status::LVL_OK, "Running");
    started=true;
  }

  // This gets called whenever a run is stopped
  virtual void OnStopRun() {
    SetStatus(eudaq::Status::LVL_OK, "Stopping");
    EUDAQ_INFO("Stopping Run");
    started=false;
    // Set a flag tao signal to the polling loop that the run is over
    stopping = true;

    // Send an EORE after all the real events have been sent
    // You can also set tags on it (as with the BORE) if necessary
    SendEvent(eudaq::RawDataEvent::EORE("Test", m_run, ++m_ev));

    std::ostringstream os;
    os.str(""); os<<"Saving the data into the outputfile: "<<dataFilePrefix<<m_run<<".root";
    EUDAQ_INFO(os.str().c_str());
    //save the tree into a file
    TFile* outfile = new TFile((dataFilePrefix+std::to_string(m_run)+".root").c_str(), "RECREATE");
    outTree->Write();
    outfile->Close();

    stopping = false;
    SetStatus(eudaq::Status::LVL_OK, "Stopped");
  }

  // This gets called when the Run Control is terminating,
  // we should also exit.
  virtual void OnTerminate() {
    EUDAQ_INFO("Terminating...");
    done = true;
    eudaq::mSleep(200);
    
    delete tdc;
    delete tdc_unpacker;
  }


  void ReadoutLoop() {
    while(!done) {
      if (!started) {
        eudaq::mSleep(200);
        continue;
      }
      
      if (stopping) continue;
      
      //boost::thread TDC_thread = boost::thread(readTDCThread, tdc, std::ref(dataStream));
      //TDC_thread.join();
      tdc->Read(dataStream);
      
      if (dataStream.size() == 0)
        continue;
      
  
      m_ev++;
      tdcData unpacked = tdc_unpacker->ConvertTDCData(dataStream);

      //making an EUDAQ event
      eudaq::RawDataEvent ev(EVENT_TYPE,m_run,m_ev);
      ev.AddBlock(1, dataStream);

      for (int channel=0; channel<N_channels; channel++) {
        channels[channel] = channel;
        dwc_timestamps[channel] = channels_enabled[channel]  ? unpacked.timeOfArrivals[channel] : defaultTimestamp;
      }


      std::cout<<"+++ Event: "<<m_ev<<" +++"<<std::endl;
      for (int channel=0; channel<N_channels; channel++) std::cout<<" "<<dwc_timestamps[channel]; std::cout<<std::endl;
      
      outTree->Fill();
      
      //Adding the event to the EUDAQ format
      
      SendEvent(ev);
    }
  }

  private:
    unsigned m_run, m_ev;
    bool stopping, done, started;

    std::string dataFilePrefix;

    //set on configuration
    CAEN_V1290* tdc;
    Unpacker* tdc_unpacker;

    std::vector<WORD> dataStream;

    int N_channels;
    std::map<int, bool> channels_enabled;

    //generated for each run
    TTree* outTree;

    std::vector<int> dwc_timestamps; 
    std::vector<int> channels;  

    int defaultTimestamp;

};

// The main function that will create a Producer instance and run it
int main(int /*argc*/, const char ** argv) {
  // You can use the OptionParser to get command-line arguments
  // then they will automatically be described in the help (-h) option
  eudaq::OptionParser op("Delay Wire Chamber Producer", "0.1",
  "Just an example, modify it to suit your own needs");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol",
  "tcp://localhost:44000", "address",
  "The address of the RunControl.");
  eudaq::Option<std::string> level(op, "l", "log-level", "NONE", "level",
  "The minimum level for displaying log messages locally");
  eudaq::Option<std::string> name (op, "n", "name", "DWCs", "string",
  "The name of this Producer.");
  
  try {
    op.Parse(argv);
    EUDAQ_LOG_LEVEL(level.Value());
    WireChamberProducer producer(name.Value(), rctrl.Value());
    producer.ReadoutLoop();
    
    EUDAQ_INFO("Quitting");
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
