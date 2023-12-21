/* SPDX-License-Identifier: MIT */
/**
 * @file ServerStaplerTransport.hpp
 *
 * Apache Thrift server transport using stapler kernel module
 * as an ipc communication framework.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _SERVER_STAPLER_TRANSPORT_HPP_
#define _SERVER_STAPLER_TRANSPORT_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <iostream>
#include <string>
#include <memory>
#include <format>
#include <future>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>

#include <thrift/transport/TServerTransport.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "../../../../../stplr.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define STPLR_DEVICENAME "/dev/stplr-0"

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace lts
{

template<bool NON_BLOCKING = false>
class ServerStaplerTransport : public apache::thrift::transport::TServerTransport
{
public:
    ServerStaplerTransport(int32_t max_num_of_threads)
    : m_max_num_of_threads{max_num_of_threads}
    , m_num_of_threads{0}
    {
    }

    bool isOpen() const override
    {
        return true;
    }

    void close() override
    {
    }

    std::shared_ptr<apache::thrift::transport::TTransport> acceptImpl() override
    {
        if (m_num_of_threads++ < m_max_num_of_threads)
            return std::make_shared<Transport>();
        else
            std::promise<void>().get_future().wait();

        return nullptr; /* this is needed to supress 'control reaches end of non-void function' */
    }

private:
    class Transport : public apache::thrift::transport::TTransport
    {
    public:
        Transport()
        : m_fd{-1}
        , m_pid{-1}
        , m_tid{-1}
        , m_handle{}
        {
        }

        ~Transport() override
        {
            if (isOpen())
                closeDevice();
        }

        bool isOpen() const override
        {
            return m_fd >= 0;
        }

        void open() override
        {
            if (!isOpen())
                openDevice();
        }

        void close() override
        {
            if (isOpen())
                closeDevice();
        }

        uint32_t read_virt(uint8_t* buf, uint32_t len) override
        {
            int status;

            if (!isOpen())
                openDevice();

            struct stplr_msg msgs[] = {
                {.msgbuf = buf, .buflen = len},
            };

            struct stplr_msg_receive msg_receive;
            msg_receive.handle = m_handle;
            msg_receive.rmsgs.msgs = msgs;
            msg_receive.rmsgs.count = sizeof(msgs)/sizeof(msgs[0]);

            status = ::ioctl(m_fd, STPLR_MSG_RECEIVE, &msg_receive);
            assert(status >= -1);
            if (status == -1) {
                std::string msg = std::format("ioctl(STPLR_MSG_RECEIVE) failed with code {} : {}",
                    errno, strerror(errno));
                std::cout << msg << std::endl;
                throw apache::thrift::transport::TTransportException(
                    apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
            }

            m_pid = msg_receive.pid;
            m_tid = msg_receive.tid;

            return msgs[0].buflen;
        }

        void write_virt(const uint8_t* buf, uint32_t len) override
        {
            int status;

            if (!isOpen())
                openDevice();

            struct stplr_msg msgs[] = {
                {.msgbuf = (void*)buf, .buflen = len},
            };

            if constexpr (NON_BLOCKING == true) {
                struct stplr_msg_send msg_send;
                msg_send.handle = m_handle;
                msg_send.pid = m_pid;
                msg_send.tid = m_tid;
                msg_send.smsgs.msgs = msgs;
                msg_send.smsgs.count = std::size(msgs);

                status = ::ioctl(m_fd, STPLR_MSG_SEND, &msg_send);
                assert(status >= -1);
                if (status == -1) {
                    std::string msg = std::format("ioctl(STPLR_MSG_SEND) failed with code {} : {}",
                        errno, strerror(errno));
                    std::cout << msg << std::endl;
                    throw apache::thrift::transport::TTransportException(
                        apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
                }
            } else {
                struct stplr_msg_reply msg_reply = {};
                msg_reply.handle = m_handle;
                msg_reply.pid = m_pid;
                msg_reply.tid = m_tid;
                msg_reply.rmsgs.msgs = msgs;
                msg_reply.rmsgs.count = std::size(msgs);

                status = ::ioctl(m_fd, STPLR_MSG_REPLY, &msg_reply);
                assert(status >= -1);
                if (status == -1) {
                    std::string msg = std::format("ioctl(STPLR_MSG_REPLY) failed with code {} : {}",
                        errno, strerror(errno));
                    std::cout << msg << std::endl;
                    throw apache::thrift::transport::TTransportException(
                        apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
                }
            }
        }

    private:
        void openDevice()
        {
            int fd;
            int status;
            struct stplr_version version;
            struct stplr_handle handle;

            fd = ::open(STPLR_DEVICENAME, O_RDWR);
            assert(fd >= -1);
            if (fd == -1) {
                std::string msg = std::format("Cannot open '{}' : {}",
                    STPLR_DEVICENAME, strerror(errno));
                std::cout << msg << std::endl;
                throw apache::thrift::transport::TTransportException(
                    apache::thrift::transport::TTransportException::NOT_OPEN, msg);
            }

            status = ::ioctl(fd, STPLR_VERSION, &version);
            assert(status >= -1);
            if (status == -1) {
                std::string msg = std::format("ioctl(STPLR_VERSION) failed with code {} : {}",
                    errno, strerror(errno));
                std::cout << msg << std::endl;
                throw apache::thrift::transport::TTransportException(
                    apache::thrift::transport::TTransportException::NOT_OPEN, msg);
            }

            if (version.major != STPLR_VERSION_MAJOR) {
                std::string msg = std::format("Incompatible kernel module/header major version ({}/{})",
                    version.major, STPLR_VERSION_MAJOR);
                std::cout << msg << std::endl;
                throw apache::thrift::transport::TTransportException(
                    apache::thrift::transport::TTransportException::NOT_OPEN, msg);
            }

            status = ::ioctl(fd, STPLR_HANDLE_GET, &handle);
            assert(status >= -1);
            if (status == -1) {
                std::string msg = std::format("ioctl(STPLR_HANDLE_GET) failed with code {} : {}",
                    errno, strerror(errno));
                std::cout << msg << std::endl;
                throw apache::thrift::transport::TTransportException(
                    apache::thrift::transport::TTransportException::NOT_OPEN, msg);
            }

            std::cout << "Server listening at pid: " << getpid() << ", tid: " << gettid() << std::endl;

            m_fd = fd;
            m_handle = handle;
        }

        void closeDevice()
        {
            int status;

            status = ::ioctl(m_fd, STPLR_HANDLE_PUT, &m_handle);
            assert(status >= -1);
            if (status == -1) {
                std::string msg = std::format("ioctl(STPLR_HANDLE_PUT) failed with code {} : {}",
                    errno, strerror(errno));
                std::cout << msg << std::endl;
            }

            ::close(m_fd);

            m_fd = -1;
            memset(&m_handle, 0, sizeof(m_handle));
        }

        int m_fd;
        int m_pid;
        int m_tid;
        struct stplr_handle m_handle;
    };

    const int32_t m_max_num_of_threads;
    int32_t m_num_of_threads;
};

} /* end of namespace lts */

/*===========================================================================*\
 * inline function/variable definitions
\*===========================================================================*/
namespace lts
{
} /* end of namespace lts */

/*===========================================================================*\
 * global object declarations
\*===========================================================================*/
namespace lts
{
} /* end of namespace lts */

/*===========================================================================*\
 * function forward declarations
\*===========================================================================*/
namespace lts
{
} /* end of namespace lts */

#endif /* _SERVER_STAPLER_TRANSPORT_HPP_ */
