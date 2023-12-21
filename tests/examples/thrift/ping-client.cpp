/* SPDX-License-Identifier: MIT */
/**
 * @file ping-client.cpp
 *
 * Implementation of the Thrift's 'ping' service (client side).
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <time.h>
#include <getopt.h>

#include <iostream>
#include <string>
#include <format>

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

#define NUM_OF_REPETITIONS 10000

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
static int ping_ping(lts::tutorial::pingClient& client)
{
    int32_t retval;
    std::string name;
    lts::tutorial::version_struct version;

    client.name(name);
    if (name != PING_INTERFACE_NAME) {
        dbg_at1("test failed (interface name) - expected: {}, actual: {}\n",
            PING_INTERFACE_NAME, name);
        return -1;
    }

    client.version(version);
    if (version.major != PING_INTERFACE_VERSION_MAJOR ||
        version.minor != PING_INTERFACE_VERSION_MINOR ||
        version.micro != PING_INTERFACE_VERSION_MICRO) {
        dbg_at1("test failed (version numbers) - expected: {}.{}.{}, actual: {}.{}.{}\n",
            PING_INTERFACE_VERSION_MAJOR, PING_INTERFACE_VERSION_MINOR, PING_INTERFACE_VERSION_MICRO,
            version.major, version.minor, version.micro);
        return -1;
    }

    lts::tutorial::test_struct receive;
    lts::tutorial::test_struct send;

    send.op = lts::tutorial::operation::PING;

    send.bdt.v1 = 1;
    send.bdt.v2 = 2;
    send.bdt.v3 = 3;
    send.bdt.v4 = 4;
    send.bdt.v5 = 5;
    send.bdt.v6 = 6;
    send.bdt.v7 = 7;
    send.bdt.v8 = 8;

    send.cdt.v1 = {{1, "one"}, {2, "two"}, {3, "three"}};
    send.cdt.v2 = {1, 2, 3};
    send.cdt.v3 = {1, 2, 3};

    client.ping(receive, send);
    if (receive.op != lts::tutorial::operation::PONG) {
        dbg_at1("test failed - expected PONG operation\n");
        return -1;
    }
    if (receive.bdt != send.bdt) {
        dbg_at1("test failed - send/receive basic data types mismatch\n");
        return -1;
    }
    if (receive.cdt != send.cdt) {
        dbg_at1("test failed - send/receive container data types mismatch\n");
        return -1;
    }

    return 0;
}

static int ping_hello(lts::tutorial::pingClient& client)
{
    int32_t retval;
    std::string name;
    lts::tutorial::version_struct version;

    client.name(name);
    if (name != PING_INTERFACE_NAME) {
        dbg_at1("test failed (interface name) - expected: {}, actual: {}\n",
            PING_INTERFACE_NAME, name);
        return -1;
    }

    client.version(version);
    if (version.major != PING_INTERFACE_VERSION_MAJOR ||
        version.minor != PING_INTERFACE_VERSION_MINOR ||
        version.micro != PING_INTERFACE_VERSION_MICRO) {
        dbg_at1("test failed (version numbers) - expected: {}.{}.{}, actual: {}.{}.{}\n",
            PING_INTERFACE_VERSION_MAJOR, PING_INTERFACE_VERSION_MINOR, PING_INTERFACE_VERSION_MICRO,
            version.major, version.minor, version.micro);
        return -1;
    }

    char strbuffer[16];
    static int cnt = 0;
    snprintf(strbuffer, sizeof(strbuffer), "hello #%d", cnt);
    client.hello(strbuffer);
    cnt++;

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
    bool ping = false;
    bool hello = false;
    struct timespec t1, t2;
    uint64_t microseconds;
    int n = 0;

    static struct option long_options[] = {
        {"pid",   required_argument, 0, 'p'},
        {"tid",   required_argument, 0, 't'},
        {"ping",  no_argument,       0, 'g'},
        {"hello", no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    for (;;) {
        int c = getopt_long(argc, argv, "p:t:gh", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'p':
                pid = atoi(optarg);
                break;
            case 't':
                tid = atoi(optarg);
                break;
            case 'g':
                ping = true;
                break;
            case 'h':
                hello = true;
                break;
        }
    }

    if (ping == false && hello == false) {
        dbg_at1("Neither 'ping' nor 'hello' test is selected. Terminating.\n");
        exit(EXIT_FAILURE);
    }

#if defined(USE_STAPLER_TRANSPORT)
    std::shared_ptr<lts::ClientStaplerTransport<NON_BLOCKING_TRANSPORT>> socket(new lts::ClientStaplerTransport<NON_BLOCKING_TRANSPORT>(pid, tid));
#else
    std::shared_ptr<apache::thrift::transport::TTransport> socket(new apache::thrift::transport::TSocket("localhost", PING_CONNECTION_PORT));
#endif
    std::shared_ptr<apache::thrift::transport::TTransport> transport(new apache::thrift::transport::TBufferedTransport(socket));
    std::shared_ptr<apache::thrift::protocol::TProtocol> protocol(new apache::thrift::protocol::TBinaryProtocol(transport));
    lts::tutorial::pingClient client(protocol);

    clock_gettime(CLOCK_MONOTONIC, &t1);

    try {
        transport->open();

        for (; n < NUM_OF_REPETITIONS; n++) {
            if (ping)
                if (ping_ping(client) != 0)
                    break;

            if (hello)
                if (ping_hello(client) != 0)
                    break;
        }

        transport->close();
    } catch (apache::thrift::TException& tx) {
        dbg_at1("An exception was caught: {}\n", tx.what());
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    microseconds = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    dbg_at2("{} out of {} messages sent with success\n", n, NUM_OF_REPETITIONS);
    dbg_at2("Test took {} microseconds\n", microseconds);
}
