/* SPDX-License-Identifier: MIT */
/**
 * @file timer-server.cpp
 *
 * Implementation of the Thrift's 'timer' service (server side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <iostream>
#include <string>
#include <format>
#include <chrono>

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
#include "timer-common.hpp"
#include "timer.h"
#include "timer_constants.h"

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

class TimerHandler : public lts::tutorial::timerIf {
public:
  TimerHandler()
  : counter{0}
  , interval{0}
  , start_time{0}
  {
  }

  void name(std::string& _return)
  {
    _return.assign(TIMER_INTERFACE_NAME);
  }

  void version(lts::tutorial::version_struct& _return)
  {
    _return.major = TIMER_INTERFACE_VERSION_MAJOR;
    _return.minor = TIMER_INTERFACE_VERSION_MINOR;
    _return.micro = TIMER_INTERFACE_VERSION_MICRO;
  }

  lts::tutorial::timer_id start(const int64_t interval_us)
  {
    if (interval_us <= 0)
      return lts::tutorial::g_timer_constants.INVALID_TIMER_ID;

    interval = interval_us;
    start_time = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();

    return 0;
  }

  void stop(const lts::tutorial::timer_id id)
  {
    if (id == 0) {
      counter = 0;
      interval = 0;
      start_time = 0;
    }
  }

  void tick(lts::tutorial::timestamp& _return, const lts::tutorial::timer_id id)
  {
    if (id != 0) {
        _return.counter = 0;
        _return.abstime = 0;
      return;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();

    if (now < start_time) {
        _return.counter = 0;
        _return.abstime = 0;
        return;
    }

    uint64_t diff = now - start_time;
    if (diff < interval)
      std::this_thread::sleep_for(std::chrono::microseconds(interval - diff));

    start_time += interval;
    counter++;

    _return.counter = counter;
    _return.abstime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }

private:
  uint64_t counter;
  uint64_t interval;
  uint64_t start_time;
};

class TimerCloneFactory : virtual public lts::tutorial::timerIfFactory {
 public:
  ~TimerCloneFactory() override = default;
  lts::tutorial::timerIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
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
    return new TimerHandler;
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
  dbg_at2("Starting 'timer' server ...\n");

  apache::thrift::server::TThreadedServer server(
    std::make_shared<lts::tutorial::timerProcessorFactory>(std::make_shared<TimerCloneFactory>()),
#if defined(USE_STAPLER_TRANSPORT)
    std::make_shared<lts::ServerStaplerTransport<NON_BLOCKING_TRANSPORT>>(1),
#else
    std::make_shared<apache::thrift::transport::TServerSocket>(TIMER_CONNECTION_PORT), //port
#endif
    std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
    std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());

  /*
  // if you don't need per-connection state, do the following instead
  TThreadedServer server(
    std::make_shared<TimerProcessor>(std::make_shared<TimerHandler>()),
    std::make_shared<TServerSocket>(9090), //port
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>());
  */

  /**
   * Here are some alternate server types...

  // This server only allows one connection at a time, but spawns no threads
  TSimpleServer server(
    std::make_shared<TimerProcessor>(std::make_shared<TimerHandler>()),
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
    std::make_shared<TimerProcessorFactory>(std::make_shared<TimerCloneFactory>()),
    std::make_shared<TServerSocket>(9090),
    std::make_shared<TBufferedTransportFactory>(),
    std::make_shared<TBinaryProtocolFactory>(),
    threadManager);
  */

  dbg_at2("Server started\n");
  server.serve();

  return 0;
}
