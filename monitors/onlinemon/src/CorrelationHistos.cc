/*
 * CorrelationHistos.cc
 *
 *  Created on: Jun 16, 2011
 *      Author: stanitz
 */

#include "CorrelationHistos.hh"

CorrelationHistos::CorrelationHistos(SimpleStandardPlane p1,
                                     SimpleStandardPlane p2)
    : _sensor1(p1.getName()), _sensor2(p2.getName()), _id1(p1.getID()),
      _id2(p2.getID()), _maxX1(p1.getMaxX()), _maxX2(p2.getMaxX()),
      _maxY1(p1.getMaxY()), _maxY2(p2.getMaxY()), _fills(0), _2dcorrX(NULL),
      _2dcorrY(NULL) {
  char out[1024], out2[1024], out_x[1024], out_y[1024];  
  if (_maxX1 != -1 && _maxX2 != -1) {
    sprintf(out, "X Correlation of %s %i and %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_corr_X_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out_x, "%s %i X", _sensor1.c_str(), _id1);
    sprintf(out_y, "%s %i X", _sensor2.c_str(), _id2);
    _2dcorrX = new TH2I(out2, out, _maxX1, 0, _maxX1, _maxX2, 0, _maxX2);
    if (_2dcorrX != NULL) {
      _2dcorrX->GetXaxis()->SetTitle(out_x);
      _2dcorrX->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _2dcorrX->GetXaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _2dcorrX->GetXaxis()->SetTitleOffset(1);
      _2dcorrX->GetYaxis()->SetTitle(out_y);
      _2dcorrX->GetYaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _2dcorrX->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _2dcorrX->GetXaxis()->SetTitleOffset(1);
    }
  }
  
  if (_maxY1 != -1 && _maxY2 != -1) {
    sprintf(out, "Y Correlation of %s %i and %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_corr_Y_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out_x, "%s %i Y", _sensor1.c_str(), _id1);
    sprintf(out_y, "%s %i Y", _sensor2.c_str(), _id2);
    _2dcorrY = new TH2I(out2, out, _maxY1, 0, _maxY1, _maxY2, 0, _maxY2);
    if (_2dcorrY != NULL) {
      _2dcorrY->GetXaxis()->SetTitle(out_x);
      _2dcorrY->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _2dcorrY->GetXaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _2dcorrY->GetXaxis()->SetTitleOffset(1);
      _2dcorrY->GetYaxis()->SetTitle(out_y);
      _2dcorrY->GetYaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _2dcorrY->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _2dcorrY->GetXaxis()->SetTitleOffset(1);
    }
  }
  //1D
  // if (_maxB1 != -1) {
    sprintf(out, "Diff X %s %i - %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_difX_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out_x, "X %s %i - X %s %i", _sensor1.c_str(), _id1, _sensor2.c_str(), _id2);
    _1dcorrX = new TH1I(out2, out, 201, -100.5, 100.5);
    if (_1dcorrX != NULL) {
      _1dcorrX->GetXaxis()->SetTitle(out_x);
      _1dcorrX->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _1dcorrX->GetXaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _1dcorrX->GetXaxis()->SetTitleOffset(1);
      _1dcorrX->GetYaxis()->SetTitle(out_y);
      _1dcorrX->GetYaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _1dcorrX->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _1dcorrX->GetXaxis()->SetTitleOffset(1);
    }
    //  }

    sprintf(out, "Diff Y %s %i - %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_difY_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out_x, "Y %s %i - Y %s %i", _sensor1.c_str(), _id1, _sensor2.c_str(), _id2);
    _1dcorrY = new TH1I(out2, out, 201, -100.5, 100.5);
    if (_1dcorrY != NULL) {
      _1dcorrY->GetXaxis()->SetTitle(out_x);
      _1dcorrY->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _1dcorrY->GetXaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _1dcorrY->GetXaxis()->SetTitleOffset(1);
      _1dcorrY->GetYaxis()->SetTitle(out_y);
      _1dcorrY->GetYaxis()->SetTitleSize(static_cast<Float_t>(0.03));
      _1dcorrY->GetXaxis()->SetLabelSize(static_cast<Float_t>(0.03));
      _1dcorrY->GetXaxis()->SetTitleOffset(1);
    }
    //  }
    
  m_pitchX1=1;
  m_pitchY1=1;
  m_pitchX2=1;
  m_pitchY2=1;

  if(_sensor1=="MIMOSA26"){
    m_pitchX1=18.4;
    m_pitchY1=18.4;
  }
  if(_sensor1=="USBPIXI4"){
    m_pitchX1=50;
    m_pitchY1=250;
  }
  if(!_sensor1.compare(0,3,"ABC")){
    m_pitchX1=74.5;
    m_pitchY1=74.5;
  }

  if(_sensor2=="MIMOSA26"){
    m_pitchX2=18.4;
    m_pitchY2=18.4;
  }
  if(_sensor2=="USBPIXI4"){
    m_pitchX2=50;
    m_pitchY2=250;
  }
  if(!_sensor2.compare(0,3,"ABC")){
    m_pitchX2=74.5;
    m_pitchY2=74.5;
  }

  if (_maxX1 != -1 && _maxX2 != -1) {
    sprintf(out, "X CorrelationVsTime of %s %i and %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_corrVsTime_X_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    _2dcorrTimeX = new TH2I(out2, out, 1000, 0, 1000, 200, 0, 10);
#ifdef EUDAQ_LIB_ROOT6
    _2dcorrTimeX->SetCanExtend(TH2I::kAllAxes);
#else
    _2dcorrTimeX->SetBit(TH2I::kCanRebin);
#endif
  }
  if (_maxY1 != -1 && _maxY2 != -1) {
    sprintf(out, "Y CorrelationVsTime of %s %i and %s %i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    sprintf(out2, "h_corrVsTime_Y_%s_%i_vs_%s_%i", _sensor1.c_str(), _id1,
            _sensor2.c_str(), _id2);
    _2dcorrTimeY = new TH2I(out2, out, 1000, 0, 1000, 200, 0, 10);
#ifdef EUDAQ_LIB_ROOT6
    _2dcorrTimeY->SetCanExtend(TH2I::kAllAxes);
#else
    _2dcorrTimeY->SetBit(TH2I::kCanRebin);
#endif
  }

}

void CorrelationHistos::Fill(const SimpleStandardCluster &cluster1,
                             const SimpleStandardCluster &cluster2) {
  std::lock_guard<std::mutex> lckx(m_mu);
  if (_2dcorrX != NULL){
    _2dcorrX->Fill(cluster1.getX(), cluster2.getX());
  }
  if (_2dcorrY != NULL){
    _2dcorrY->Fill(cluster1.getY(), cluster2.getY());
  }
   if (_1dcorrX != NULL){
    _1dcorrX->Fill(cluster1.getX()-cluster2.getX());
  }
   if (_1dcorrY != NULL){
     _1dcorrY->Fill(cluster1.getY()-cluster2.getY());
  }
}


void CorrelationHistos::FillCorrVsTime(const SimpleStandardCluster &cluster1,
				       const SimpleStandardCluster &cluster2,
				       const SimpleStandardEvent &simpev) {
  std::lock_guard<std::mutex> lckx(m_mu);
  
  if (_2dcorrTimeX != NULL){
    _2dcorrTimeX->Fill(simpev.getEvent_number(),  cluster1.getX()-cluster2.getX()*m_pitchX2/m_pitchX1);
  }
  if (_2dcorrTimeY != NULL){
    _2dcorrTimeY->Fill(simpev.getEvent_number(), cluster1.getY()-cluster2.getY()*m_pitchY2/m_pitchY1);
  }

}


void CorrelationHistos::Reset() {
  _2dcorrX->Reset();
  _2dcorrY->Reset();
  _2dcorrTimeX->Reset();
  _2dcorrTimeY->Reset();
  _1dcorrX->Reset();
  _1dcorrY->Reset();
}

TH1I *CorrelationHistos::getDiffXHisto() { return _1dcorrX; }
TH1I *CorrelationHistos::getDiffYHisto() { return _1dcorrY; }
TH2I *CorrelationHistos::getCorrXHisto() { return _2dcorrX; }
TH2I *CorrelationHistos::getCorrYHisto() { return _2dcorrY; }

void CorrelationHistos::Write() {
  _1dcorrX->Write();
  _1dcorrY->Write();
  _2dcorrX->Write();
  _2dcorrY->Write();
  _2dcorrTimeX->Write();
  _2dcorrTimeY->Write();
}


std::mutex* CorrelationHistos::getMutex(){return &m_mu;}
