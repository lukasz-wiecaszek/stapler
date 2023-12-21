/* SPDX-License-Identifier: MIT */
/**
 * @file timer-client.cpp
 *
 * Implementation of the Thrift's 'timer' service (client side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <time.h>
#include <getopt.h>

#include <iostream>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>

#if defined(USE_STAPLER_TRANSPORT)
#include "thrift/transport/ClientStaplerTransport.hpp"
#else
#include <thrift/transport/TSocket.h>
#endif

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "timer-common.hpp"
#include "timer.h"

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

#define NUM_OF_REPETITIONS 20

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/
namespace
{
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
static int send_message(lts::tutorial::timerClient& client)
{
    std::string name;
    lts::tutorial::version_struct version;
    lts::tutorial::timestamp timestamp;

    client.name(name);
    if (name != TIMER_INTERFACE_NAME) {
        dbg_at1("test failed - name: %s\n", name.c_str());
        return -1;
    }

    client.version(version);
    if (version.major != TIMER_INTERFACE_VERSION_MAJOR ||
        version.minor != TIMER_INTERFACE_VERSION_MINOR ||
        version.micro != TIMER_INTERFACE_VERSION_MICRO) {
        dbg_at1("test failed - version: %d.%d.%d\n", version.major, version.minor, version.micro);
        return -1;
    }

    lts::tutorial::timer_id timer_id = client.start(100000 /* 100 ms */);

    for (int i = 0; i < NUM_OF_REPETITIONS; i++) {
        client.tick(timestamp, timer_id);

        uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
        dbg_at2("counter: {}, client: {}, server: {}, diff: {}\n",
            timestamp.counter, now, timestamp.abstime, now - timestamp.abstime);
    }

    client.stop(timer_id);

    return 0;
}

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
int main(int argc, char *argv[]) {
    int pid = -1;
    int tid = -1;

    static struct option long_options[] = {
        {"pid", required_argument, 0, 'p'},
        {"tid", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    for (;;) {
        int c = getopt_long(argc, argv, "p:t:", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 't':
                tid = atoi(optarg);
                break;
        }
    }

#if defined(USE_STAPLER_TRANSPORT)
    std::shared_ptr<lts::ClientStaplerTransport<NON_BLOCKING_TRANSPORT>> socket(new lts::ClientStaplerTransport<NON_BLOCKING_TRANSPORT>(pid, tid));
#else
    std::shared_ptr<apache::thrift::transport::TTransport> socket(new apache::thrift::transport::TSocket("localhost", TIMER_CONNECTION_PORT));
#endif
    std::shared_ptr<apache::thrift::transport::TTransport> transport(new apache::thrift::transport::TBufferedTransport(socket));
    std::shared_ptr<apache::thrift::protocol::TProtocol> protocol(new apache::thrift::protocol::TBinaryProtocol(transport));
    lts::tutorial::timerClient client(protocol);

    struct timespec t1, t2;
    uint64_t microseconds;
    int n;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    try {
        transport->open();

        send_message(client);

        transport->close();
    } catch (apache::thrift::TException& tx) {
        dbg_at1("An exception was caught: {}\n", tx.what());
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    dbg_at2("Test took {} microseconds\n", microseconds);
}
