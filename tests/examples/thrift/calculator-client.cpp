/* SPDX-License-Identifier: MIT */
/**
 * @file calculator-client.cpp
 *
 * Implementation of the Thrift's 'calculator' service (client side).
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
#include "calculator-common.hpp"
#include "calculator.h"

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
static int send_messages(lts::tutorial::calculatorClient& client)
{
    int32_t arg1 = 100;
    int32_t arg2 = 3;
    int32_t retval;
    std::string name;
    lts::tutorial::version_struct version;

    client.name(name);
    if (name != CALCULATOR_INTERFACE_NAME) {
        dbg_at1("test failed - name: %s\n", name.c_str());
        return -1;
    }

    client.version(version);
    if (version.major != CALCULATOR_INTERFACE_VERSION_MAJOR ||
        version.minor != CALCULATOR_INTERFACE_VERSION_MINOR ||
        version.micro != CALCULATOR_INTERFACE_VERSION_MICRO) {
        dbg_at1("test failed - version: %d.%d.%d\n", version.major, version.minor, version.micro);
        return -1;
    }

    retval = client.add(arg1, arg2);
    if ((100 + 3) != retval) {
        dbg_at1("test failed - expected: 100 + 3, received: %d\n", retval);
        return -1;
    }

    retval = client.subtract(arg1, arg2);
    if ((100 - 3) != retval) {
        dbg_at1("test failed - expected: 100 - 3, received: %d\n", retval);
        return -1;
    }

    retval = client.multiply(arg1, arg2);
    if ((100 * 3) != retval) {
        dbg_at1("test failed - expected: 100 * 3, received: %d\n", retval);
        return -1;
    }

    retval = client.divide(arg1, arg2);
    if ((100 / 3) != retval) {
        dbg_at1("test failed - expected: 100 / 3, received: %d\n", retval);
        return -1;
    }

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
    struct timespec t1, t2;
    uint64_t microseconds;
    int n = 0;

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
    std::shared_ptr<apache::thrift::transport::TTransport> socket(new apache::thrift::transport::TSocket("localhost", CALCULATOR_CONNECTION_PORT));
#endif
    std::shared_ptr<apache::thrift::transport::TTransport> transport(new apache::thrift::transport::TBufferedTransport(socket));
    std::shared_ptr<apache::thrift::protocol::TProtocol> protocol(new apache::thrift::protocol::TBinaryProtocol(transport));
    lts::tutorial::calculatorClient client(protocol);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    try {
        transport->open();
        for (; n < NUM_OF_REPETITIONS; n++)
            if (send_messages(client) != 0)
                break;

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
