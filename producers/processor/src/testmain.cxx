
#include"Processor.hh"
// #include"ProcessorManager.hh"
#include"RawDataEvent.hh"


#include <chrono>
#include <thread>
#include <iostream>


using namespace eudaq;

int main(int argn, char **argc){
  PSSP ptt0 = Processor::MakeShared("TimeTriggerPS", "SYS:PSID=0");
  PSSP pdd0 = Processor::MakeShared("DummyDevicePS", "SYS:PSID=10");
  // PSSP pdd1 = Processor::MakeShared("DummyDevicePS", "SYS:PSID=11");
  PSSP prd0 = Processor::MakeShared("RandomDelayPS", "SYS:PSID=20");
  PSSP pst0 = Processor::MakeShared("SyncByTimestampPS", "SYS:PSID=30");
  PSSP pep0 = Processor::MakeShared("ExamplePS", "SYS:PSID=40");

  
  ptt0 + "TRIGGER"
    >>pdd0 + "DUMMYDEV" + "TRIGGER"
    >>prd0 + "DUMMYDEV" + "TRIGGER"
    >>pst0 + "SYNC"
    >>pep0
    ;

  // ptt0 + "TRIGGER"
  //   >>pdd1 + "DUMMYDEV"
  //   >>prd0
  //   ;
  
  ptt0 + "TRIGGER"
    >>prd0
    ;  

  pst0<<"N_STREAM=2";

  // ptt0->Print(std::cout);
  // pdd0->Print(std::cout);
  // // pdd1->Print(std::cout);
  // prd0->Print(std::cout);
  // pst0->Print(std::cout);
  // pep0->Print(std::cout);
  
  ptt0<<"SYS:HB:RUN";
  ptt0<<"SYS:PD:RUN";  
  std::cout << "press any key to exit...\n"; getchar();
  ptt0.reset();
}

// "EventFileReaderPS(SYS:PSID=0;FILE=/opt/eudaq/run000703.raw;SYS:SLEEP=1000;SYS:PD:RUN)"
// "EventReceiverPS(SYS:PSID=7;SETSERVER=tcp://40000;SYS:PD:RUN)"
// "EventSenderPS(SYS:PSID=6;CONNECT=Producer,p6,tcp://127.0.0.1:40000)"
// "ExamplePS(SYS:PSID=2222)"
// "EV(ADD=_DET)"

// std::cout<<"xxxxxxx"<<std::endl;
// {uint32_t i; std::cin>>i;}

// >>"DetEventUnpackInsertTimestampPS(SYS:PSID=100)"
// >>"EV(ADD=_RAW)"
// >>"SyncByTimestampPS(SYS:PSID=101)"
// >>"ExamplePS(SYS:PSID=11)"
// >>"EV(ADD=_RAW)"
// >>"ExamplePS(SYS:PSID=12)"
// >>"EventSenderPS(SYS:PSID=6;CONNECT=Producer,p6,tcp://127.0.0.1:40000)"

  
// PSSP p1 = Processor::MakePSSP("EventFileReaderPS", "SYS:PSID=1;FILE=../data/run000703.raw;SYS:SLEEP=1000");  

// PSSP p1 = Processor::MakePSSP("TimeTriggerPS", "SYS:PSID=1");
// PSSP p2 = Processor::MakePSSP("DummyDevicePS", "SYS:PSID=2");

// PSSP p3 = Processor::MakePSSP("ExamplePS", "SYS:PSID=3");
