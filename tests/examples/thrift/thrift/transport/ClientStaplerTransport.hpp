/* SPDX-License-Identifier: MIT */
/**
 * @file ClientStaplerTransport.hpp
 *
 * Apache Thrift client transport using stapler kernel module
 * as an ipc communication framework.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#ifndef _CLIENT_STAPLER_TRANSPORT_HPP_
#define _CLIENT_STAPLER_TRANSPORT_HPP_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <iostream>
#include <string>
#include <memory>
#include <format>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>

#include <thrift/transport/TTransport.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "../../../../../stplr.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define STPLR_DEVICENAME  "/dev/stplr-0"
#define RECEIVE_BUFFER_SIZE (64 * 1024)

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace lts
{

template<bool NON_BLOCKING = false>
class ClientStaplerTransport : public apache::thrift::transport::TTransport
{
public:
    ClientStaplerTransport(int pid, int tid)
    : m_fd{-1}
    , m_pid{pid}
    , m_tid{tid}
    , m_handle{}
    , m_receive_buffer{new uint8_t[RECEIVE_BUFFER_SIZE]()}
    , m_receive_len{0}
    , m_receive_offset{0}
    {
    }

    ~ClientStaplerTransport() override
    {
        if (isOpen())
            close();
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
        if constexpr (NON_BLOCKING == true)
            return read_non_blocking(buf, len);
        else
            return read_blocking(buf, len);
    }

    void write_virt(const uint8_t* buf, uint32_t len) override
    {
        if (!isOpen())
            openDevice();

        if constexpr (NON_BLOCKING == true)
            write_non_blocking(buf, len);
        else
            write_blocking(buf, len);
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

    void write_non_blocking(const uint8_t* buf, uint32_t len)
    {
        int status;

        struct stplr_msg smsgs[] = {
            {.msgbuf = (void*)buf, .buflen = len},
        };

        struct stplr_msg_send msg_send = {};
        msg_send.handle = m_handle;
        msg_send.pid = m_pid;
        msg_send.tid = m_tid;
        msg_send.smsgs.msgs = smsgs;
        msg_send.smsgs.count = std::size(smsgs);

        status = ::ioctl(m_fd, STPLR_MSG_SEND, &msg_send);
        assert(status >= -1);
        if (status == -1) {
            std::string msg = std::format("ioctl(STPLR_MSG_SEND) failed with code {} : {}",
                errno, strerror(errno));
            std::cout << msg << std::endl;
            throw apache::thrift::transport::TTransportException(
                apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
        }
    }

    void write_blocking(const uint8_t* buf, uint32_t len)
    {
        int status;

        struct stplr_msg smsgs[] = {
            {.msgbuf = (void*)buf, .buflen = len},
        };

        struct stplr_msg rmsgs[] = {
            {.msgbuf = m_receive_buffer.get(), .buflen = RECEIVE_BUFFER_SIZE}
        };

        struct stplr_msg_send_receive msg_send_receive = {};
        msg_send_receive.handle = m_handle;
        msg_send_receive.pid = m_pid;
        msg_send_receive.tid = m_tid;
        msg_send_receive.smsgs.msgs = smsgs;
        msg_send_receive.smsgs.count = std::size(smsgs);
        msg_send_receive.rmsgs.msgs = rmsgs;
        msg_send_receive.rmsgs.count = std::size(rmsgs);

        status = ::ioctl(m_fd, STPLR_MSG_SEND_RECEIVE, &msg_send_receive);
        assert(status >= -1);
        if (status == -1) {
            std::string msg = std::format("ioctl(STPLR_MSG_SEND_RECEIVE) failed with code {} : {}",
                errno, strerror(errno));
            std::cout << msg << std::endl;
            throw apache::thrift::transport::TTransportException(
                apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
        }

        m_receive_len = rmsgs[0].buflen;
        m_receive_offset = 0;
    }

    uint32_t read_non_blocking(uint8_t* buf, uint32_t len)
    {
        int status;

        struct stplr_msg rmsgs[] = {
            {.msgbuf = buf, .buflen = len},
        };

        struct stplr_msg_receive msg_receive;
        msg_receive.handle = m_handle;
        msg_receive.rmsgs.msgs = rmsgs;
        msg_receive.rmsgs.count = std::size(rmsgs);

        status = ::ioctl(m_fd, STPLR_MSG_RECEIVE, &msg_receive);
        assert(status >= -1);
        if (status == -1) {
            std::string msg = std::format("ioctl(STPLR_MSG_RECEIVE) failed with code {} : {}",
                errno, strerror(errno));
            std::cout << msg << std::endl;
            throw apache::thrift::transport::TTransportException(
                apache::thrift::transport::TTransportException::INTERNAL_ERROR, msg);
        }

        return rmsgs[0].buflen;
    }

    uint32_t read_blocking(uint8_t* buf, uint32_t len)
    {
        uint32_t n = std::min(m_receive_len - m_receive_offset, len);
        if (n > 0) {
            ::memcpy(buf, m_receive_buffer.get() + m_receive_offset, n);
            m_receive_offset += n;
        }

        return n;
    }

    int m_fd;
    int m_pid;
    int m_tid;
    struct stplr_handle m_handle;
    std::unique_ptr<uint8_t[]> m_receive_buffer;
    uint32_t m_receive_len;
    uint32_t m_receive_offset;
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

#endif /* _CLIENT_STAPLER_TRANSPORT_HPP_ */
