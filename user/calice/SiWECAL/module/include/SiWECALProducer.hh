#ifndef SiWECALPRODUCER_HH
#define SiWECALPRODUCER_HH

#include "eudaq/Producer.hh"

#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <thread>
#include <mutex>

namespace eudaq {

  class SiWECALProducer;

  class SiWECALReader {
  public:
    virtual void Read(std::deque<unsigned char> & buf, std::deque<eudaq::EventUP> & deqEvent) = 0;
    virtual void OnStart(int runNo) {
    }
    virtual void OnStop(int waitQueueTimeS) {
    }
    virtual void OnConfig(std::string _fname) {
    }
    virtual void DumpCycle(std::deque<eudaq::EventUP> &deqEvent, bool dumpAll) {
    }
    
    SiWECALReader(SiWECALProducer *r) :
      _producer(r) {
    }
    virtual ~SiWECALReader() {
    }

    bool _debug=false;
    bool _ASCIIOUT=false;
    int previous_cycleID=-1;
        
   virtual void setDebugMessages(bool b) {
      _debug=b;
    }
    virtual void setDumpASCII(bool b) {
      _ASCIIOUT=b;
    }

  public:
    std::mutex _eventBuildingQueueMutex;

  protected:

    SiWECALProducer * _producer;
  };

  class SiWECALProducer: public eudaq::Producer {
  public:

    SiWECALProducer(const std::string & name, const std::string & runcontrol);
    void DoConfigure() override final;
    void DoStartRun() override final;
    void DoStopRun() override final;
    void DoTerminate() override final;
    void DoReset() override final {
    }
    void RunLoop() override final;

    void SetReader(SiWECALReader *r) {
      _reader = r;
    }
    bool OpenConnection(); //
    void CloseConnection(); //
    void SendCommand(const char *command, int size = 0);

    void OpenRawFile(unsigned param, bool _writerawfilename_timestamp);
    void sendallevents(std::deque<eudaq::EventUP> &deqEvent, int minimumsize);

       
    static const uint32_t m_id_factory = eudaq::cstr2hash("SiWECALProducer");
  private:
     
    int _runNo;
    int _eventNo; //last sent event - for checking of correct event numbers sequence during sending events
#ifdef _WIN32
    SOCKET _fd;
    std::ifstream _redirectedInputFstream;
#else
    int _fd;
#endif
    int _maxTrigidSkip;

    std::mutex _mufd;

    std::string _redirectedInputFileName; // if set, this filename will be used as input

    bool _running;
    bool _stopped;
    bool _terminated;
    bool _BORE_sent;         //was first event sent? (id has to be marked with BORE tag)

    // debug output
    //         bool _dumpRaw;
    bool _writeRaw;
    std::string _rawFilename;
    bool _writerawfilename_timestamp;
    std::ofstream _rawFile;
    //run type:
    //std::string _runtype;
    std::string _fileSettings;

    //std::string _filename; // input file name at file mode
    int _StartWaitSeconds;
    int _waitmsFile; // period to wait at each ::read() at file mode
    int _waitsecondsForQueuedEvents; // period to wait after each run to read the queued events
    int _SlowdownMillisecAfterEvents; //wait 1 ms after given amount of events
    int _port; // input port at network mode
    std::string _ipAddress; // input address at network mode

    //std::time_t _last_readout_time; //last time when there was any data from SiWECAL

    uint32_t m_id_stream;

    SiWECALReader * _reader;
  };

}

#endif // SiWECALPRODUCER_HH

