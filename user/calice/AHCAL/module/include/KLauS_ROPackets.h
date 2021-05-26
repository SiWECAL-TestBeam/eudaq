/*
 *  Header definitions of DIF->LDA->Labview->EUDAQ packets
 *  5/2021 K.Briggl (kbriggl@kip.uni-heidelberg.de)
 *
 */
#include <stdint.h>
#ifndef KLAUS_ROPACKETS_H_
#define KLAUS_ROPACKETS_H_

#include <deque>
#include <array>
struct LDA_PKT{
private:
	std::deque<unsigned char>& buf;
	uint32_t len;
	std::array<unsigned char,24> headers;
public:
	LDA_PKT(std::deque<unsigned char>& _buf,uint32_t _len):buf(_buf),len(_len){
		for(int i=0;i<24;i++){
			headers[i]=_buf[i];
		}
	};

//LDA PACKET HEADER
	uint16_t HDR_len(){return *((uint16_t*)&headers[2]);};
	uint8_t  HDR_ROC(){return *((uint8_t*)&headers[4]);};
	uint8_t  HDR_LDA(){return *((uint8_t*)&headers[6]);};
	uint8_t  HDR_PORT(){return *((uint8_t*)&headers[7]);};
	uint16_t HDR_status(){return *((uint16_t*)&headers[8]);};
	bool IsFooterOK();
//RO DATA  HEADER
	//Parse DIF packet assuming readout data packet
	uint16_t PKT_Type(){return *((uint16_t*)&headers[10]);};
	uint16_t PKT_PID(){return *((uint16_t*)&headers[12]);};
	uint16_t PKT_TypeMod(){return *((uint16_t*)&headers[14]);};
	uint16_t PKT_len16(){return *((uint16_t*)&headers[16]);};
	uint8_t  PKT_asic(){return *((uint8_t*)&headers[18]);};
	uint8_t  PKT_chain(){return *((uint8_t*)&headers[19]);};
	uint16_t  PKT_order(){return *((uint16_t*)&headers[20]);};
	uint16_t  PKT_DIFID(){return *((uint16_t*)&headers[22]);};

	uint16_t PayloadLen8(){return PKT_len16()*2-8;};
	//Payload pointers
	std::deque<unsigned char>::iterator PayloadStart(){return buf.begin()+24;};
	std::deque<unsigned char>::iterator PayloadEnd(){return buf.begin()+24+PayloadLen8();};

	const char* PacketTypeStr();
	bool IsDIFResponseData(){return (PKT_Type()==0xcc02);};
	bool IsROData(){return (PKT_Type()==0x4341);};
	bool IsK5HitData(){return (PKT_Type()==0x4341) && (PKT_TypeMod()==0x4149);};
	bool IsK6HitData(){return (PKT_Type()==0x4341) && (PKT_TypeMod()==0x4150);};
	bool IsCECData(){return (PKT_Type()==0x4341) && (PKT_TypeMod()==0x4151);};

	void PrintLDAHDR();
	void PrintRoPKTHDR();
	void Reorder();
};

#endif
