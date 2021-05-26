#include <stdio.h>
#include <stdlib.h>
#include "KLauS_Data.h"

////////////////////////////// KLauS_Hit //////////////////////////////////

void KLauS_Hit::PrintHeader(FILE* fd)
{
	fprintf(fd,"CHANNEL\tGS\tADC_10b\tADC_6b\tPIPE\tTime\n");
}

void KLauS_Hit::Print(FILE* fd)
{
	fprintf(fd,"%2.2u\t%1.1u\t%3.3u\t%2.2u\t%3.3u\t%d\n",
		channel,
		gainsel_evt,
		ADC_10b,
		ADC_6b,
		ADC_PIPE,
		this->GetTime()
	);
}

void  KLauS_Hit::Parse(std::deque<unsigned char>::iterator event,unsigned char asic)
{
	unsigned char	channelID;
	unsigned char	groupID;
	groupID=		( event[0] >> 4 ) & 0x0003;
	channelID=		( event[0] >> 0 ) & 0x000f;
	gainsel_evt	=	( event[1] >> 7 ) & 0x0001;
	gainsel_busy=		( event[1] >> 6 ) & 0x0001;
	ADC_10b=		((event[1] << 4 ) & 0x03f0) | ((event[2] >> 4) & 0x000f);
	ADC_6b=		        ( event[1] >> 0 ) & 0x003f;
	ADC_PIPE=               ( event[2] >> 0 ) & 0x00ff;
	time=			((event[3] << 8 ) & 0xff00) | ((event[4] << 0) & 0x00ff);

	ADC_10b= 	1023-ADC_10b;
	ADC_6b=		  63-ADC_6b;
	ADC_PIPE= 	 255-ADC_PIPE;
        
        // This is verfied, for T0 channel: groupID=3, channelID=0
        channel = 0xff;
        if(groupID==3&&channelID==0) channel = 36; // T0 channel
        else if(channelID<12) channel = 12*groupID + (int)channelID;
	chipID=asic;
}


//convert gray encoded data into binary representation [LSB : MSB]
inline unsigned short grayToBinary(unsigned short num) {
	unsigned short mask;
	for (mask = num >> 1; mask != 0; mask = mask >> 1)
		num = num ^ mask;
	return num;
}

long unsigned int	KLauS_Hit::GetTime() const{
	unsigned int res=grayToBinary(time) * 4*25;
	return res;
}

int KLauS_Hit::DiffTime(KLauS_Hit& evt) const{
	return (this->GetTime()-evt.GetTime());
}


/*
// this function is implemented in KLauS-6(a) 
// because the MC[2:0] is mistaken as MC[0:2],
// so that a remap is needed. Also compensates on-chip gray to binary, MC[2] inversion.
//MC offset must be set to zero on the chip; factory mode enabled
inline unsigned short	MapMC(unsigned short MC){
	const unsigned short map[]={6,1,2,5,4,3,0,7};
	return map[MC];
}


void  KLauS6_Hit::Parse(unsigned char* event,unsigned char asic)
{
	groupID         =	( event[0] >> 6 ) & 0x0003;     // evt[0]: (7:6)
	channelID       =	( event[0] >> 2 ) & 0x000f;     // evt[0]: (5:2)
	gainsel_evt	=	( event[0] >> 1 ) & 0x0001;     // evt[0]: (1:1)
	ADC_10b         =	((event[0] << 9 ) & 0x0200) | ((event[1] << 1) & 0x01fe) | ((event[2] >> 7) & 0x0001);
	ADC_6b          =	((event[0] << 5 ) & 0x0020) | ((event[1] >> 3) & 0x001f);
	ADC_PIPE        =       ((event[1] << 5 ) & 0x0007) | ((event[2] >> 3) & 0x001f);
	T_CC            =	((event[2] <<16 ) &0x70000) | ((event[3] << 8 )& 0x0ff00) | ((event[4] << 0)& 0x000ff);
        T_MC            =       ((event[5] >> 5 ) & 0x0007);
        T_FC            =       ((event[5] << 0 ) & 0x001f);

	ADC_10b         = 	1023    -       ADC_10b;
	ADC_6b          =	63      -       ADC_6b;
	ADC_PIPE        = 	255     -       ADC_PIPE;
	T_FC		= 	31	-	T_FC;	// correct
	T_MC		= 	MapMC(T_MC);	// correct
        
        // This is verfied, for T0 channel: groupID=3, channelID=0
        channel = 0xff;
        if(groupID==3&&channelID==15) channel = 36; // T0 channel
        else if(channelID<9) channel = 9*groupID + (int)channelID;
	
}
unsigned long	KLauS6_Hit::GetTime() const{
	unsigned short  FC = (T_FC + foffset)            & 0x1f;        // offset correction
	unsigned short  MC = (T_MC + moffset)            & 0x07;        // offset correction
	unsigned short  mctemp = MC>>1 &0x03;                                 // align MC according to FC
	unsigned long   cctemp = (T_CC%(1<<19))>>1;                     // align CC according to aligned-MC
	if(MC%2==1 && FC/8==0) mctemp = ((MC/2) + 1)     & 0x03;
	if(MC%2==0 && FC/8==3) mctemp = ((MC/2) - 1 + 4) & 0x03;

	if(T_CC%2==1 && mctemp==0) cctemp = (cctemp + 1) & 0x1ffff;
	if(T_CC%2==0 && mctemp==3) cctemp = (cctemp + (1<<18) - 1) & 0x1ffff;
	return (cctemp<<7)+(mctemp<<5)+FC;
}
*/

////////////////////////////// klaus_acquisition //////////////////////////////////

////////////////////////////// klaus_cec_event //////////////////////////////////
void klaus_cec_data::Fill(unsigned char* buf, unsigned char _asic){
	asic = _asic;
	if(asic>3) return;
	this->Now();
	//decompose results for each channel
	//TODO: not yet implemented per channel, needs modification in data format
	// TODO: this code should be more generic
	cnts[asic*36+0]=(0x100&(buf[40]<<8)) | (0xFF&(buf[41]>>0));
	cnts[asic*36+1]=(0x180&(buf[39]<<7)) | (0x7F&(buf[40]>>1));
	cnts[asic*36+2]=(0x1C0&(buf[38]<<6)) | (0x3F&(buf[39]>>2));
	cnts[asic*36+3]=(0x1E0&(buf[37]<<5)) | (0x1F&(buf[38]>>3));
	cnts[asic*36+4]=(0x1F0&(buf[36]<<4)) | (0x0F&(buf[37]>>4));
	cnts[asic*36+5]=(0x1F8&(buf[35]<<3)) | (0x07&(buf[36]>>5));
	cnts[asic*36+6]=(0x1FC&(buf[34]<<2)) | (0x03&(buf[35]>>6));
	cnts[asic*36+7]=(0x1FE&(buf[33]<<1)) | (0x01&(buf[34]>>7));

	cnts[asic*36+8] =(0x100&(buf[31]<<8)) | (0xFF&(buf[32]>>0));
	cnts[asic*36+9] =(0x180&(buf[30]<<7)) | (0x7F&(buf[31]>>1));
	cnts[asic*36+10]=(0x1C0&(buf[29]<<6)) | (0x3F&(buf[30]>>2));
	cnts[asic*36+11]=(0x1E0&(buf[28]<<5)) | (0x1F&(buf[29]>>3));
	cnts[asic*36+12]=(0x1F0&(buf[27]<<4)) | (0x0F&(buf[28]>>4));
	cnts[asic*36+13]=(0x1F8&(buf[26]<<3)) | (0x07&(buf[27]>>5));
	cnts[asic*36+14]=(0x1FC&(buf[25]<<2)) | (0x03&(buf[26]>>6));
	cnts[asic*36+15]=(0x1FE&(buf[24]<<1)) | (0x01&(buf[25]>>7));

	cnts[asic*36+16]=(0x100&(buf[22]<<8)) | (0xFF&(buf[23]>>0));
	cnts[asic*36+17]=(0x180&(buf[21]<<7)) | (0x7F&(buf[22]>>1));
	cnts[asic*36+18]=(0x1C0&(buf[20]<<6)) | (0x3F&(buf[21]>>2));
	cnts[asic*36+19]=(0x1E0&(buf[19]<<5)) | (0x1F&(buf[20]>>3));
	cnts[asic*36+20]=(0x1F0&(buf[18]<<4)) | (0x0F&(buf[19]>>4));
	cnts[asic*36+21]=(0x1F8&(buf[17]<<3)) | (0x07&(buf[18]>>5));
	cnts[asic*36+22]=(0x1FC&(buf[16]<<2)) | (0x03&(buf[17]>>6));
	cnts[asic*36+23]=(0x1FE&(buf[15]<<1)) | (0x01&(buf[16]>>7));

	cnts[asic*36+24]=(0x100&(buf[13]<<8)) | (0xFF&(buf[14]>>0));
	cnts[asic*36+25]=(0x180&(buf[12]<<7)) | (0x7F&(buf[13]>>1));
	cnts[asic*36+26]=(0x1C0&(buf[11]<<6)) | (0x3F&(buf[12]>>2));
	cnts[asic*36+27]=(0x1E0&(buf[10]<<5)) | (0x1F&(buf[11]>>3));
	cnts[asic*36+28]=(0x1F0&(buf[9]<<4)) | (0x0F&(buf[10]>>4));
	cnts[asic*36+29]=(0x1F8&(buf[8]<<3)) | (0x07&(buf[9]>>5));
	cnts[asic*36+30]=(0x1FC&(buf[7]<<2)) | (0x03&(buf[8]>>6));
	cnts[asic*36+31]=(0x1FE&(buf[6]<<1)) | (0x01&(buf[7]>>7));

	cnts[asic*36+32]=(0x100&(buf[4]<<8)) | (0xFF&(buf[5]>>0));
	cnts[asic*36+33]=(0x180&(buf[3]<<7)) | (0x7F&(buf[4]>>1));
	cnts[asic*36+34]=(0x1C0&(buf[2]<<6)) | (0x3F&(buf[3]>>2));
	cnts[asic*36+35]=(0x1E0&(buf[1]<<5)) | (0x1F&(buf[2]>>3));
	printf("CEC payload for ASIC %u:\n",_asic);
	for(int i=0;i<42;i++)
		printf("%2.2x ",buf[i]);
	printf("\n");
	for(int i=0;i<36;i++)
		printf("%2.2x ",cnts[asic*36+i]);
	printf("\n");
	//cut off overflow
	for(int i=0;i<35;i++)
		if(cnts[asic*36+i]&0x100) cnts[asic*36+i]=0xff;


}


void klaus_cec_data::Clear(){
	Now();
	time[0]=time[1];
	//printf("klaus_cec_data::Clear(): T0=%lu\n",time[0]);
	for(int i=0;i<144/*constants::CH_NUM*/;i++)
		cnts[i]=0;
}
void klaus_cec_data::Now(){
	timeval t;
	gettimeofday(&t,0);
	time[1]=(t.tv_sec*1000+(t.tv_usec/1000)); //ms
}

void klaus_cec_data::Add(klaus_cec_data* other){
	time[1]=other->time[1]; //update end time, assuming other is newer
	for(int i=0;i<144/*constants::CH_NUM*/;i++)
		cnts[i]+=other->cnts[i];
}


float klaus_cec_data::Duration(){
	return (time[1]-time[0])*1e-3; //s
}
float klaus_cec_data::Rate(int channel){
	if(channel < 0 || channel > 144)
		return -1;
	if(Duration()==0)
		return -1;
	return cnts[channel]/Duration();
}

void klaus_cec_data::Print(int channel, FILE* fd) {
	float dt=Duration();
	if(dt==0) dt=-1;	
	//fprintf(fd,"TIME_USEC0\tTIME_USEC1\tDT_sec\tSLAVE\tCH\tCNTS\tRATE\n");
	fprintf(fd,"DT_sec\tSLAVE\tCH\tCNTS\tRATE\n");
	
	for (int i=0; i<144/*constants::CH_NUM*/;i++){
		if(channel>=0)
			i=channel;
		//fprintf(fd,"%5.5lu\t%5.5lu\t%2.5f\t%3.3u\t%2.2u\t%3.3u\t%3.3e Hz\n",
		fprintf(fd,"%2.5f\t%3.3u\t%2.2u\t%10.10llu\t%3.3e Hz\n",
			//time[0],
			//time[1],
			Duration(),
			asic,
			i,
			cnts[i],
			Rate(i)
		);
		if(channel>=0)
			break;
	}
}

void	klaus_cec_data::PrintHeaderTransposed(FILE* fd){
		for(int i=0;i<144/*constants::CH_NUM*/;i++)
			printf("S%u.CH%d\t",asic,i);
		printf("\n");
}

void	klaus_cec_data::PrintCountsTransposed(FILE* fd){
	for(int i=0;i<144;i++){
			printf(" %3.3u ",cnts[i]);
		}
	printf("\n");
}


void	klaus_cec_data::PrintRateTransposed(FILE* fd){
	for(int i=0;i<144;i++){
			printf("%2.2e ",Rate(i));
		}
	printf("\n");
}


