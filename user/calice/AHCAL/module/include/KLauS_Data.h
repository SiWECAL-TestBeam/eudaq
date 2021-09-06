#ifndef KLAUS_DATA_H__
#define KLAUS_DATA_H__

#include <fstream>
#include <iostream>
#include <map>
#include <list>

#include <deque>
#include <sys/time.h>

////////////////////////////////////////////////////////////////
///                 KLauS HIT TYPES
///////////////////////////////////////////////////////////////

//Single event type for KLauS5 ASIC / Overloaded functions for KLauS6
class KLauS_Hit
{
	protected:
		unsigned short	ADC_10b;

		unsigned short	ADC_6b;
		unsigned short	ADC_PIPE;

		unsigned char channel;
		unsigned short chipID;

		bool		gainsel_evt;
		bool		gainsel_busy;
		unsigned short  badhit;
		unsigned short	time;

		//DAQ fields
		unsigned short	m_ROC;
	public:
		KLauS_Hit(){}
		KLauS_Hit(std::deque<unsigned char>::iterator buffer, unsigned short ROC, unsigned char asic){m_ROC=ROC; Parse(buffer,asic);}
                // Parse the data stream received through the I2C interface
		void		Parse(std::deque<unsigned char>::iterator event, unsigned char asic=0);

		//Charge information
		unsigned short GetADC_10b() const{return ADC_10b;};
		unsigned short GetADC_12b() const{return ADC_6b<<8+ADC_PIPE;};
		bool GetGainBit() const {return gainsel_evt;};

		//Channel information
		unsigned short GetChannelUnique() const{if(channel!=0xff) return channel+chipID*36; else return 10000+chipID;};
		unsigned short GetASICChannel() const{return channel;};
		unsigned short GetASICNumber(){return chipID;};

		//Time information. Time is always given in 200ps bins.
		// transfer the time from raw data (Gray to decimal / CC+MC+FC) and get the time interval wrt the last event 
		virtual unsigned long GetTime() const;
		int 		DiffTime(KLauS_Hit& evt) const;
		unsigned short GetROC(){return m_ROC;};
		// Print 
		static void	PrintHeader(FILE* fd=stdout);
		void		Print(FILE* fd=stdout);
};

////////////////////////////////////////////////////////////////
///                 KLauS CEC TYPES
///////////////////////////////////////////////////////////////
class klaus_cec_data
{
	public:
		klaus_cec_data(){}
		void Fill(unsigned char* buf,unsigned char _asic);
		unsigned long time[2]; //start and end of counting
		//accumulated count values
		unsigned long cnts[144];
		unsigned short asic;
		void		Clear();
		void		Now();
		void		Add(klaus_cec_data* other);
		float		Duration();
		float		Rate(int channel);
		void		Print(int channel=-1, FILE* fd=stdout);
		void		PrintHeaderTransposed(FILE* fd=stdout);
		void		PrintRateTransposed(FILE* fd=stdout);
		void		PrintCountsTransposed(FILE* fd=stdout);
};

#endif
