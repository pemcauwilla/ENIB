#pragma once

#include <cstdint>
#include <string>

#include <linux/can.h>


namespace scpp
{

    struct CanFrame // Payload CAN
    {
        uint32_t id = 0;
        uint8_t len = 0;
        uint8_t flags = 0;
        uint8_t data[64]; // 64 bytes to store either the CANFD mode or the default CAN MTU

    };

    enum SocketCanStatus // Error codes
    {
        STATUS_OK = 1 << 0,
        STATUS_SOCKET_CREATE_ERROR = 1 << 2,
        STATUS_INTERFACE_NAME_TO_IDX_ERROR = 1 << 3,
        STATUS_MTU_ERROR = 1 << 4, /// maximum transfer unit
        STATUS_CANFD_NOT_SUPPORTED = 1 << 5, /// Flexible data-rate is not supported on this interface
        STATUS_ENABLE_FD_SUPPORT_ERROR = 1 << 6, /// Error on enabling fexible-data-rate support
        STATUS_WRITE_ERROR = 1 << 7,
        STATUS_READ_ERROR = 1 << 8,
        STATUS_BIND_ERROR = 1 << 9,
    };

    class SocketCan
    {
    public:
        SocketCan(); // Default constructor
        SocketCan(const SocketCan &) = delete; // Delete copy constructor
        SocketCan & operator=(const SocketCan &) = delete; // Delete assignment operator
        SocketCanStatus open(const std::string & can_interface, int32_t read_timeout_ms = 3);
        SocketCanStatus write(const CanFrame & msg);
        SocketCanStatus read(CanFrame & msg);
        SocketCanStatus close();
        const std::string & interfaceName() const;
        ~SocketCan();

        int socketFd() const {return m_socket;} // Needed by QSocketNotifier
    private:
        int m_socket = -1;
        int32_t m_read_timeout_ms = 3;
        std::string m_interface;
    };
}
