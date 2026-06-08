#include "can.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>

CAN::CAN(const std::string& interface) : interface_(interface), fd_(-1), async_running_(false) {}

CAN::~CAN() {
    close();
}

bool CAN::open() {
    fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) return false;
    
    strcpy(ifr_.ifr_name, interface_.c_str());
    if (ioctl(fd_, SIOCGIFINDEX, &ifr_) < 0) {
        close();
        return false;
    }
    
    addr_.can_family = AF_CAN;
    addr_.can_ifindex = ifr_.ifr_ifindex;
    
    if (bind(fd_, (struct sockaddr*)&addr_, sizeof(addr_)) < 0) {
        close();
        return false;
    }
    
    // Set CAN filter to accept all
    setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
    
    return true;
}

void CAN::close() {
    stopAsyncReceive();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool CAN::sendFrame(const CANFrame& frame) {
    struct can_frame cf;
    fillCANFrame(frame, cf);
    
    int bytes = write(fd_, &cf, sizeof(cf));
    if (bytes == sizeof(cf)) {
        stats_.tx_frames++;
        return true;
    }
    return false;
}

bool CAN::receiveFrame(CANFrame& frame, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd_ + 1, &fds, NULL, NULL, &timeout);
    if (result > 0) {
        struct can_frame cf;
        int bytes = read(fd_, &cf, sizeof(cf));
        if (bytes == sizeof(cf)) {
            parseCANFrame(cf, frame);
            stats_.rx_frames++;
            return true;
        }
    }
    return false;
}

void CAN::parseCANFrame(const struct can_frame& cf, CANFrame& frame) {
    frame.id = cf.can_id & CAN_EFF_MASK;
    frame.extended = (cf.can_id & CAN_EFF_FLAG) != 0;
    frame.rtr = (cf.can_id & CAN_RTR_FLAG) != 0;
    frame.error = (cf.can_id & CAN_ERR_FLAG) != 0;
    frame.len = cf.can_dlc;
    memcpy(frame.data, cf.data, frame.len);
}

void CAN::fillCANFrame(const CANFrame& frame, struct can_frame& cf) {
    memset(&cf, 0, sizeof(cf));
    
    cf.can_id = frame.id;
    if (frame.extended) cf.can_id |= CAN_EFF_FLAG;
    if (frame.rtr) cf.can_id |= CAN_RTR_FLAG;
    
    cf.can_dlc = frame.len;
    memcpy(cf.data, frame.data, frame.len);
}
