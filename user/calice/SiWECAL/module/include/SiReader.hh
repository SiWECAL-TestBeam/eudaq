#ifndef SIREADER_HH
#define SIREADER_HH

#include "SiWECALProducer.hh"
#include "eudaq/RawEvent.hh"
#include <cmath> 
#include <deque>

#define TERMCOLOR_RED_BOLD "\033[31;1m"
#define TERMCOLOR_RED "\033[31m"
#define TERMCOLOR_GREEN_BOLD "\033[32;1m"
#define TERMCOLOR_GREEN "\033[32m"
#define TERMCOLOR_YELLOW_BOLD "\033[33;1m"
#define TERMCOLOR_YELLOW "\033[33m"
#define TERMCOLOR_BLUE_BOLD "\033[34;1m"
#define TERMCOLOR_BLUE "\033[34m"
#define TERMCOLOR_MAGENTA_BOLD "\033[35;1m"
#define TERMCOLOR_MAGENTA "\033[35m"
#define TERMCOLOR_CYAN_BOLD "\033[36;1m"
#define TERMCOLOR_CYAN "\033[36m"
#define TERMCOLOR_RESET "\033[0m"

#define DAQ_ERRORS_INCOMPLETE        0x0001
#define DAQ_ERRORS_OUTSIDE_ACQ       0x0002
#define DAQ_ERRORS_MULTIPLE_TRIGGERS 0x0004
#define DAQ_ERRORS_MISSED_DUMMY      0x0008
#define DAQ_ERRORS_MISSING_START     0x0010
#define DAQ_ERRORS_MISSING_STOP      0x0020

#define NB_OF_SKIROCS_PER_ASU 16
#define NB_OF_CHANNELS_IN_SKIROC 64
#define NB_OF_SCAS_IN_SKIROC 15
#define SINGLE_SKIROC_EVENT_SIZE (129*2) 
#define SLBDEPTH 15 
#define NEGDATA_THR 11
#define BCIDTHRES 4

namespace eudaq {

  class SiReader: public SiWECALReader {
  public:

    bool _debug=false;
    bool _ASCIIOUT=false;
    virtual void Read(std::deque<unsigned char> & buf, std::deque<eudaq::EventUP> & deqEvent) override;
    virtual void OnStart(int runNo) override;
    virtual void OnStop(int waitQueueTimeS) override;
    virtual void OnConfig(std::string _fname) override; //chose configuration file

    //         virtual std::deque<eudaq::RawEvent *> NewEvent_createRawDataEvent(std::deque<eudaq::RawEvent *> deqEvent, bool tempcome, int LdaRawcycle,
    //         bool newForced);
    //  virtual void readTemperature(std::deque<unsigned char>& buf);

    //   virtual void setTbTimestamp(uint32_t ts) override;
    //  virtual uint32_t getTbTimestamp() const override;

    //   int updateCntModulo(const int oldCnt, const int newCntModulo, const int bits, const int maxBack);
    //   void appendOtherInfo(eudaq::RawEvent * ev);

    SiReader(SiWECALProducer *r); //:
    //               SiWECALReader(r),
    //                     _runNo(-1),
    //                     _buffer_inside_acquisition(false),
    //                     _lastBuiltEventNr(0),
    //                     _cycleNo(0),
    //                     _tempmode(false),
    //                     _trigID(0),
    //                     _unfinishedPacketState(UnfinishedPacketStates::DONE),
    //                     length(0) {
    //         }
    virtual ~SiReader();

    virtual void setDebugMessages(bool b) {
      _debug=b;
    }
    virtual void setDumpASCII(bool b) {
      _ASCIIOUT=b;
    }

    struct RunTimeStatistics {
      void clear();
      // void append(const RunTimeStatistics& otherStats);
      void print(std::ostream &out, int colorOutput) const;
      uint64_t cycles;
    
    };

    const SiReader::RunTimeStatistics& getRunTimesStatistics() const;
    unsigned int getCycleNo() const;


  private:
    int coreDaughterIndex;
    int chipId;
    int asuIndex; 
    int slabIndex;
    int skirocIndex;
    int datasize;
    int nbOfSingleSkirocEventsInFrame;
    int frame,n , i;
    unsigned short rawData;
    int index;
    int channel;
    int rawValue;
    int slabAdd;
    unsigned short trailerWord;
    int rawTSD ;
    int rawAVDD0, rawAVDD1;
    int previous_cycleID;
    int cycleID;
    int transmitID;
    double startAcqTimeStamp;
    float temperature; // in °C
    float AVDD0; // in Volts
    float AVDD1; // in Volts
    int sca;
    int core, slab;
    int chargevalue[2][SINGLE_SKIROC_EVENT_SIZE][NB_OF_CHANNELS_IN_SKIROC];
    int gainvalue[2][SINGLE_SKIROC_EVENT_SIZE][NB_OF_CHANNELS_IN_SKIROC];
    int hitvalue[2][SINGLE_SKIROC_EVENT_SIZE][NB_OF_CHANNELS_IN_SKIROC];
    int valid_frame;
    int bcid[SINGLE_SKIROC_EVENT_SIZE];
    int skirocEventNumber;
    int nhits[SINGLE_SKIROC_EVENT_SIZE];

    eudaq::EventUP nev;
    eudaq::RawEvent *nev_raw;
    
    enum class UnfinishedPacketStates {
				       DONE = (unsigned int) 0x0000,
				       //            LEDINFO = (unsigned int) 0x0001,
				       TEMPERATURE = (unsigned int) 0x0001,
				       SLOWCONTROL = (unsigned int) 0x0002,
    };
    enum {
	  e_sizeLdaHeader = (int)10 // 8bytes + 0xcdcd
    };
    enum BufferProcessigExceptions {
				    ERR_INCOMPLETE_INFO_CYCLE, OK_ALL_READ, OK_NEED_MORE_DATA
    };

  private:
    
    void DecodeAndSendRawFrame(std::vector<unsigned char> ucharValFrameVec);
    void prepareEudaqRawPacket(eudaq::RawEvent * ev);
    void colorPrint(const std::string &colorString, const std::string& msg);

 
    UnfinishedPacketStates _unfinishedPacketState;

    int _runNo;
    int _cycleNo; //last successfully read readoutcycle: ASIC Data
    int length; //length of the packed derived from LDA Header

    //bool _tempmode; // during the temperature readout time
    bool _buffer_inside_acquisition; //the reader is reading data from within the acquisition, defined by start and stop commands
    //uint64_t _last_stop_ts; //timestamp of the last stop of acquisition

     std::vector<uint32_t> cycleData;

    RunTimeStatistics _RunTimesStatistics;

    /* ======================================================================= */
    int	Convert_FromGrayToBinary (int grayValue, int nbOfBits)
    /* ======================================================================= */
    {
      // converts a Gray integer of nbOfBits bits to a decimal integer.
      int binary, grayBit, binBit;
      
      binary = 0;
      
      // mask the MSB.
      
      grayBit = 1 << ( nbOfBits - 1 );
      
      // copy the MSB.
      
      binary = grayValue & grayBit;
      
      // store the bit we just set.
      
      binBit = binary;
      
      // traverse remaining Gray bits.
      
      while( grayBit >>= 1 )
	{
	  // shift the current binary bit to align with the Gray bit.
	  binBit >>= 1;
	  // XOR the two bits.
	  binary |= binBit ^ ( grayValue & grayBit );
	  // store the current binary bit
	  binBit = binary & grayBit;
	}
      
      return( binary );
    }

    
  };
}

#endif // SIREADER_HH
