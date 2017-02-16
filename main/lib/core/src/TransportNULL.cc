#include "eudaq/TransportNULL.hh"
#include "eudaq/Utils.hh"

#include <iostream>

namespace eudaq {

  const std::string NULLServer::name = "null";

  NULLServer::NULLServer(const std::string &) {}

  NULLServer::~NULLServer() {}

  void NULLServer::Close(const ConnectionInfo &) {}

  void NULLServer::SendPacket(const unsigned char *, size_t,
                              const ConnectionInfo &, bool) {}

  void NULLServer::ProcessEvents(int timeout) { mSleep(timeout); }

  std::vector<ConnectionSPC> NULLServer::GetConnections() const{
    return std::vector<ConnectionSPC>();
  }
  
  std::string NULLServer::ConnectionString() const { return "null://"; }

  NULLClient::NULLClient(const std::string & /*param*/):m_buf("") {}

  void NULLClient::SendPacket(const unsigned char *, size_t,
                              const ConnectionInfo &, bool) {}

  void NULLClient::ProcessEvents(int timeout) {
    // std::cout << "NULLClient::ProcessEvents " << timeout << std::endl;
    mSleep(timeout);
    // std::cout << "ok" << std::endl;
  }

  NULLClient::~NULLClient() {}
}
