#include "socketcan_cpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can/raw.h>

namespace scpp
{
    SocketCan::SocketCan()
    {
    }

    SocketCanStatus SocketCan::open(const std::string & can_interface, int32_t read_timeout_ms)
    {
        m_interface = can_interface;
        m_read_timeout_ms = read_timeout_ms;

        /* open socket */
        if ((m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
        {
            perror("socket");
            return STATUS_SOCKET_CREATE_ERROR;
        }

        struct sockaddr_can addr;
        struct ifreq ifr;

        strncpy(ifr.ifr_name, can_interface.c_str(), IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
        ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
        if (!ifr.ifr_ifindex) {
            perror("if_nametoindex");
            return STATUS_INTERFACE_NAME_TO_IDX_ERROR;
        }

        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        // LINUX
        struct timeval tv;
        tv.tv_sec = 0;  /* 30 Secs Timeout */
        tv.tv_usec = m_read_timeout_ms * 1000;  // Not init'ing this can cause strange errors
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

        if (bind(m_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return STATUS_BIND_ERROR;
        }
        return STATUS_OK;
    }

    SocketCanStatus SocketCan::write(const CanFrame & msg)
    {
        struct can_frame frame;
        memset(&frame, 0, sizeof(frame)); /* init CAN frame, e.g. LEN = 0 */

        //convert CanFrame to canfd_frame
        frame.can_id = msg.id;
        frame.len = msg.len;
        memcpy(frame.data, msg.data, msg.len);

        /* send frame */
        if (::write(m_socket, &frame, sizeof(frame)) != sizeof(struct can_frame)) {
            perror("write");
            return STATUS_WRITE_ERROR;
        }
        return STATUS_OK;
    }

    SocketCanStatus SocketCan::read(CanFrame & msg)
    {
        struct can_frame frame;

        // Read in a CAN frame
        auto num_bytes = ::read(m_socket, &frame, sizeof(struct can_frame));
        if (num_bytes != sizeof(struct can_frame))
        {
            //perror("Can read error");
            return STATUS_READ_ERROR;
        }

        msg.id = frame.can_id;
        msg.len = frame.len;
        memcpy(msg.data, frame.data, frame.len);

        return STATUS_OK;
    }

    SocketCanStatus SocketCan::close()
    {
        ::close(m_socket);
        return STATUS_OK;
    }

    const std::string & SocketCan::interfaceName() const
    {
        return m_interface;
    }

    SocketCan::~SocketCan()
    {
        close();
    }
}
