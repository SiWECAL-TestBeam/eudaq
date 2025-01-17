// CaliceReceiver.cc

#include "SiWECALProducer.hh"

#include "eudaq/OptionParser.hh"
#include "eudaq/Logger.hh"
#include "eudaq/Configuration.hh"
#include "eudaq/Utils.hh"
#include "SiReader.hh"
#include "stdlib.h"

#include <iomanip>
#include <iterator>
#include <thread>
#include <mutex>

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#include <winsock.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace eudaq;
using namespace std;

namespace {
   auto dummy0 = eudaq::Factory<eudaq::Producer>::Register<SiWECALProducer, const std::string&, const std::string&>(SiWECALProducer::m_id_factory);
}

SiWECALProducer::SiWECALProducer(const std::string & name, const std::string & runcontrol) :
  Producer(name, runcontrol), _runNo(0), _eventNo(0), _fd(0), _running(false), _stopped(true), _terminated(false), _BORE_sent(false), _reader(NULL),  _port(5622), _StartWaitSeconds(2),_waitmsFile(0), _waitsecondsForQueuedEvents(2), _writerawfilename_timestamp(true), _writeRaw(true) {
   m_id_stream = eudaq::cstr2hash(name.c_str());
}

void SiWECALProducer::DoTerminate() {
   _terminated = true;
}

void SiWECALProducer::DoConfigure() {
   const eudaq::Configuration &param = *GetConfiguration();
   std::cout << " START SiWECAL 2CONFIGURATION " << std::endl;
   // configuration file, empty by default"
   _fileSettings = param.Get("FileSettings", "");

   // file name
   _waitmsFile = param.Get("WaitMillisecForFile", 100);
   _waitsecondsForQueuedEvents = param.Get("waitsecondsForQueuedEvents", 2);
   _SlowdownMillisecAfterEvents = param.Get("SlowdownMillisecAfterEvents",0);

    // raw output
   _writeRaw = param.Get("WriteRawOutput", 1);
   _rawFilename = param.Get("RawFileName", "run%d.raw");
   _writerawfilename_timestamp = param.Get("WriteRawFileNameTimestamp", 0);

    // port
   _port = param.Get("Port", 8008);
   _ipAddress = param.Get("IPAddress", "192.168.0.66");
   _redirectedInputFileName = param.Get("RedirectInputFromFile", "");

   if (!_reader) {
      SetReader(new SiReader(this));
   }
   //debug information
   bool debugmessages=param.Get("DebugMessages", true);
   bool dumpascii=param.Get("DumpASCII", true);
   _reader->setDebugMessages(debugmessages);
   _reader->setDumpASCII(dumpascii);

   //      if (_reader != nullptr)
   //         SetReader(std::unique_ptr<ScReader>(new ScReader(this))); // in sc dif ID is not specified
   std::cout << " config step " << std::endl;
   _reader->OnConfig(_fileSettings); //newLED

}

void SiWECALProducer::DoStartRun() {
   _runNo = GetRunNumber();
   _eventNo = 0;
   _BORE_sent = false;
   // raw file open
   if (_writeRaw) OpenRawFile(_runNo, _writerawfilename_timestamp);
   if (_StartWaitSeconds) {
      std::cout << "Delayed start by " << _StartWaitSeconds << " seconds. Waiting";
      for (int i = 0; i < _StartWaitSeconds; i++) {
         std::cout << "." << std::flush;
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      std::cout << std::endl;
   }
   std::cout << "SiWECALProducer::OnStartRun _reader->OnStart(param);" << std::endl; //DEBUG
   _reader->previous_cycleID=-1;
   _reader->OnStart(_runNo);
   std::cout << "Start Run: " << _runNo << std::endl;
   _running = true;
   _stopped = false;
   
}

void SiWECALProducer::OpenRawFile(unsigned param, bool _writerawfilename_timestamp) {

   //	read the local time and save into the string myString
   time_t ltime;
   struct tm *Tm;
   ltime = time(NULL);
   Tm = localtime(&ltime);
   char file_timestamp[25];
   std::string myString;
   if (_writerawfilename_timestamp == 1) {
      sprintf(file_timestamp, "__%02dp%02dp%02d__%02dp%02dp%02d.raw", Tm->tm_mday, Tm->tm_mon + 1, Tm->tm_year + 1900, Tm->tm_hour, Tm->tm_min, Tm->tm_sec);
      myString.assign(file_timestamp, 26);
   } else
      myString = ".raw";

   std::string _rawFilenameTimeStamp;
   //if chosen like this, add the local time to the filename
   _rawFilenameTimeStamp = _rawFilename + myString;
   char _rawFilename[256];
   sprintf(_rawFilename, _rawFilenameTimeStamp.c_str(), (int) param);

   _rawFile.open(_rawFilename, std::ofstream::binary);
}

void SiWECALProducer::DoStopRun() {
   std::cout << "SiWECALProducer::DoStopRun:  Stop run" << std::endl;
   _reader->OnStop(_waitsecondsForQueuedEvents);
   _running = false;
   //   _stopped=true;
   std::this_thread::sleep_for(std::chrono::seconds(1));

   std::cout << "SiWECALProducer::OnStopRun waiting for _stopped" << std::endl;
   while (!_stopped) {
      std::cout << "!" << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }

   if (_writeRaw) _rawFile.close();
   std::cout << "SiWECALProducer::DoStopRun() finished" << std::endl;
   //CloseConnection();
   //   SetStatusTag("ReprocessingFinished", std::to_string(0));
 
}

bool SiWECALProducer::OpenConnection() {
  std::cout << " OPEN CONNECTION " << std::endl;

#ifdef _WIN32
  if (_redirectedInputFileName.empty()) {
     WSADATA wsaData;
     int wsaRet=WSAStartup(MAKEWORD(2, 2), &wsaData); //initialize winsocks 2.2
     if (wsaRet) {cout << "ERROR: WSA init failed with code " << wsaRet << endl; return false;}
     cout << "DEBUG: WSAinit OK" << endl;

     std::unique_lock<std::mutex> myLock(_mufd);
     _fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (_fd == INVALID_SOCKET) {
       cout << "ERROR: invalid socket" << endl;
       WSACleanup;
       return false;
     }
     cout << "DEBUG: Socket OK" << endl;

     struct sockaddr_in dstAddr; //win ok
     //??		memset(&dstAddr, 0, sizeof(dstAddr));
     dstAddr.sin_family = AF_INET;
     dstAddr.sin_port = htons(_port);
     dstAddr.sin_addr.s_addr = inet_addr(_ipAddress.c_str());

     int ret = connect(_fd, (struct sockaddr *) &dstAddr, sizeof(dstAddr));
     if (ret != 0) {
       cout << "DEBUG: Connect failed" << endl;
       return 0;
     }
     cout << "DEBUG: Connect OK" << endl;
     return 1;
  } else {
     std::cout << "Redirecting intput from file: " << _redirectedInputFileName << std::endl;
       _redirectedInputFstream = std::ifstream(_redirectedInputFileName.c_str(),ios::in|ios::binary);
       //_fd = open(_redirectedInputFileName.c_str(), O_RDONLY);
       if (_redirectedInputFstream) {
          _redirectedInputFstream.seekg(0, _redirectedInputFstream.end);
          int length = _redirectedInputFstream.tellg();
          _redirectedInputFstream.seekg(0, _redirectedInputFstream.beg);
          std::cout << "Redirected file is " << length << " bytes long" << std::endl;
          return true;
       } else {
          cout << "open redirected file failed from this path:" << _redirectedInputFileName << endl;
          return false;
       }
    }
#else
   if (_redirectedInputFileName.empty()) {
      struct sockaddr_in dstAddr;
      memset(&dstAddr, 0, sizeof(dstAddr));
      dstAddr.sin_port = htons(_port);
      dstAddr.sin_family = AF_INET;
      dstAddr.sin_addr.s_addr = inet_addr(_ipAddress.c_str());
      std::unique_lock<std::mutex> myLock(_mufd);
      _fd = socket(AF_INET, SOCK_STREAM, 0);
      int ret = connect(_fd, (struct sockaddr*) &dstAddr, sizeof(dstAddr));
      if (ret != 0) return 0;
      return 1;
   } else {
      std::cout << "Redirecting intput from file: " << _redirectedInputFileName << std::endl;
      std::cout << "Waiting " << _waitmsFile << " ms ...";
      eudaq::mSleep(_waitmsFile);
      std::cout << "Finished";
      _fd = open(_redirectedInputFileName.c_str(), O_RDONLY);
      if (_fd < 0) {
         cout << "open redirected file failed from this path:" << _redirectedInputFileName << endl;
         return false;
      }
      return true;
   }

#endif //_WIN32
}

void SiWECALProducer::CloseConnection() {
   std::unique_lock<std::mutex> myLock(_mufd);
#ifdef _WIN32
   if (_redirectedInputFileName.empty()) {
      closesocket(_fd);
   } else {
      _redirectedInputFstream.close();
   }
   WSACleanup;
#else
   close(_fd);
#endif
   _fd = 0;
}

// send a string without any handshaking
void SiWECALProducer::SendCommand(const char *command, int size) {
   cout << "DEBUG: in SiWECALProducer::SendCommand(const char *command, int size)" << endl;
   if (size == 0) size = strlen(command);
   cout << "DEBUG: size: " << size << " message:" << command << endl;
   if (_redirectedInputFileName.empty()) {
      if (_fd <= 0) {
         cout << "SiWECALProducer::SendCommand(): cannot send command because connection is not open." << endl;
      }
      cout << "DEBUG: sending command over TCP" << endl;
#ifdef _WIN32
      size_t bytesWritten = send(_fd, command, size, 0);
#else
      size_t bytesWritten = write(_fd, command, size);
#endif
      if (bytesWritten < 0) {
         cout << "There was an error writing to the TCP socket" << endl;
      } else {
         cout << bytesWritten << " out of " << size << " bytes is  written to the TCP socket" << endl;
      }
   } else {
      std::cout << "input overriden from file. No command is send." << std::endl;
      std::cout << "sending " << size << " bytes:";
      for (int i = 0; i < size; ++i) {
         std::cout << " " << to_hex(command[i], 2);
      }
      std::cout << std::endl;
   }
   
}


void SiWECALProducer::sendallevents(std::deque<eudaq::EventUP> & deqEvent, int minimumsize) {
   while (deqEvent.size() > minimumsize) {
      std::lock_guard<std::mutex> lock(_reader->_eventBuildingQueueMutex);
      if (deqEvent.front()) {
         if (!_BORE_sent) {
            _BORE_sent = true;
            deqEvent.front()->SetBORE();
	    //  deqEvent.front()->SetTag("FirstROCStartTS", dynamic_cast<ScReader*>(_reader)->getRunTimesStatistics().first_TS);
         }
         if ((minimumsize == 0) && (deqEvent.size() == 1)) deqEvent.front()->SetEORE();

	 // if ((deqEvent.front()->GetBlockNumList().size() - 10) >= _minEventHits) {
	 if ((deqEvent.front()->GetBlockNumList().size()>0) ) {
	    _eventNo++;      // = deqEvent.front()->GetEventN();
            SendEvent(std::move(deqEvent.front()));
	    if (_SlowdownMillisecAfterEvents){
	      if ( (_eventNo % _SlowdownMillisecAfterEvents) == 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	    }
         }
	 deqEvent.pop_front();
      }
   }
}

void SiWECALProducer::RunLoop() {
   std::cout << " Main Run loop " << std::endl;
   //StartCommandReceiver();
   deque<unsigned char> bufRead;
   // deque for events: add one event when new acqId is arrived: to be determined in reader
//      deque<eudaq::RawDataEvent *> deqEvent2;
   std::deque<eudaq::EventUP> deqEvent;

   const int bufsize = 1000*SINGLE_SKIROC_EVENT_SIZE;
   // copy to C array, then to vector
   char buf[bufsize]; //buffer to read from TCP socket
   //while (!_terminated) {

   while (!_terminated ) {

     if (_reader) {
       SetStatusTag("lastROC", std::to_string(dynamic_cast<SiReader*>(_reader)->getCycleNo()));
       //	 // SetStatusTag("lastTrigN", std::to_string(dynamic_cast<ScReader*>(_reader)->getTrigId() - getLdaTrigidOffset()));
     }
      // wait until configured and connected
      std::unique_lock<std::mutex> myLock(_mufd);
      int size = 0;
      if ((_fd <= 0) && _redirectedInputFileName.empty()) {
         myLock.unlock();
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         continue;
      }
      if ((!_running) && (!_redirectedInputFileName.empty())) break; //stop came during read of a file. In this case exit before reaching the end of the file.
#ifdef _WIN32
      if (_redirectedInputFileName.empty()) {
         size = recv(_fd, buf, bufsize, 0);
      } else {
         size = _redirectedInputFstream.read(buf, bufsize).gcount();
      }
#else
      size = ::read(_fd, buf, bufsize);   //blocking. Get released when the connection is closed from Labview
#endif // _WIN32
      if (size > 0) {
         //_last_readout_time = std::time(NULL);
         if (_writeRaw && _rawFile.is_open()) _rawFile.write(buf, size);

         // C array to vector
         copy(buf, buf + size, back_inserter(bufRead));

	 if (_reader) _reader->Read(bufRead, deqEvent);

         // send events : remain the last event
         if (_stopped) continue;   //sending the events to a stopped data collector will crash producer, but we still need to flush the TCP buffers

	 try {
	   sendallevents(deqEvent, 1);
         } catch (Exception& e) {
            _stopped = 1;
            std::cout << "Cannot send events! sendallevents1 exception: " << e.what() << endl;
         }
         continue;
      } else
         if (size == -1) {
	   if (errno == EAGAIN) continue;
            std::cout << "Error on read: " << errno << " Disconnect and going to the waiting mode." << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            break;
         } else
            if (size == 0) {
	      std::cout << "Socket disconnected. going to the waiting mode." << endl;
	      std::cout << "sending the remaining events" << std::endl;
	      _reader->DumpCycle(deqEvent, true);
	      SetStatusTag("verylastROC", std::to_string(dynamic_cast<SiReader*>(_reader)->getCycleNo()));
	      break;
            }

      //_running && ! _terminated
      //  std::cout << "sending the rest of the event" << std::endl;
      if (!_stopped) {
	try {
	  cout<<"sending events sendallevents(deqEvent, 0)"<<endl;
	  sendallevents(deqEvent, 0);
	} catch (Exception& e) {
	  _stopped = 1;
	  std::cout << "Cannot send events! sendallevents1 exception: " << e.what() << endl;
	}
      }
   }
   
   _stopped = 1;
   bufRead.clear();
   deqEvent.clear();
   EUDAQ_INFO_STREAMOUT(
			"Completed Run " + std::to_string(GetRunNumber()) +", Events=", std::cout,std::cerr);
#ifdef _WIN32
   closesocket(_fd);
#else
   close(_fd);
#endif
   _fd = -1;
   if (!_redirectedInputFileName.empty()) { //signalling during reprocessing
       std::this_thread::sleep_for(std::chrono::milliseconds(3000)); //extra time for the data collectors to process the events
       SetStatusTag("ReprocessingFinished", std::to_string(1));
    }
}




