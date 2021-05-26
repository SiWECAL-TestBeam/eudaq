#include "KLauS_ROPackets.h"
#include <stdio.h>

bool LDA_PKT::IsFooterOK(){
	//printf("Footer: %2.2x %2.2x\n",buf[HDR_len()+10 - 2],buf[HDR_len()+10 - 1]);
	return (buf[HDR_len()+10 - 2] == 0xab && buf[HDR_len()+10 - 1] == 0xab);
}


void LDA_PKT::PrintLDAHDR(){
	printf("\n-----------\n");
	printf("TCP/Length = 0x%4.4x\n",len);
	printf("HDR/Length = 0x%4.4x\n",HDR_len());
	printf("HDR/ROC    = 0x%2.2x\n",HDR_ROC());
	printf("HDR/LDA    = 0x%2.2x\n",HDR_LDA());
	printf("HDR/PORT   = 0x%2.2x\n",HDR_PORT());
	printf("HDR/status = 0x%4.4x\n",HDR_status());
}
void LDA_PKT::PrintRoPKTHDR(){
	printf("\n-----------\n");
	printf("PKT/Type   = 0x%4.4x\n",PKT_Type());
	printf("PKT/Tmod   = 0x%4.4x\n",PKT_TypeMod());
	if(IsDIFResponseData())
		printf(" [DIF RESPONSE]\n");
	else
		printf(" [ASIC DATA]\n");

	printf("PKT/DLEN   = 0x%4.4x -> %d+8 bytes\n",PKT_len16(),PayloadLen8());
	printf("PKT/chain  = 0x%2.2x\n",PKT_chain());
	printf("PKT/asic   = 0x%2.2x\n",PKT_asic());
	printf("PKT/order  = 0x%4.4x\n",PKT_order());
	printf("PKT/DIF-ID = 0x%4.4x\n",PKT_DIFID());
	printf("PKT/PID    = 0x%4.4x\n",PKT_PID());

};

void LDA_PKT::Reorder(){
	auto p=PayloadStart();
	while(p+2<=PayloadEnd()){
		char x=*p;
		p[0]=p[1];
		p[1]=x;
		p+=2;
	}
}

/*
void LDAInterface::Ruler(){
printf("Ruler\n"\
  "length   [         ]\n"\
  "type                 []\n"\
  "port                   |[]\n"\
  "pktID                  |   [   ]\n"\
  "type_mod               |        |[   ]\n"\
  "spec                   |        |      [   ]\n"\
  "data_len               |        |           |[   ]\n"\
  "fake CRC               |        |           |      [   ]\n");
printf("         ");

}
void LDAInterface::RxRuler(){
printf("RxRuler\n"\
  "length   [   ]\n"\
  "ROC            []\n"\
  "--               |[]\n"\
  "LDA#                 []\n"\
  "PORT#                  |[]\n"\
  "stat (LO)                  []\n"\
  "stat (HI)                    |[]\n");
printf("         ");

}
*/


