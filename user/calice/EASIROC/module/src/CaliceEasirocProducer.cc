#include "eudaq/Producer.hh"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#ifndef _WIN32
#include <sys/file.h>
#endif
#include <easiroc.h>

extern unsigned int slowdata[sizeByte_SC];
extern unsigned int ReadSCdata[sizeByte_RSC];
extern unsigned int Probedata[sizeByte_PSC];
extern unsigned int Pindata[sizeByte_PIN];

class Udpsetper {
   public:
      float DACdata[63];
      float HVC_1;
      float HVC_2;
      float ADC2HV;
      float ADC2uA;
      float ADC2V;
      float ADC2K;
      Udpsetper() {
         HVC_1 = 415;
         HVC_2 = 780;
         //HVDAC =HVC_1 * HV + HVC_2;            convert HV to DAC bit
         ADC2HV = 0.00208;
         //rd_data = ADC2HV * rd_data;          convert ADC bit to HV
         ADC2uA = 0.034;
         //rd_data = ADC2uA * rd_data;          convert ADC bit to HVcurrent
         ADC2V = 0.0000685;
         //rd_data = ADC2V * rd_data;             convert ADC bit to inputDAC Voltage
         ADC2K = 4500;
         //rd_data = ADC2K * rd_data/65535/2.4; convert ADC bit to TEMP(K)
      }
};

class CaliceEasirocProducer: public eudaq::Producer {
   public:
      CaliceEasirocProducer(const std::string & name, const std::string & runcontrol);
      void DoInitialise() override;
      void DoConfigure() override;
      void DoStartRun() override;
      void DoStopRun() override;
      void DoTerminate() override;
      void DoReset() override;
      void Mainloop();
      int ADCOneCycle_wHeader(Exchanger* exchange, std::ofstream& file);
      void ADCStop(Exchanger* exchange);
      int DebugFPGA(Exchanger* exchange);
      int TransmitSC(Exchanger* exchange);
      int TransmitReadSC(Exchanger* exchange);
      void setuplog(Exchanger* exchange, Udpsetper* udpper);
      void udplog(Exchanger* exchange, Udpsetper* udpper, std::string file_name);
      void update_counter_modulo(unsigned int &counter, const unsigned int newvalue_modulo, const unsigned int modulo_bits, const unsigned int max_backwards);
      void openConnections();
      void closeConnections();
      void OpenRawFile();
      void hvset(Exchanger * exchange, Udpsetper * udpper, const double HV);
      void hvstatus(Exchanger * exchange, Udpsetper * udpper);
      void rampHV(Exchanger* exchange, Udpsetper* udpper, const double HV);

      static const uint32_t m_id_factory = eudaq::cstr2hash("CaliceEasirocProducer");

   private:
      bool m_flag_ts;
      bool m_flag_tg;
      //      FILE* m_file_lock;
      //      std::chrono::milliseconds m_ms_busy;
      std::thread m_thd_run;
      bool m_exit_of_run;

      unsigned int m_tcpport; //tcp port of easiroc
      unsigned int m_tcptimeout;
      unsigned int m_udpport; //udp port of easiroc
      std::string m_ip; //IP address of the easiroc module
      unsigned int m_daq_mode; //a proprietary DAQ mode of the easiroc module. 6 by default

      bool m_writeRaw; //whether to dump the raw data or not
      std::string m_rawFilename; //file name
      bool m_writerawfilename_timestamp; //whether to append timestamp to the file name
      std::ofstream m_rawFile; //output raw file stream

      std::string m_redirectedInputFileName; // if set, this filename will be used as input
      std::ifstream m_redirectedInput;

      int m_printEvents; // level of event prints. 1=event info, 2= eudaq_events
      double m_HV; //high voltage value

      int m_AHCALBXIDWidth;
      int m_AHCALBXID0Offset;
      //easiroc old module variables
      //int ForceStop = 0;
      int EndADC = 0;

      unsigned int m_lastTrigN;
      unsigned int m_lastCycleN;
      unsigned int m_lastTDCVal;
      unsigned int m_lastEventN;
      Exchanger* m_exchanger;      //TCP communication library
      Udpsetper* m_udpper;      //UDP stuff??
      int m_skipConOpen;      //skips opening of the connection
};
namespace {
   auto dummy0 = eudaq::Factory<eudaq::Producer>::
         Register<CaliceEasirocProducer, const std::string&, const std::string&>(CaliceEasirocProducer::m_id_factory);
}
CaliceEasirocProducer::CaliceEasirocProducer(const std::string & name, const std::string & runcontrol)
:
      eudaq::Producer(name, runcontrol), m_exit_of_run(false) {
   std::cout << "CaliceEasirocProducer created" << std::endl;
}
void CaliceEasirocProducer::DoInitialise() {
   auto ini = GetInitConfiguration();
   ini->Print(std::cout);
   m_ip = ini->Get("IPAddress", "");
   if (m_ip == "") EUDAQ_THROW("Unable to obtain IPAddress. check the ini file and name of the producer");
   m_tcpport = ini->Get("TCP_Port", 24);
   m_tcptimeout = ini->Get("TCP_Timeout", 3);
   m_udpport = ini->Get("UDP_Port", 4660);
   m_daq_mode = ini->Get("DAQ_Mode", 6);
   m_HV = ini->Get("HV", 10.0);
   m_skipConOpen = ini->Get("SkipConnectionOpen", 0);
   if (!m_skipConOpen) openConnections();
}

void CaliceEasirocProducer::DoConfigure() {
   auto conf = GetConfiguration();
   conf->Print(std::cout);
   m_flag_ts = conf->Get("EX0_ENABLE_TIEMSTAMP", 0);
   m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
   if (!m_flag_ts && !m_flag_tg) {
      EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
      m_flag_ts = false;
      m_flag_tg = true;
   }
   m_printEvents = conf->Get("PrintEvents", 0);
   m_writeRaw = conf->Get("WriteRawOutput", 0);
   m_rawFilename = conf->Get("RawFileName", "easiroc_run%d");
   m_writerawfilename_timestamp = conf->Get("WriteRawFileNameTimestamp", 0);
   m_redirectedInputFileName = conf->Get("RedirectInputFromFile", "");
//   EUDAQ_DC="dc1" # list of the data collectors
//   WriteRawOutput = 1
//   #RawFileName = "../data/Run_%05d"
//   RawFileName = "data/ahcalRaw_%05d"
   m_AHCALBXIDWidth = conf->Get("AHCALBXIDWidth", 160);
   m_AHCALBXID0Offset = conf->Get("AHCALBXID0Offset", 2198);
}
void CaliceEasirocProducer::DoStartRun() {
   m_exit_of_run = false;
   m_lastCycleN = 0;
   m_lastTDCVal = 0;
   m_lastTrigN = 0;
   m_lastEventN = 0;
   m_thd_run = std::thread(&CaliceEasirocProducer::Mainloop, this);
}
void CaliceEasirocProducer::DoStopRun() {
   m_exit_of_run = true;
   if (m_thd_run.joinable())
      m_thd_run.join();
}
void CaliceEasirocProducer::DoReset() {
   m_exit_of_run = true;
   if (m_thd_run.joinable())
      m_thd_run.join();
   m_thd_run = std::thread();
   m_exit_of_run = false;
}
void CaliceEasirocProducer::DoTerminate() {
   m_exit_of_run = true;
   if (m_thd_run.joinable())
      m_thd_run.join();
//   fclose (m_file_lock);
}

//void CaliceEasirocProducer::Mainloop() {
//   auto tp_start_run = std::chrono::steady_clock::now();
//   uint32_t trigger_n = 0;
//   uint8_t x_pixel = 16;
//   uint8_t y_pixel = 16;
//   std::random_device rd;
//   std::mt19937 gen(rd());
//   std::uniform_int_distribution<uint32_t> position(0, x_pixel * y_pixel - 1);
//   std::uniform_int_distribution<uint32_t> signal(0, 255);
//   while (!m_exit_of_run) {
//      auto ev = eudaq::Event::MakeUnique("EasirocRaw");
//      auto tp_trigger = std::chrono::steady_clock::now();
//      auto tp_end_of_busy = tp_trigger + m_ms_busy;
//      if (m_flag_ts) {
//         std::chrono::nanoseconds du_ts_beg_ns(tp_trigger - tp_start_run);
//         std::chrono::nanoseconds du_ts_end_ns(tp_end_of_busy - tp_start_run);
//         ev->SetTimestamp(du_ts_beg_ns.count(), du_ts_end_ns.count());
//      }
//      if (m_flag_tg)
//         ev->SetTriggerN(trigger_n);
//
//      std::vector<uint8_t> hit(x_pixel * y_pixel, 0);
//      hit[position(gen)] = signal(gen);
//      std::vector<uint8_t> data;
//      data.push_back(x_pixel);
//      data.push_back(y_pixel);
//      data.insert(data.end(), hit.begin(), hit.end());
//
//      uint32_t block_id = m_plane_id;
//      ev->AddBlock(block_id, data);
//      SendEvent(std::move(ev));
//      trigger_n++;
//      std::this_thread::sleep_until(tp_end_of_busy);
//   }
//}
void CaliceEasirocProducer::ADCStop(Exchanger* exchange) {
   unsigned int data = 0;
   std::cout << "ADC exit process" << std::endl;
   data += 16 << 24;
   data += 100 << 16;
   if (m_redirectedInputFileName.empty()) exchange->tcp_send(data);
   std::this_thread::sleep_for(std::chrono::seconds(1));

   //data = 0;
   //data += 100 << 16;
   //exchange->tcp_send(data);
   //usleep(10000);

   return;
}

void CaliceEasirocProducer::update_counter_modulo(unsigned int &counter, const unsigned int newvalue_modulo, const unsigned int modulo_bits,
      const unsigned int max_backwards) {
   unsigned int newvalue = counter - max_backwards;
   unsigned int mask = (1 << modulo_bits) - 1;
   if ((newvalue & mask) > (newvalue_modulo & mask)) {
      newvalue += (1 << modulo_bits);
   }
   counter = (newvalue & (~mask)) | (newvalue_modulo & mask);
}

void CaliceEasirocProducer::openConnections()
{
   //----------- initialize -----------------
   m_exchanger = new Exchanger(m_ip.c_str(), m_tcpport, m_udpport);
   m_exchanger->CreateTCPSock();
   m_exchanger->CreateUDPSock();
   m_exchanger->SetTCPTimeOut(m_tcptimeout, 0);

   PrepareFPGA();
   DebugFPGA(m_exchanger);
   std::cout << "ASIS Initialize : Done" << std::endl;
   std::cout << std::endl;
   std::this_thread::sleep_for(std::chrono::milliseconds(10));

   m_exchanger->WriteData(37888);
   PrepareSC(1);
   TransmitSC(m_exchanger);
   std::cout << "Slow Control chip1 : Done" << std::endl;
   std::cout << std::endl;
   PrepareReadSC(1);
   TransmitReadSC(m_exchanger);
   std::cout << "Read Slow Control chip1 : Done" << std::endl;
   std::cout << std::endl;

   m_exchanger->WriteData(21504);
   PrepareSC(2);
   TransmitSC(m_exchanger);
   std::cout << "Slow Control chip2 : Done" << std::endl;
   std::cout << std::endl;
   PrepareReadSC(2);
   TransmitReadSC(m_exchanger);
   std::cout << "Read Slow Control chip2: Done" << std::endl;
   std::cout << std::endl;

   m_exchanger->WriteData(5120); // TODO no idea what it does
   {
      unsigned int sidata = 31;
      sidata = sidata << 16;
      unsigned int data = m_daq_mode;
      data = data << 8;
      sidata += data;
      if (-1 == m_exchanger->WriteData(sidata)) {
         EUDAQ_THROW("failed to set the daqmode.");
      }
      std::cout << "#D : DAQ mode is " << m_daq_mode << std::endl;
      std::cout << std::endl;
   }

   //UDP Initialize-------------------------------------------------------
   std::cout << "Debug UDP 1" << std::endl;
   m_udpper = new Udpsetper();
   std::cout << "Debug UDP 2" << std::endl;
   //Windows has some instability issues with sending over UDP
   m_exchanger->udp_send(0x00000012, 248); //Set ADC rate to 50Hz
   m_exchanger->udp_send(0x0000001f, 0);
   std::cout << "Debug UDP 3" << std::endl;

   rampHV(m_exchanger, m_udpper, m_HV);
}

void CaliceEasirocProducer::closeConnections()
{
   if (m_redirectedInputFileName.empty()) {
      ADCStop(m_exchanger);
      EndADC = 1;
      int abort = 0;
      while (0 == ADCOneCycle_wHeader(m_exchanger, m_rawFile)) {
         std::this_thread::sleep_for(std::chrono::microseconds(10000));
         if (abort == 50) {
            ADCStop(m_exchanger);
            abort = 0;
         }
         std::cout << "dummy data" << std::endl;
         ++abort;
      }
      EndADC = 0;
      //ForceStop = 0;
      std::cout << "End ADC" << std::endl;

#ifndef _WIN32
      if (m_redirectedInputFileName.empty()) m_exchanger->CloseUDPSock();
#endif // !_WIN32
      if (m_redirectedInputFileName.empty()) m_exchanger->CloseTCPSock();
      std::cout << "All :: Connection Close" << std::endl;
      std::cout << "DEBUG: Terminate 1" << std::endl;
      delete m_exchanger;
      std::cout << "DEBUG: Terminate 2" << std::endl;
      delete m_udpper;
      std::cout << "DEBUG: Terminate 3" << std::endl;
   }
}

int CaliceEasirocProducer::ADCOneCycle_wHeader(Exchanger* exchange, std::ofstream& file) {

   unsigned int DataBuffer[1000];
   memset(DataBuffer, 0, sizeof(DataBuffer));
   unsigned int *ptr_DataBuf = DataBuffer;
   unsigned int TotalRecvByte = 0;
   unsigned int TotalRecvEvent = 0;

   static const int NofHeader = 3;
   static unsigned int sizeHeader = NofHeader * sizeof(int);
   static unsigned int wordsize = sizeof(int);

   unsigned int Header[NofHeader] = { 1, 1, 1 };
   unsigned int header1[1] = { 1 };
   unsigned int header2[1] = { 1 };
   unsigned int header3[1] = { 1 };
   int ret = 0;
   while (true) {
      //       std::cout << "." << std::flush;
      if (m_redirectedInputFileName.empty()) {
         ret = exchange->tcp_multi_recv((char*) header1, &wordsize);
      } else {
         m_redirectedInput.read((char*) header1, wordsize);
         ret = (m_redirectedInput) ? 1 : 0;
      }

      if (ret <= 0) {
         if ((EndADC == 1) || m_exit_of_run) {
            std::cout << "ERROR read 1" << std::endl;
            return -1;
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      //std::cout << std::hex << header1[0] << std::endl;
      if (header1[0] == 0xFFFF7368) {
         break;
      }
      //else{
      //   std::cout << "dump data " << header1[0] << std::endl;
      //}
   }
   if (m_redirectedInputFileName.empty()) {
      ret = exchange->tcp_multi_recv((char*) header2, &wordsize);
   } else {
      m_redirectedInput.read((char*) header2, wordsize);
      ret = (m_redirectedInput) ? 1 : 0;
   }
   if (ret <= 0 && ((EndADC == 1) || m_exit_of_run)) {
      std::cout << "ERROR read 2" << std::endl;
      return -1;
   }

   if (m_redirectedInputFileName.empty()) {
      ret = exchange->tcp_multi_recv((char*) header3, &wordsize);
   } else {
      m_redirectedInput.read((char*) header3, wordsize);
      ret = (m_redirectedInput) ? 1 : 0;
   }
   if (ret <= 0 && ((EndADC == 1) || m_exit_of_run)) {
      std::cout << "ERROR read 3" << std::endl;
      return -1;
   }
//std::cout << " Header1 : " << std::hex << header1[0] << std::endl;
//std::cout << " Header2 : " << std::hex << header2[0] << std::endl;
//std::cout << " Header3 : " << std::hex << header3[0] << std::endl;
   Header[0] = header1[0];
   Header[1] = header2[0];
   Header[2] = header3[0];
   if (header1[0] != 0xFFFF7368) {
      std::cerr << " Fatal ADC ERROR : HEADER" << std::endl;
      std::cerr << " Header1 : " << std::hex << header1[0] << std::endl;
      std::cerr << " Header2 : " << std::hex << header2[0] << std::endl;
      std::cerr << " Header3 : " << std::hex << header3[0] << std::endl;
      exit(-1);
   } else {
      unsigned int sizeData = (sizeof(int) * header2[0] & 0xffff);
      const unsigned int NofWord = (header2[0] & 0xffff);      //doesn't work on win
      unsigned int OneData[1024];
      if (m_redirectedInputFileName.empty()) {
         ret = exchange->tcp_multi_recv((char*) OneData, &sizeData);
      } else {
         m_redirectedInput.read((char*) OneData, sizeData);
         ret = (m_redirectedInput) ? 1 : 0;
      }
      if (ret <= 0 && EndADC == 1) {
         std::cout << "ERROR read 4" << std::endl;
         return -1;
      }
      /*
       for(int i = 0; i<NofWord; ++i){//
       if (i%4 ==0)
       {
       std::cout << std::hex << OneData[i] <<" "<< std::hex << OneData[i+1] <<" "
       << std::hex << OneData[i+2] <<" "<< std::hex << OneData[i+3] <<" "<< std::endl;
       }
       }
       */
      memcpy(ptr_DataBuf, Header, sizeHeader);
      ptr_DataBuf += NofHeader;

      memcpy(ptr_DataBuf, OneData, sizeData);
      ptr_DataBuf += sizeData / sizeof(int);

      TotalRecvByte += sizeHeader + sizeData;
      ++TotalRecvEvent;
   }
   if (EndADC != 1) {
      for (unsigned int i = 0; i < TotalRecvByte / sizeof(int); ++i) {
         unsigned int buffer = DataBuffer[i];
//         std::cout<<eudaq::to_hex(buffer,8)<<" ";
         if (m_writeRaw) file.write((char*) &buffer, sizeof(int));
      }
      // DEBUG prints
      //      for (int i = 0; i < 40; i++) {
      //         std::cout << eudaq::to_hex((unsigned char) *(((unsigned char *) DataBuffer) + i), 2) << " ";
      //      }
      //      std::cout << std::endl;

      //cherrypick the information from the data
      unsigned int trig = (DataBuffer[2] >> 16) & 0xFFF;
      unsigned int cycle = DataBuffer[3] & 0xFFFFF;
      unsigned int tdc_val = DataBuffer[4] & 0xFFFFF;
      unsigned char tdc_bit = (DataBuffer[4] >> 20) & 0x1;
      // fix overflowing
      if ((m_lastCycleN & 0xFFFFF) != (cycle)) m_lastTDCVal = 0;      //clear the tdc value for the next readout cycle
      update_counter_modulo(m_lastTrigN, trig, 12, 1);
      update_counter_modulo(m_lastCycleN, cycle, 20, 1);
      update_counter_modulo(m_lastTDCVal, tdc_val, 20, 1);
      if (m_printEvents) {
         if ((trig % 30) == 0) std::cout << "#trig\ttrg_mod\tcycle\tcyc_mod\ttdc\ttdc_mod\tcyc_bit" << std::endl;
         std::cout << m_lastTrigN << "\t" << trig << "\t" << m_lastCycleN << "\t" << cycle << "\t" << m_lastTDCVal << "\t" << tdc_val << "\t" << tdc_bit
               << std::endl;
      }

      eudaq::EventUP ev = eudaq::Event::MakeUnique("CaliceObject");      //"EasirocRaw";
      std::string s = "EudaqDataHodoscope"; //"EudaqDataHodoscope";
      ev->AddBlock(0, s.c_str(), s.length());
      s = "i:header,i:size,i:trigNx65536,i:cycle_num,i:tdc_and_bit,i:data[64]";
      ev->AddBlock(1, s.c_str(), s.length());

      unsigned int unixtime[1];
//      auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
//      unixtime[0] = std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
      unixtime[0] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      ev->AddBlock(2, unixtime, sizeof(unixtime));

      ev->SetTriggerN(m_lastTrigN);
      ev->SetTag("ROC", m_lastCycleN);
      ev->SetTag("BXID", ((int) m_lastTDCVal - m_AHCALBXID0Offset) / m_AHCALBXIDWidth);
      ev->SetTag("TrigBxidTdc", ((int) m_lastTDCVal - m_AHCALBXID0Offset) % m_AHCALBXIDWidth);
      ev->SetTag("TDC", m_lastTDCVal);
      ev->SetTag("SRC", GetName());
      ev->SetEventN(++m_lastEventN);
      // if ((((int) m_lastTDCVal - m_AHCALBXID0Offset) / m_AHCALBXIDWidth) > 10000) {
      //    std::cout << "#too big BXID: ROC=" << m_lastCycleN << ", BXID=" << (m_lastTDCVal - m_AHCALBXID0Offset) / m_AHCALBXIDWidth << std::endl;
      // }
      //TODO ev->SetDeviceN(1);
      ev->AddBlock(3, DataBuffer, TotalRecvByte);
      if (m_lastTrigN == 0) ev->SetBORE();
      if (m_printEvents > 1) ev->Print(std::cout);
//      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      SendEvent(std::move(ev));

      //update the status of the producer
      SetStatusTag("lastROC", eudaq::to_string(m_lastCycleN));
      SetStatusTag("lastTrig", eudaq::to_string(m_lastTrigN));
   }
   return 0;
}

int CaliceEasirocProducer::DebugFPGA(Exchanger* exchange) {
   unsigned int buffer = 0;
   buffer += 0 << 16;
   buffer += (Pindata[0] & 255) << 8;
   if (-1 == exchange->WriteData(buffer)) {
      exit(-1);
   }

   for (int i = 1; i < 5; ++i) {
      buffer = 0;
      if (i == 4) {
         buffer += 5 << 16;
      } else {
         buffer += i << 16;
      }
      buffer += ((Pindata[1] >> (i - 1) * 8) & 255) << 8;
      if (-1 == exchange->WriteData(buffer)) {
         exit(-1);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      //usleep(1);
   }
   return 0;
}

int CaliceEasirocProducer::TransmitSC(Exchanger* exchange) {
   unsigned int data = 0;
//Set SC mode -----------------------------------------------------------
   data = 0;
   data += 1 << 16;
   data += 240 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

//SC start -------------------------------------------------------------
   data = 0;
   data += 10 << 16;
   data += (slowdata[0] & 255) << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

   for (int i = 1; i < 15; ++i) {
      for (int shift = 0; shift < 4; ++shift) {
         data = 0;
         data += 10 << 16;
         data += ((slowdata[i] >> 8 * shift) & 255) << 8;
         if (-1 == exchange->WriteData(data)) {
            exit(-1);
         }
         std::this_thread::sleep_for(std::chrono::microseconds(1));
         //usleep(1);
      }
   }

//StartCycle -----------------------------------------------------------
   data = 0;
   data += 1 << 16;
   data += 242 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

   data = 0;
   data += 1 << 16;
   data += 240 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

//Load SC --------------------------------------------------------------
   data = 0;
   data += 1 << 16;
   data += 241 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

   data = 0;
   data += 1 << 16;
   data += 240 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }
   return 0;
}

int CaliceEasirocProducer::TransmitReadSC(Exchanger* exchange) {
//SCA read ---------------------------------------------------------------
   unsigned int data = 0;

   for (int i = 0; i < 4; ++i) {
      data = 0;
      data += 12 << 16;
      data += ((ReadSCdata[0] >> i * 8) & 255) << 8;
      if (-1 == exchange->WriteData(data)) {
         exit(-1);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1));
      // usleep(1);
   }

//StartCycle ------------------------------------------------------------
   data = 0;
   data += 1 << 16;
   data += 242 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }

   data = 0;
   data += 1 << 16;
   data += 240 << 8;
   if (-1 == exchange->WriteData(data)) {
      exit(-1);
   }
   return 0;
}

void CaliceEasirocProducer::setuplog(Exchanger* exchange, Udpsetper* udpper) {
   std::vector<std::string> rdsufile, dataval;
   std::ifstream ifs("autoconfig/sulogfile.txt");
   std::string fntemp, datatemp, ofntemp;
   std::ostringstream os;
   while (ifs >> fntemp) {
      rdsufile.push_back(fntemp);
   }
   ifs.close();
   time_t now = time(NULL);
   struct tm *pnow = localtime(&now);

   os << pnow->tm_year + 1900 << pnow->tm_mon + 1 << pnow->tm_mday << "_" << pnow->tm_hour << pnow->tm_min << "_sulog";
   ofntemp = "./sulog/" + os.str();
#ifdef _WIN32
   CreateDirectory(ofntemp.c_str(), NULL);
#else
   mkdir(ofntemp.c_str(), 0775);
#endif // _WIN32

   for (int i = 0; i < rdsufile.size(); ++i)
         {
      fntemp = "setup/" + rdsufile[i];
      std::ifstream ifs2(fntemp.c_str());
      ofntemp = "sulog/" + os.str() + "/" + rdsufile[i];
      std::ofstream ofs(ofntemp.c_str());
      while (getline(ifs2, datatemp)) {
         ofs << datatemp << std::endl;
      }
      ifs2.close();
      ofs.close();
   }
   ofntemp = "./sulog/" + os.str() + "/udplog.txt";
   udplog(exchange, udpper, ofntemp);
   return;
}

void CaliceEasirocProducer::udplog(Exchanger* exchange, Udpsetper* udpper, std::string file_name) {
   double Bias = 0;
   double rd_data = 0;
   float DACdata[63];
   int MUX = 0;
   std::ofstream ofs(file_name.c_str());
   rd_data = exchange->read_madc(3);
   rd_data = udpper->ADC2HV * rd_data;
   Bias = rd_data;
   ofs << "Bias voltage : " << rd_data << " V " << std::endl;
   rd_data = exchange->read_madc(4);
   rd_data = udpper->ADC2uA * rd_data;
   ofs << "Bias current : " << rd_data << " uA " << std::endl;

   ofs << "Input DAC scan" << std::endl;
   int i = 0;
   for (i = 0; i < 32; i++) {
      if (i % 2 == 0 && i < 16) {
         MUX = 199 - i / 2;
      }
      else
      if (i % 2 == 1 && i < 16) {
         MUX = 207 - i / 2;
      }
      else
      if (i % 2 == 0 && i < 32 && i > 15) {
         MUX = 55 - (i - 16) / 2;
      }
      else
      if (i % 2 == 1 && i < 32 && i > 15) {
         MUX = 63 - (i - 16) / 2;
      }
      else
      if (i == 32) {
         MUX = 0;
      }
      exchange->udp_send(0x00000013, MUX);
      std::this_thread::sleep_for(std::chrono::microseconds(2000));
      //usleep(2000);

      ofs << std::left;
      if (i < 32) {
         rd_data = exchange->read_madc(1);     //Read ADC data
         //std::cout <<"MADC_data = "<< rd_data;
         rd_data = udpper->ADC2V * rd_data; //convert ADC bit to V
         ofs << "ch" << std::setw(3) << i << std::setw(8) << rd_data << " V " << std::setw(8) << Bias - rd_data << " V" << std::endl;
         rd_data = exchange->read_madc(2); //Read ADC data
         //std::cout <<"MADC_data = "<< rd_data;
         rd_data = udpper->ADC2V * rd_data; //convert ADC bit to V
         ofs << "ch" << std::setw(3) << i + 32 << std::setw(8) << rd_data << " V " << std::setw(8) << Bias - rd_data << " V" << std::endl;
      }
   }

   return;
}

void CaliceEasirocProducer::OpenRawFile() {
   uint32_t runNo = GetRunNumber();
// read the local time and save into the string myString
   time_t ltime;
   struct tm *Tm;
   ltime = time(NULL);
   Tm = localtime(&ltime);
   char file_timestamp[25];
   std::string myString;
   if (m_writerawfilename_timestamp == 1) {
      sprintf(file_timestamp, "_%02dp%02dp%02d_%02dp%02dp%02d.raw", Tm->tm_mday, Tm->tm_mon + 1, Tm->tm_year + 1900, Tm->tm_hour, Tm->tm_min, Tm->tm_sec);
      myString.assign(file_timestamp, 26);
   } else {
      myString = ".raw";
   }
   std::string _rawFilenameTimeStamp;
//if chosen like this, add the local time to the filename
   _rawFilenameTimeStamp = m_rawFilename + myString;
   char _rawFilename[256];
   sprintf(_rawFilename, _rawFilenameTimeStamp.c_str(), (int) runNo);
   m_rawFile.open(_rawFilename, std::ios::binary);
}

void CaliceEasirocProducer::hvset(Exchanger* exchange, Udpsetper* udpper, const double HV) {
   int HVDAC = 0;
   if (HV > 90) {
      std::cout << "errer" << std::endl;
      return;
   }
   HVDAC = udpper->HVC_1 * HV + udpper->HVC_2;        //change HV to DAC bit
   exchange->udp_send(0x00000010, HVDAC / 256);        //Set higher 8bit to FPGA reg
   exchange->udp_send(0x00000011, HVDAC % 256);        //lower 8bit
   exchange->udp_send(0x0000001e, 1); //Start DAC control
   return;
}
;
void CaliceEasirocProducer::hvstatus(Exchanger* exchange, Udpsetper* udpper) {
   double rd_data = 0;
   std::cout << "Bias voltage";
   rd_data = exchange->read_madc(3); //Read ADC data
   //std::cout <<"MADC_data = "<< rd_data;
   rd_data = udpper->ADC2HV * rd_data; //convert ADC bit to HV
   printf(" : %.2lfV\n", rd_data);
   rd_data = exchange->read_madc(4); //Read ADC data
   //std::cout <<"MADC_data = "<< rd_data;
   rd_data = udpper->ADC2uA * rd_data;   //convert ADC bit to HVcurrent
   printf("Bias current : %.2lfuA", rd_data);
   std::cout << std::endl;
   return;
}
void CaliceEasirocProducer::rampHV(Exchanger* exchange, Udpsetper* udpper, const double HV) {
   if (HV > 65.0) EUDAQ_THROW("HV is bigger than 65 V. This is a hard-coded limit, you have to recompile to get over it.");
   std::cout << "Ramping to " << HV << " V." << std::endl;
   double curHV = 0.0;
   while (HV > curHV) {
      hvset(exchange, udpper, curHV);
      std::cout << "current HV :" << curHV << "V" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
      hvstatus(exchange, udpper);
      curHV += 10.0;
   }
   hvset(exchange, udpper, HV);
   std::cout << "Now HV is set to:" << HV << "V" << std::endl;
   std::this_thread::sleep_for(std::chrono::milliseconds(5000));
   hvstatus(exchange, udpper);
}

void CaliceEasirocProducer::Mainloop() {
   unsigned int data = 0;
   int EventNum = 0;

   if (!m_redirectedInputFileName.empty()) {
      //m_redirectedInput = std::ifstream(m_redirectedInputFileName, std::ifstream::binary);
      //fix for a gcc <5, which doesn't allow to copy.
      //This possibly breaks windows build?
      m_redirectedInput.open(m_redirectedInputFileName, std::ifstream::in | std::ifstream::binary);
      if (m_redirectedInput) {
         m_redirectedInput.seekg(0, m_redirectedInput.end);
         int length = m_redirectedInput.tellg();
         m_redirectedInput.seekg(0, m_redirectedInput.beg);
         std::cout << "Redirecting input from " << m_redirectedInputFileName << "; File is " << length << "bytes long." << std::endl;
      } else {
         std::cout << "ERROR: file " << m_redirectedInputFileName << " was not successfully open. Exiting." << std::endl;
         return;
      }
   } else {
      //data taking related
      std::cout << "Debug 3" << std::endl;
      //------------- end of configuration ----------------------
      //setuplog(exchange, udpper);
      //m_rawFile(ofilename.c_str(), std::ios::binary);
      //std::ofstream file(ofilename.c_str(), std::ios::binary);
      std::cout << "Debug 4" << std::endl;
      data += 32 << 24;
      data += 100 << 16;
      m_exchanger->tcp_send(data);
      std::cout << "Debug 5" << std::endl;
   };
   if (m_writeRaw) {
      OpenRawFile();
   }
   while (!m_exit_of_run) {   //run until runcontrol decides to stop
      ADCOneCycle_wHeader(m_exchanger, m_rawFile);
      if (0 == m_lastEventN % 1000) {
         std::cout << "Event # " << std::dec << m_lastEventN << std::endl;
      }
   }

   if (m_redirectedInputFileName.empty()) {
      ADCStop(m_exchanger);
      EndADC = 1;
      int abort = 0;
      while (0 == ADCOneCycle_wHeader(m_exchanger, m_rawFile)) {
         std::this_thread::sleep_for(std::chrono::microseconds(10000));
         if (abort == 50) {
            ADCStop(m_exchanger);
            abort = 0;
         }
         std::cout << "dummy data" << std::endl;
         ++abort;
      }
      EndADC = 0;
      //ForceStop = 0;
      std::cout << "End ADC" << std::endl;
   } else{
      m_redirectedInput.close();
   }


   if (m_writeRaw) {
      m_rawFile.close();
   }
   std::cout << "DEBUG: Terminate 4" << std::endl;
}
