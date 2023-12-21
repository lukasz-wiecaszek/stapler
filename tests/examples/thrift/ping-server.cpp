/* SPDX-License-Identifier: MIT */
/**
 * @file ping-server.cpp
 *
 * Implementation of the Thrift's 'ping' service (server side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <iostream>
#include <string>
#include <format>

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>

#if defined(USE_STAPLER_TRANSPORT)
#include "thrift/transport/ServerStaplerTransport.hpp"
#else
#include <thrift/transport/TServerSocket.h>
#endif

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "ping-common.hpp"
#include "ping.h"

/*===========================================================================*\
 * 'using namespace' section
\*===========================================================================*/

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
static int debug_level = 3; /* It is actually good place for this definition */

#define dbg_at1(args...) do { if (debug_level >= 1) {std::cerr << std::format(args);} } while (0)
#define dbg_at2(args...) do { if (debug_level >= 2) {std::cout << std::format(args);} } while (0)
#define dbg_at3(args...) do { if (debug_level >= 3) {std::cout << std::format(args);} } while (0)

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/
namespace
{

class PingHandler : public lts::tutorial::pingIf {
public:
  PingHandler() = default;

  void name(std::string& _return)
  {
    _return.assign(PING_INTERFACE_NAME);
  }

  void version(lts::tutorial::version_struct& _return)
  {
    _return.major = PING_INTERFACE_VERSION_MAJOR;
    _return.minor = PING_INTERFACE_VERSION_MINOR;
    _return.micro = PING_INTERFACE_VERSION_MICRO;
  }

  void ping(lts::tutorial::test_struct& _return, const lts::tutorial::test_struct& arg)
  {
    _return = arg;
    _return.op = lts::tutorial::operation::PONG;
  }

  void hello(const std::string& text)
  {
    static int cnt = 0;

    if ((cnt % 100) == 0)
      dbg_at2("{}\n", text);

    cnt++;
  }
};

class PingCloneFactory : virtual public lts::tutorial::pingIfFactory {
 public:
  ~PingCloneFactory() override = default;
  lts::tutorial::pingIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
  {
#if defined(USE_STAPLER_TRANSPORT)
    std::shared_ptr<lts::ServerStaplerTransport<NON_BLOCKING_TRANSPORT>> sock =
      std::dynamic_pointer_cast<lts::ServerStaplerTransport<NON_BLOCKING_TRANSPORT>>(connInfo.transport);
#else
    std::shared_ptr<apache::thrift::transport::TSocket> sock =
      std::dynamic_pointer_cast<apache::thrift::transport::TSocket>(connInfo.transport);
    dbg_at2("Incoming connection\n");
    dbg_at2("\tSocketInfo: {}\n",  sock->getSocketInfo());
    dbg_at2("\tPeerHost: {}\n",    sock->getPeerHost());
    dbg_at2("\tPeerAddress: {}\n", sock->getPeerAddress());
    dbg_at2("\tPeerPort: {}\n",    sock->getPeerPort());
#endif
    return new PingHandler;
  }
  void releaseHandler(lts::tutorial::exampleIf* handler) override {
    delete handler;
  }
};

} // end of anonymous namespace

/*===========================================================================*\
 * local (internal linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * global (external linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) functions definitions
\*===========================================================================*/

/*===========================================================================*\
 * class public functions definitions
\*===========================================================================*/

/*===========================================================================*\
 * class protected functions definitions
\*===========================================================================*/

/*===========================================================================*\
 * class private functions definitions
\*===========================================================================*/

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main() {
  dbg_at2("Starting 'ping' server ...\n");

  apache::thrift::server::TThreadedServer server(
    std::make_shared<lts::tutorial::pingProcessorFactory>(std::make_shared<PingCloneFactory>()),
#if defined(USE_STAPLER_TRANSPORT)
    std::make_shared<lts::ServerStaplerTransport<NON_BLOCKING_TRANSPORT>>(1),
#else
    std::make_shared<apache::thrift::transport::TServerSocket>(PING_CONNECTION_PORT), //port
#endif
    std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
    std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());

  /*
  // if you don't need per-connection state, do the following instead
  TThreadedServer server(
    std::make_shared<PingProcessor>(std::make_shared<PingHandler>()),
    std::make_shared<TServerSocket>(9090), //port
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>());
  */

  /**
   * Here are some alternate server types...

  // This server only allows one connection at a time, but spawns no threads
  TSimpleServer server(
    std::make_shared<PingProcessor>(std::make_shared<PingHandler>()),
    std::make_shared<TServerSocket>(9090),
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>());

  const int workerCount = 4;

  std::shared_ptr<ThreadManager> threadManager =
    ThreadManager::newSimpleThreadManager(workerCount);
  threadManager->threadFactory(
    std::make_shared<ThreadFactory>());
  threadManager->start();

  // This server allows "workerCount" connection at a time, and reuses threads
  TThreadPoolServer server(
    std::make_shared<PingProcessorFactory>(std::make_shared<PingCloneFactory>()),
    std::make_shared<TServerSocket>(9090),
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>(),
    threadManager);
  */

  dbg_at2("Server started\n");
  server.serve();

  return 0;
}
