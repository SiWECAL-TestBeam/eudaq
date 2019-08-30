#include "MonitorWindow.hh"

#include "TApplication.h"
#include "TTimer.h"
#include "TGButton.h"
#include "TTree.h"
#include "TGFileDialog.h"
#include "TCanvas.h"

#include <iostream>
#include <fstream>
#include <sstream>

MonitorWindow::MonitorWindow(TApplication* par, const std::string& name)
  :TGMainFrame(gClient->GetRoot(), 800, 600, kVerticalFrame), m_parent(par),
   m_icon_db(gClient->GetPicture("rootdb_t.xpm")),
   m_icon_save(gClient->GetPicture("bld_save.xpm")),
   m_icon_del(gClient->GetPicture("bld_delete.xpm")),
   m_icon_th1(gClient->GetPicture("h1_t.xpm")),
   m_icon_th2(gClient->GetPicture("h2_t.xpm")),
   m_icon_tgraph(gClient->GetPicture("graph.xpm")),
   m_timer(new TTimer){
  SetWindowName(name.c_str());

  m_top_win = new TGHorizontalFrame(this);
  m_left_bar = new TGVerticalFrame(m_top_win);
  m_left_canv = new TGCanvas(m_left_bar, 200, 600);

  auto vp = m_left_canv->GetViewPort();

  m_tree_list = new TGListTree(m_left_canv, kHorizontalFrame);
  m_tree_list->Connect("DoubleClicked(TGListTreeItem*, Int_t)", NAME, this,
                       "DrawElement(TGListTreeItem*, Int_t)");
  m_tree_list->Connect("Clicked(TGListTreeItem*, Int_t, Int_t, Int_t)",
                       NAME, this,
                       "DrawMenu(TGListTreeItem*, Int_t, Int_t, Int_t)");
  vp->AddFrame(m_tree_list, new TGLayoutHints(kLHintsExpandY | kLHintsExpandY, 5, 5, 5, 5));
  m_top_win->AddFrame(m_left_bar, new TGLayoutHints(kLHintsExpandY, 2, 2, 2, 2));
  m_left_bar->AddFrame(m_left_canv, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));

  auto right_frame = new TGVerticalFrame(m_top_win);

  // toolbar
  m_toolbar = new TGToolBar(right_frame, 180, 80);
  right_frame->AddFrame(m_toolbar, new TGLayoutHints(kLHintsExpandX, 2, 2, 2, 2));
  m_button_save = new TGPictureButton(m_toolbar, m_icon_save);
  m_button_save->SetEnabled(false);
  m_button_save->Connect("Clicked()", NAME, this, "SaveTree()");
  m_toolbar->AddFrame(m_button_save, new TGLayoutHints(kLHintsLeft, 2, 1, 0, 0));

  m_button_clean = new TGPictureButton(m_toolbar, m_icon_del);
  m_button_clean->SetEnabled(false);
  m_button_clean->Connect("Clicked()", NAME, this, "CleanTree()");
  m_toolbar->AddFrame(m_button_clean, new TGLayoutHints(kLHintsLeft, 1, 2, 0, 0));

  auto update_toggle = new TGCheckButton(m_toolbar, "&Update", 1);
  m_toolbar->AddFrame(update_toggle, new TGLayoutHints(kLHintsLeft, 2, 2, 2, 2));
  update_toggle->Connect("Toggled(Bool_t)", NAME, this, "SwitchUpdate(Bool_t)");

  // main canvas
  m_main_canvas = new TRootEmbeddedCanvas("Canvas", right_frame);
  right_frame->AddFrame(m_main_canvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));
  m_top_win->AddFrame(right_frame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 2, 2, 2, 2));
  this->AddFrame(m_top_win, new TGLayoutHints(kLHintsExpandY | kLHintsExpandX | kLHintsLeft, 0, 0, 0, 0));

  // status bar
  int status_parts[(int)StatusBarPos::num_parts] = {20, 10, 35, 35};
  m_status_bar = new TGStatusBar(this, 510, 10, kHorizontalFrame);
  m_status_bar->SetParts(status_parts, (int)StatusBarPos::num_parts);
  ResetCounters();

  this->AddFrame(m_status_bar, new TGLayoutHints(kLHintsBottom | kLHintsExpandX, 0, 0, 2, 0));
  //left_bar->MapSubwindows();

  m_timer->Connect("Timeout()", NAME, this, "Update()");
  update_toggle->SetOn();
  SwitchUpdate(true);

  this->MapSubwindows();
  this->Layout();
  this->MapSubwindows();
  this->MapWindow();
  m_context_menu = new TContextMenu("", "");
}

MonitorWindow::~MonitorWindow(){
  gClient->FreePicture(m_icon_db);
  gClient->FreePicture(m_icon_save);
  gClient->FreePicture(m_icon_del);
  gClient->FreePicture(m_icon_th1);
  gClient->FreePicture(m_icon_th2);
  gClient->FreePicture(m_icon_tgraph);
  m_parent->Terminate(1);
}

void MonitorWindow::ResetCounters(){
  if (m_status_bar)
    return;
  m_status_bar->SetText("Run: N/A", (int)StatusBarPos::run_number);
  m_status_bar->SetText("Curr. event: N/A", (int)StatusBarPos::tot_events);
  m_status_bar->SetText("Analysed events: N/A", (int)StatusBarPos::an_events);
}

void MonitorWindow::SetRunNumber(int run){
  m_status_bar->SetText(Form("Run: %u", run), 1);
}

void MonitorWindow::SetStatus(Status st){
  m_status = st;
  std::ostringstream st_txt; st_txt << st;
  m_status_bar->SetText(st_txt.str().c_str(), (int)StatusBarPos::status);
}

void MonitorWindow::SetTree(TTree* tree){
  m_tree = tree;
  m_button_save->SetEnabled(true);
  m_button_clean->SetEnabled(true);
}

void MonitorWindow::SaveTree(){
  if (!m_tree || !m_button_save)
    return;
  static TString dir(".");
  TGFileInfo fi;
  const char *filetypes[] = { //"All files",     "*",
                            "ROOT files",    "*.root",
                            //"ROOT macros",   "*.C",
                            //"Text files",    "*.[tT][xX][tT]",
                            0,               0 };
  fi.fFileTypes = filetypes;
  fi.fIniDir = StrDup(dir);
  new TGFileDialog(gClient->GetRoot(), this, kFDSave, &fi);
  dir = fi.fIniDir;
  m_tree->SaveAs(fi.fFilename);
}

void MonitorWindow::SwitchUpdate(bool up){
  if (!up)
    m_timer->Stop();
  else if (up)
    m_timer->Start(1000, kFALSE); // update automatically every second
}

void MonitorWindow::Update(){
  if (m_tree && m_status == Status::running) {
    int ev_num = 0;
    m_tree->SetBranchAddress("event_n", &ev_num);
    m_status_bar->SetText(Form("Analysed events: %d", ev_num), (int)StatusBarPos::an_events);
    m_status_bar->SetText(Form("Curr. event: %d", m_tree->GetEntriesFast()), (int)StatusBarPos::tot_events);
  }
  std::cout << "Update..." << std::endl;
  if (!m_drawable.empty()) {
    TCanvas* canv = m_main_canvas->GetCanvas();
    canv->cd();
    canv->Clear();
    canv->Divide(m_drawable.size(), 1); //FIXME
    for (size_t i = 0; i < m_drawable.size(); ++i) {
      canv->cd(i+1);
      m_drawable.at(i)->Draw();
    }
    canv->Update();
  }
}

TObject* MonitorWindow::Get(const char* name){
  auto it = m_objects.find(name);
  if (it == m_objects.end())
    throw std::runtime_error("Failed to retrieve object with path \""+std::string(name)+"\"!");
  return it->second.second;
}

void MonitorWindow::DrawElement(TGListTreeItem* it, int){
  m_drawable.clear();
  for (auto& obj : m_objects)
    if (obj.second.first == it)
      m_drawable.emplace_back(obj.second.second);
  if (m_drawable.empty())
    throw std::runtime_error("Failed to retrieve the tree item!");
  Update();
}

void MonitorWindow::DrawMenu(TGListTreeItem* it, int but, int x, int y){
  if (but == 3)
    m_context_menu->Popup(x, y, this);
}

std::ostream& operator<<(std::ostream& os, const MonitorWindow::Status& stat){
  switch (stat) {
    case MonitorWindow::Status::idle: return os << "IDLE";
    case MonitorWindow::Status::configured: return os << "CONFIGURED";
    case MonitorWindow::Status::running: return os << "RUNNING";
    case MonitorWindow::Status::error: return os << "ERROR";
  }
  return os << "UNKNOWN";
}
