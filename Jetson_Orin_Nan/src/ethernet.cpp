#include "ethernet.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>

// Ethernet implementation
Ethernet::Ethernet(const std::string& interface) 
    : interface_(interface), socket_fd_(-1) {}

Ethernet::~Ethernet() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

bool Ethernet::ioctlCommand(unsigned long request, struct ifreq& ifr) {
    if (socket_fd_ < 0) {
        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) return false;
    }
    
    strncpy(ifr.ifr_name, interface_.c_str(), IFNAMSIZ);
    return ioctl(socket_fd_, request, &ifr) >= 0;
}

std::string Ethernet::MACAddress::toString() const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return std::string(buf);
}

bool Ethernet::MACAddress::fromString(const std::string& str) {
    return sscanf(str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &bytes[0], &bytes[1], &bytes[2],
                  &bytes[3], &bytes[4], &bytes[5]) == 6;
}

bool Ethernet::MACAddress::isBroadcast() const {
    return bytes[0] == 0xFF && bytes[1] == 0xFF && bytes[2] == 0xFF &&
           bytes[3] == 0xFF && bytes[4] == 0xFF && bytes[5] == 0xFF;
}

bool Ethernet::MACAddress::isMulticast() const {
    return bytes[0] & 0x01;
}

bool Ethernet::MACAddress::isUnicast() const {
    return !isMulticast() && !isBroadcast();
}

std::string Ethernet::IPAddress::toString() const {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

bool Ethernet::IPAddress::fromString(const std::string& str) {
    return inet_pton(AF_INET, str.c_str(), &addr) == 1;
}

bool Ethernet::IPAddress::isPrivate() const {
    uint32_t ip = ntohl(addr);
    return (ip >= 0x0A000000 && ip <= 0x0AFFFFFF) ||      // 10.0.0.0/8
           (ip >= 0xAC100000 && ip <= 0xAC1FFFFF) ||      // 172.16.0.0/12
           (ip >= 0xC0A80000 && ip <= 0xC0A8FFFF);        // 192.168.0.0/16
}

bool Ethernet::setMACAddress(const MACAddress& mac) {
    struct ifreq ifr;
    if (!ioctlCommand(SIOCGIFHWADDR, ifr)) return false;
    
    memcpy(ifr.ifr_hwaddr.sa_data, mac.bytes, 6);
    return ioctlCommand(SIOCSIFHWADDR, ifr);
}

Ethernet::MACAddress Ethernet::getMACAddress() const {
    struct ifreq ifr;
    MACAddress mac;
    
    if (!const_cast<Ethernet*>(this)->ioctlCommand(SIOCGIFHWADDR, ifr)) {
        memset(&mac, 0, sizeof(mac));
        return mac;
    }
    
    memcpy(mac.bytes, ifr.ifr_hwaddr.sa_data, 6);
    return mac;
}

bool Ethernet::setIPAddress(const IPAddress& ip, const IPAddress& netmask) {
    struct ifreq ifr;
    
    struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = ip.addr;
    if (!ioctlCommand(SIOCSIFADDR, ifr)) return false;
    
    addr = (struct sockaddr_in*)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = netmask.addr;
    return ioctlCommand(SIOCSIFNETMASK, ifr);
}

bool Ethernet::setGateway(const IPAddress& gateway) {
    FILE* fp = fopen("/proc/sys/net/ipv4/ip_forward", "w");
    if (fp) {
        fprintf(fp, "1");
        fclose(fp);
    }
    
    char command[256];
    snprintf(command, sizeof(command), 
             "ip route add default via %s dev %s 2>/dev/null",
             gateway.toString().c_str(), interface_.c_str());
    return system(command) == 0;
}

bool Ethernet::setDNS(const std::vector<IPAddress>& dns_servers) {
    std::ofstream resolv("/etc/resolv.conf");
    if (!resolv.is_open()) return false;
    
    for (const auto& dns : dns_servers) {
        resolv << "nameserver " << dns.toString() << "\n";
    }
    
    return true;
}

bool Ethernet::setMTU(int mtu) {
    struct ifreq ifr;
    if (!ioctlCommand(SIOCGIFMTU, ifr)) return false;
    
    ifr.ifr_mtu = mtu;
    return ioctlCommand(SIOCSIFMTU, ifr);
}

int Ethernet::getMTU() const {
    struct ifreq ifr;
    if (!const_cast<Ethernet*>(this)->ioctlCommand(SIOCGIFMTU, ifr)) {
        return 1500;
    }
    return ifr.ifr_mtu;
}

bool Ethernet::setLinkUp(bool up) {
    struct ifreq ifr;
    if (!ioctlCommand(SIOCGIFFLAGS, ifr)) return false;
    
    if (up) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }
    return ioctlCommand(SIOCSIFFLAGS, ifr);
}

bool Ethernet::isLinkUp() const {
    struct ifreq ifr;
    if (!const_cast<Ethernet*>(this)->ioctlCommand(SIOCGIFFLAGS, ifr)) {
        return false;
    }
    return ifr.ifr_flags & IFF_RUNNING;
}

Ethernet::LinkStats Ethernet::getStats() const {
    LinkStats stats = {0};
    
    std::string base_path = "/sys/class/net/" + interface_ + "/statistics/";
    
    auto read_stat = [&](const std::string& name) -> uint64_t {
        std::ifstream file(base_path + name);
        uint64_t value;
        file >> value;
        return value;
    };
    
    stats.rx_bytes = read_stat("rx_bytes");
    stats.tx_bytes = read_stat("tx_bytes");
    stats.rx_packets = read_stat("rx_packets");
    stats.tx_packets = read_stat("tx_packets");
    stats.rx_errors = read_stat("rx_errors");
    stats.tx_errors = read_stat("tx_errors");
    stats.rx_dropped = read_stat("rx_dropped");
    stats.tx_dropped = read_stat("tx_dropped");
    stats.rx_multicast = read_stat("multicast");
    
    std::ifstream speed_file("/sys/class/net/" + interface_ + "/speed");
    if (speed_file) speed_file >> stats.link_speed;
    
    std::ifstream duplex_file("/sys/class/net/" + interface_ + "/duplex");
    if (duplex_file) duplex_file >> stats.duplex;
    
    return stats;
}

void Ethernet::printStats() const {
    auto stats = getStats();
    printf("=== Ethernet Statistics (%s) ===\n", interface_.c_str());
    printf("RX: %lu bytes, %lu packets\n", stats.rx_bytes, stats.rx_packets);
    printf("TX: %lu bytes, %lu packets\n", stats.tx_bytes, stats.tx_packets);
    printf("Errors - RX: %lu, TX: %lu\n", stats.rx_errors, stats.tx_errors);
    printf("Dropped - RX: %lu, TX: %lu\n", stats.rx_dropped, stats.tx_dropped);
    printf("Speed: %u Mbps, Duplex: %s\n", stats.link_speed, stats.duplex.c_str());
}

bool Ethernet::setAutoNegotiation(bool enable) {
    std::ofstream adv("/sys/class/net/" + interface_ + "/autoneg");
    if (adv.is_open()) {
        adv << (enable ? "1" : "0");
        return true;
    }
    return false;
}

bool Ethernet::setSpeed(int speed_mbps) {
    std::ofstream speed_file("/sys/class/net/" + interface_ + "/speed");
    if (speed_file.is_open()) {
        speed_file << speed_mbps;
        return true;
    }
    return false;
}

bool Ethernet::setDuplex(const std::string& duplex) {
    std::ofstream duplex_file("/sys/class/net/" + interface_ + "/duplex");
    if (duplex_file.is_open()) {
        duplex_file << duplex;
        return true;
    }
    return false;
}

bool Ethernet::setWakeOnLAN(bool enable) {
    std::ofstream wol("/sys/class/net/" + interface_ + "/device/power/wakeup");
    if (wol.is_open()) {
        wol << (enable ? "enabled" : "disabled");
        return true;
    }
    return false;
}

std::vector<std::string> Ethernet::getAvailableInterfaces() {
    std::vector<std::string> interfaces;
    
    DIR* dir = opendir("/sys/class/net");
    if (!dir) return interfaces;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        interfaces.push_back(std::string(entry->d_name));
    }
    
    closedir(dir);
    return interfaces;
}

// RawSocket implementation
Ethernet::RawSocket::RawSocket(const std::string& interface) : fd_(-1), interface_(interface) {
    fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd_ < 0) {
        throw std::runtime_error("Failed to create raw socket (need root)");
    }
    
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex(interface.c_str());
    
    if (bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd_);
        throw std::runtime_error("Failed to bind to interface");
    }
}

Ethernet::RawSocket::~RawSocket() {
    if (fd_ >= 0) close(fd_);
}

bool Ethernet::RawSocket::send(const uint8_t* data, size_t len) {
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_nametoindex(interface_.c_str());
    
    ssize_t sent = sendto(fd_, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    return sent == (ssize_t)len;
}

ssize_t Ethernet::RawSocket::receive(uint8_t* buffer, size_t max_len, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd_ + 1, &fds, NULL, NULL, &timeout);
    if (result > 0) {
        struct sockaddr_ll addr;
        socklen_t addr_len = sizeof(addr);
        return recvfrom(fd_, buffer, max_len, 0, (struct sockaddr*)&addr, &addr_len);
    }
    return 0;
}

void Ethernet::RawSocket::setPromiscuous(bool enable) {
    struct packet_mreq mr;
    mr.mr_ifindex = if_nametoindex(interface_.c_str());
    mr.mr_type = PACKET_MR_PROMISC;
    mr.mr_alen = 0;
    
    setsockopt(fd_, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
}

// TCPSocket implementation
Ethernet::TCPSocket::TCPSocket() : fd_(-1), connected_(false) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to create TCP socket");
    }
}

Ethernet::TCPSocket::~TCPSocket() {
    disconnect();
    if (fd_ >= 0) close(fd_);
}

bool Ethernet::TCPSocket::connect(const IPAddress& ip, uint16_t port, int timeout_ms) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip.addr;
    
    // Set non-blocking for timeout
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    
    int result = ::connect(fd_, (struct sockaddr*)&addr, sizeof(addr));
    
    if (result < 0 && errno == EINPROGRESS) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        result = select(fd_ + 1, NULL, &fds, NULL, &tv);
        
        if (result > 0) {
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_error, &len);
            result = (so_error == 0) ? 0 : -1;
        } else {
            result = -1;
        }
    }
    
    // Restore blocking mode
    fcntl(fd_, F_SETFL, flags);
    
    if (result == 0) {
        connected_ = true;
        return true;
    }
    
    return false;
}

void Ethernet::TCPSocket::disconnect() {
    if (connected_) {
        shutdown(fd_, SHUT_RDWR);
        connected_ = false;
    }
}

ssize_t Ethernet::TCPSocket::send(const uint8_t* data, size_t len) {
    if (!connected_) return -1;
    return ::send(fd_, data, len, 0);
}

ssize_t Ethernet::TCPSocket::receive(uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!connected_) return -1;
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    return recv(fd_, buffer, max_len, 0);
}

bool Ethernet::TCPSocket::isConnected() const {
    return connected_;
}

// UDPSocket implementation
Ethernet::UDPSocket::UDPSocket() : fd_(-1) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }
    
    int reuse = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

Ethernet::UDPSocket::~UDPSocket() {
    if (fd_ >= 0) close(fd_);
}

bool Ethernet::UDPSocket::bind(uint16_t port, const IPAddress& ip) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip.addr;
    
    return ::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

ssize_t Ethernet::UDPSocket::sendTo(const uint8_t* data, size_t len, 
                                     const IPAddress& ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip.addr;
    
    return sendto(fd_, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
}

ssize_t Ethernet::UDPSocket::receiveFrom(uint8_t* buffer, size_t max_len,
                                          IPAddress* ip, uint16_t* port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t bytes = recvfrom(fd_, buffer, max_len, 0, 
                              (struct sockaddr*)&addr, &addr_len);
    
    if (bytes > 0) {
        if (ip) ip->addr = addr.sin_addr.s_addr;
        if (port) *port = ntohs(addr.sin_port);
    }
    
    return bytes;
}

void Ethernet::UDPSocket::setBroadcast(bool enable) {
    int broadcast = enable ? 1 : 0;
    setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
}

void Ethernet::UDPSocket::joinMulticast(const IPAddress& multicast_ip) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = multicast_ip.addr;
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
}

// HTTPClient implementation
Ethernet::HTTPClient::HTTPClient() {
    socket_ = std::make_unique<TCPSocket>();
}

Ethernet::HTTPClient::~HTTPClient() {}

bool Ethernet::HTTPClient::connect(const std::string& host, uint16_t port) {
    host_ = host;
    IPAddress ip;
    
    // DNS resolution
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) return false;
    
    memcpy(&ip.addr, he->h_addr_list[0], he->h_length);
    
    return socket_->connect(ip, port);
}

std::string Ethernet::HTTPClient::get(const std::string& path) {
    std::string request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host_ + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    
    socket_->send((const uint8_t*)request.c_str(), request.length());
    
    std::string response;
    uint8_t buffer[4096];
    ssize_t bytes;
    
    while ((bytes = socket_->receive(buffer, sizeof(buffer), 5000)) > 0) {
        response.append((char*)buffer, bytes);
    }
    
    return response;
}

std::string Ethernet::HTTPClient::post(const std::string& path, const std::string& data,
                                        const std::string& content_type) {
    std::string request = "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host_ + "\r\n";
    request += "Content-Type: " + content_type + "\r\n";
    request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    request += data;
    
    socket_->send((const uint8_t*)request.c_str(), request.length());
    
    std::string response;
    uint8_t buffer[4096];
    ssize_t bytes;
    
    while ((bytes = socket_->receive(buffer, sizeof(buffer), 5000)) > 0) {
        response.append((char*)buffer, bytes);
    }
    
    return response;
}
