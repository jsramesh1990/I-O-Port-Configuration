
## docs/ethernet.md

```markdown
# Ethernet Interface

## Overview

The Jetson Orin Nano features a Gigabit Ethernet controller supporting 10/100/1000 Mbps operation with hardware acceleration for network protocols.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| Controller | Realtek RTL8211F (or similar) |
| Interface | RGMII (Reduced Gigabit Media Independent Interface) |
| Max Speed | 1000 Mbps (Gigabit) |
| Auto-negotiation | Yes |
| MDI/MDIX Auto-crossover | Yes |
| Jumbo Frames | Up to 9KB |
| Hardware Checksum | TCP/UDP/IP checksum offload |
| Wake-on-LAN | Supported |
| IEEE 1588 | Precision Time Protocol (PTP) |
| PHY Address | 0x00 (default) |
| MAC Address | Factory programmed in eFUSE |

## Pin Configuration

### Ethernet PHY Interface
```
Jetson Orin Nano         RTL8211F PHY
┌─────────────┐         ┌─────────────┐
│ RGMII_TXD0  ├────────►│ TXD0        │
│ RGMII_TXD1  ├────────►│ TXD1        │
│ RGMII_TXD2  ├────────►│ TXD2        │
│ RGMII_TXD3  ├────────►│ TXD3        │
│ RGMII_TX_CTL├────────►│ TX_CTL      │
│ RGMII_TXC   ├────────►│ TXC         │
│             │         │             │
│ RGMII_RXD0  │◄────────┤ RXD0        │
│ RGMII_RXD1  │◄────────┤ RXD1        │
│ RGMII_RXD2  │◄────────┤ RXD2        │
│ RGMII_RXD3  │◄────────┤ RXD3        │
│ RGMII_RX_CTL│◄────────┤ RX_CTL      │
│ RGMII_RXC   │◄────────┤ RXC         │
│             │         │             │
│ MDC         ├────────►│ MDC         │
│ MDIO        │◄───────►│ MDIO        │
└─────────────┘         └─────────────┘
```

## PHY Registers (RTL8211F)

| Register | Address | Description |
|----------|---------|-------------|
| BMCR | 0x00 | Basic Mode Control Register |
| BMSR | 0x01 | Basic Mode Status Register |
| PHYIDR1 | 0x02 | PHY Identifier Register 1 |
| PHYIDR2 | 0x03 | PHY Identifier Register 2 |
| ANAR | 0x04 | Auto-Negotiation Advertisement Register |
| ANLPAR | 0x05 | Auto-Negotiation Link Partner Ability Register |
| ANER | 0x06 | Auto-Negotiation Expansion Register |
| ANNPTR | 0x07 | Auto-Negotiation Next Page Register |
| GBCR | 0x09 | Gigabit Control Register |
| GBSR | 0x0A | Gigabit Status Register |
| MMD_ACCESS | 0x0D | MMD Access Register |
| EEE_CTRL | 0x0E | Energy Efficient Ethernet Control |

## Implementation

### Ethernet Configuration Class
```cpp
class EthernetConfig {
private:
    std::string interface_name;
    int socket_fd;
    struct ifreq ifr;
    
    bool ioctlCommand(unsigned long request) {
        ifr.ifr_ifindex = if_nametoindex(interface_name.c_str());
        return ioctl(socket_fd, request, &ifr) >= 0;
    }
    
public:
    EthernetConfig(const std::string& iface) : interface_name(iface) {
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(socket_fd < 0) {
            throw std::runtime_error("Cannot create socket");
        }
        strncpy(ifr.ifr_name, interface_name.c_str(), IFNAMSIZ);
    }
    
    ~EthernetConfig() {
        close(socket_fd);
    }
    
    bool setMACAddress(const std::string& mac) {
        unsigned char mac_addr[6];
        if(sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &mac_addr[0], &mac_addr[1], &mac_addr[2],
                  &mac_addr[3], &mac_addr[4], &mac_addr[5]) != 6) {
            return false;
        }
        
        ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
        memcpy(ifr.ifr_hwaddr.sa_data, mac_addr, 6);
        return ioctlCommand(SIOCSIFHWADDR);
    }
    
    std::string getMACAddress() {
        if(!ioctlCommand(SIOCGIFHWADDR)) return "";
        
        char mac[18];
        sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
        return std::string(mac);
    }
    
    bool setIPAddress(const std::string& ip, const std::string& netmask) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
        addr->sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
        if(!ioctlCommand(SIOCSIFADDR)) return false;
        
        addr = (struct sockaddr_in*)&ifr.ifr_netmask;
        inet_pton(AF_INET, netmask.c_str(), &addr->sin_addr);
        return ioctlCommand(SIOCSIFNETMASK);
    }
    
    bool setGateway(const std::string& gateway) {
        FILE* fp = fopen("/proc/sys/net/ipv4/ip_forward", "w");
        if(fp) {
            fprintf(fp, "1");
            fclose(fp);
        }
        
        char command[256];
        snprintf(command, sizeof(command), 
                 "route add default gw %s dev %s", 
                 gateway.c_str(), interface_name.c_str());
        return system(command) == 0;
    }
    
    bool setMTU(int mtu) {
        ifr.ifr_mtu = mtu;
        return ioctlCommand(SIOCSIFMTU);
    }
    
    int getMTU() {
        if(!ioctlCommand(SIOCGIFMTU)) return 0;
        return ifr.ifr_mtu;
    }
    
    bool setLinkUp(bool up) {
        if(!ioctlCommand(SIOCGIFFLAGS)) return false;
        
        if(up) {
            ifr.ifr_flags |= IFF_UP;
        } else {
            ifr.ifr_flags &= ~IFF_UP;
        }
        return ioctlCommand(SIOCSIFFLAGS);
    }
    
    bool isLinkUp() {
        if(!ioctlCommand(SIOCGIFFLAGS)) return false;
        return ifr.ifr_flags & IFF_RUNNING;
    }
    
    uint32_t getSpeed() {
        FILE* fp = fopen("/sys/class/net/eth0/speed", "r");
        if(!fp) return 0;
        
        int speed;
        fscanf(fp, "%d", &speed);
        fclose(fp);
        return speed;
    }
    
    std::string getDuplex() {
        FILE* fp = fopen("/sys/class/net/eth0/duplex", "r");
        if(!fp) return "unknown";
        
        char duplex[16];
        fscanf(fp, "%s", duplex);
        fclose(fp);
        return std::string(duplex);
    }
};
```

### Raw Socket Ethernet Frame Handling
```cpp
class RawEthernet {
private:
    int socket_fd;
    std::string interface_name;
    
    struct ether_header {
        uint8_t dest_mac[6];
        uint8_t src_mac[6];
        uint16_t ether_type;
    } __attribute__((packed));
    
public:
    RawEthernet(const std::string& iface) : interface_name(iface) {
        // Create raw socket
        socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if(socket_fd < 0) {
            throw std::runtime_error("Cannot create raw socket (need root)");
        }
        
        // Bind to interface
        struct sockaddr_ll addr;
        memset(&addr, 0, sizeof(addr));
        addr.sll_family = AF_PACKET;
        addr.sll_protocol = htons(ETH_P_ALL);
        addr.sll_ifindex = if_nametoindex(interface_name.c_str());
        
        if(bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(socket_fd);
            throw std::runtime_error("Cannot bind to interface");
        }
        
        // Set non-blocking mode
        int flags = fcntl(socket_fd, F_GETFL, 0);
        fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    ~RawEthernet() {
        close(socket_fd);
    }
    
    bool sendFrame(const uint8_t* data, size_t len) {
        struct sockaddr_ll addr;
        memset(&addr, 0, sizeof(addr));
        addr.sll_family = AF_PACKET;
        addr.sll_ifindex = if_nametoindex(interface_name.c_str());
        
        ssize_t sent = sendto(socket_fd, data, len, 0,
                              (struct sockaddr*)&addr, sizeof(addr));
        return sent == (ssize_t)len;
    }
    
    ssize_t receiveFrame(uint8_t* buffer, size_t max_len, int timeout_ms = 100) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(socket_fd, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int result = select(socket_fd + 1, &fds, NULL, NULL, &timeout);
        if(result > 0) {
            struct sockaddr_ll addr;
            socklen_t addr_len = sizeof(addr);
            return recvfrom(socket_fd, buffer, max_len, 0,
                           (struct sockaddr*)&addr, &addr_len);
        }
        return 0;
    }
    
    void sendARPRequest(const std::string& target_ip) {
        struct arp_packet {
            struct ether_header eth;
            struct ether_arp arp;
        } packet;
        
        // Ethernet header
        memset(packet.eth.dest_mac, 0xFF, 6);  // Broadcast
        // Source MAC (get from interface)
        getInterfaceMAC(packet.eth.src_mac);
        packet.eth.ether_type = htons(ETH_P_ARP);
        
        // ARP header
        packet.arp.arp_hrd = htons(ARPHRD_ETHER);
        packet.arp.arp_pro = htons(ETH_P_IP);
        packet.arp.arp_hln = 6;
        packet.arp.arp_pln = 4;
        packet.arp.arp_op = htons(ARPOP_REQUEST);
        
        // Sender MAC and IP
        memcpy(packet.arp.arp_sha, packet.eth.src_mac, 6);
        inet_pton(AF_INET, getInterfaceIP().c_str(), packet.arp.arp_spa);
        
        // Target MAC (unknown)
        memset(packet.arp.arp_tha, 0, 6);
        inet_pton(AF_INET, target_ip.c_str(), packet.arp.arp_tpa);
        
        sendFrame((uint8_t*)&packet, sizeof(packet));
    }
    
private:
    void getInterfaceMAC(uint8_t* mac) {
        struct ifreq ifr;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        strcpy(ifr.ifr_name, interface_name.c_str());
        ioctl(fd, SIOCGIFHWADDR, &ifr);
        memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        close(fd);
    }
    
    std::string getInterfaceIP() {
        struct ifreq ifr;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        strcpy(ifr.ifr_name, interface_name.c_str());
        ioctl(fd, SIOCGIFADDR, &ifr);
        close(fd);
        
        char ip[16];
        inet_ntop(AF_INET, &((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr, ip, sizeof(ip));
        return std::string(ip);
    }
};
```

### UDP Socket Implementation
```cpp
class UDPSocket {
private:
    int sock_fd;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    bool connected;
    
public:
    UDPSocket() : sock_fd(-1), connected(false) {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock_fd < 0) {
            throw std::runtime_error("Cannot create UDP socket");
        }
        
        // Set socket options
        int reuse = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        
        // Set timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    ~UDPSocket() {
        if(sock_fd >= 0) close(sock_fd);
    }
    
    bool bind(uint16_t port, const std::string& address = "0.0.0.0") {
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &local_addr.sin_addr);
        
        return ::bind(sock_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) == 0;
    }
    
    bool connect(const std::string& host, uint16_t port) {
        memset(&remote_addr, 0, sizeof(remote_addr));
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(port);
        
        if(inet_pton(AF_INET, host.c_str(), &remote_addr.sin_addr) <= 0) {
            // Try DNS resolution
            struct hostent* he = gethostbyname(host.c_str());
            if(!he) return false;
            memcpy(&remote_addr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        connected = true;
        return true;
    }
    
    ssize_t send(const uint8_t* data, size_t len) {
        if(connected) {
            return sendto(sock_fd, data, len, 0,
                         (struct sockaddr*)&remote_addr, sizeof(remote_addr));
        }
        return -1;
    }
    
    ssize_t sendTo(const uint8_t* data, size_t len, const std::string& host, uint16_t port) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        return sendto(sock_fd, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    }
    
    ssize_t receive(uint8_t* buffer, size_t max_len, std::string* sender_ip = nullptr, 
                    uint16_t* sender_port = nullptr) {
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        ssize_t bytes = recvfrom(sock_fd, buffer, max_len, 0,
                                (struct sockaddr*)&sender_addr, &addr_len);
        
        if(bytes > 0 && sender_ip) {
            char ip[16];
            inet_ntop(AF_INET, &sender_addr.sin_addr, ip, sizeof(ip));
            *sender_ip = std::string(ip);
            if(sender_port) *sender_port = ntohs(sender_addr.sin_port);
        }
        
        return bytes;
    }
    
    void setBroadcast(bool enable) {
        int broadcast = enable ? 1 : 0;
        setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    }
    
    void setMulticastGroup(const std::string& multicast_ip, const std::string& interface_ip) {
        struct ip_mreq mreq;
        inet_pton(AF_INET, multicast_ip.c_str(), &mreq.imr_multiaddr);
        inet_pton(AF_INET, interface_ip.c_str(), &mreq.imr_interface);
        
        setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    }
};
```

### TCP Server Implementation
```cpp
class TCPServer {
private:
    int listen_fd;
    int client_fd;
    struct sockaddr_in server_addr;
    bool running;
    std::thread accept_thread;
    std::function<void(int)> connection_callback;
    
public:
    TCPServer(uint16_t port) : listen_fd(-1), client_fd(-1), running(false) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(listen_fd < 0) {
            throw std::runtime_error("Cannot create TCP socket");
        }
        
        // Set socket options
        int reuse = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        
        // Bind to port
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(listen_fd);
            throw std::runtime_error("Cannot bind to port");
        }
        
        // Start listening
        listen(listen_fd, 5);
    }
    
    ~TCPServer() {
        stop();
        if(listen_fd >= 0) close(listen_fd);
    }
    
    void onConnection(std::function<void(int)> callback) {
        connection_callback = callback;
    }
    
    void start() {
        running = true;
        accept_thread = std::thread([this]() {
            while(running) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                
                client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
                
                if(client_fd > 0 && connection_callback) {
                    char ip[16];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
                    printf("Client connected from %s:%d\n", ip, ntohs(client_addr.sin_port));
                    
                    connection_callback(client_fd);
                    close(client_fd);
                }
            }
        });
    }
    
    void stop() {
        running = false;
        if(accept_thread.joinable()) {
            accept_thread.join();
        }
    }
};

class TCPClient {
private:
    int sock_fd;
    bool connected;
    
public:
    TCPClient() : sock_fd(-1), connected(false) {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(sock_fd < 0) {
            throw std::runtime_error("Cannot create TCP socket");
        }
    }
    
    ~TCPClient() {
        disconnect();
        if(sock_fd >= 0) close(sock_fd);
    }
    
    bool connect(const std::string& host, uint16_t port, int timeout_ms = 5000) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        // DNS resolution
        if(inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
            struct hostent* he = gethostbyname(host.c_str());
            if(!he) return false;
            memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
        }
        
        // Set non-blocking for timeout
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        
        int result = ::connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        if(result < 0) {
            if(errno == EINPROGRESS) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(sock_fd, &fds);
                
                struct timeval timeout;
                timeout.tv_sec = timeout_ms / 1000;
                timeout.tv_usec = (timeout_ms % 1000) * 1000;
                
                result = select(sock_fd + 1, NULL, &fds, NULL, &timeout);
                
                if(result > 0) {
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if(so_error == 0) {
                        result = 0;  // Connected successfully
                    }
                } else {
                    result = -1;  // Timeout or error
                }
            }
        }
        
        // Restore blocking mode
        fcntl(sock_fd, F_SETFL, flags);
        
        if(result == 0) {
            connected = true;
            return true;
        }
        
        return false;
    }
    
    void disconnect() {
        if(connected) {
            shutdown(sock_fd, SHUT_RDWR);
            connected = false;
        }
    }
    
    ssize_t send(const uint8_t* data, size_t len) {
        if(!connected) return -1;
        return ::send(sock_fd, data, len, 0);
    }
    
    ssize_t receive(uint8_t* buffer, size_t max_len, int timeout_ms = 5000) {
        if(!connected) return -1;
        
        // Set receive timeout
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        return recv(sock_fd, buffer, max_len, 0);
    }
    
    bool isConnected() const {
        return connected;
    }
};
```

### HTTP/HTTPS Client
```cpp
class HTTPClient {
private:
    TCPClient tcp;
    std::string host;
    uint16_t port;
    
    std::string urlEncode(const std::string& str) {
        std::string encoded;
        for(char c : str) {
            if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                encoded += hex;
            }
        }
        return encoded;
    }
    
public:
    HTTPClient() : port(80) {}
    
    bool connect(const std::string& server, uint16_t server_port = 80) {
        host = server;
        port = server_port;
        return tcp.connect(host, port);
    }
    
    std::string get(const std::string& path, 
                    const std::map<std::string, std::string>& headers = {}) {
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Connection: close\r\n";
        
        for(const auto& header : headers) {
            request += header.first + ": " + header.second + "\r\n";
        }
        
        request += "\r\n";
        
        tcp.send((uint8_t*)request.c_str(), request.length());
        
        std::string response;
        uint8_t buffer[4096];
        ssize_t bytes;
        
        while((bytes = tcp.receive(buffer, sizeof(buffer), 5000)) > 0) {
            response.append((char*)buffer, bytes);
        }
        
        return response;
    }
    
    std::string post(const std::string& path, const std::string& data,
                    const std::string& content_type = "application/x-www-form-urlencoded") {
        std::string request = "POST " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Content-Type: " + content_type + "\r\n";
        request += "Content-Length: " + std::to_string(data.length()) + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        request += data;
        
        tcp.send((uint8_t*)request.c_str(), request.length());
        
        std::string response;
        uint8_t buffer[4096];
        ssize_t bytes;
        
        while((bytes = tcp.receive(buffer, sizeof(buffer), 5000)) > 0) {
            response.append((char*)buffer, bytes);
        }
        
        return response;
    }
    
    std::string postJSON(const std::string& path, const nlohmann::json& json_data) {
        return post(path, json_data.dump(), "application/json");
    }
};
```

### Network Performance Monitoring
```cpp
class NetworkMonitor {
private:
    std::string interface_name;
    uint64_t last_rx_bytes;
    uint64_t last_tx_bytes;
    std::chrono::steady_clock::time_point last_time;
    
    uint64_t readStat(const std::string& stat_name) {
        std::string path = "/sys/class/net/" + interface_name + "/statistics/" + stat_name;
        FILE* fp = fopen(path.c_str(), "r");
        if(!fp) return 0;
        
        uint64_t value;
        fscanf(fp, "%lu", &value);
        fclose(fp);
        return value;
    }
    
public:
    NetworkMonitor(const std::string& iface) : interface_name(iface) {
        last_rx_bytes = readStat("rx_bytes");
        last_tx_bytes = readStat("tx_bytes");
        last_time = std::chrono::steady_clock::now();
    }
    
    struct Stats {
        uint64_t rx_bytes;
        uint64_t tx_bytes;
        uint64_t rx_packets;
        uint64_t tx_packets;
        uint64_t rx_errors;
        uint64_t tx_errors;
        uint64_t rx_dropped;
        uint64_t tx_dropped;
        uint64_t multicast;
        double rx_rate_mbps;
        double tx_rate_mbps;
    };
    
    Stats getStats() {
        Stats stats;
        stats.rx_bytes = readStat("rx_bytes");
        stats.tx_bytes = readStat("tx_bytes");
        stats.rx_packets = readStat("rx_packets");
        stats.tx_packets = readStat("tx_packets");
        stats.rx_errors = readStat("rx_errors");
        stats.tx_errors = readStat("tx_errors");
        stats.rx_dropped = readStat("rx_dropped");
        stats.tx_dropped = readStat("tx_dropped");
        stats.multicast = readStat("multicast");
        
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_time).count();
        
        if(elapsed > 0) {
            stats.rx_rate_mbps = ((stats.rx_bytes - last_rx_bytes) * 8.0) / (elapsed * 1000000);
            stats.tx_rate_mbps = ((stats.tx_bytes - last_tx_bytes) * 8.0) / (elapsed * 1000000);
        } else {
            stats.rx_rate_mbps = 0;
            stats.tx_rate_mbps = 0;
        }
        
        last_rx_bytes = stats.rx_bytes;
        last_tx_bytes = stats.tx_bytes;
        last_time = now;
        
        return stats;
    }
    
    void printStats() {
        Stats stats = getStats();
        printf("=== Network Statistics (%s) ===\n", interface_name.c_str());
        printf("RX: %lu bytes, %lu packets\n", stats.rx_bytes, stats.rx_packets);
        printf("TX: %lu bytes, %lu packets\n", stats.tx_bytes, stats.tx_packets);
        printf("Errors - RX: %lu, TX: %lu\n", stats.rx_errors, stats.tx_errors);
        printf("Dropped - RX: %lu, TX: %lu\n", stats.rx_dropped, stats.tx_dropped);
        printf("Rate - RX: %.2f Mbps, TX: %.2f Mbps\n", stats.rx_rate_mbps, stats.tx_rate_mbps);
    }
};
```

## Performance Optimization

### Zero-Copy Packet Processing
```cpp
class ZeroCopyEthernet {
private:
    int socket_fd;
    void* ring_buffer;
    size_t ring_size;
    int ring_fd;
    
public:
    ZeroCopyEthernet(const std::string& iface, size_t buffer_size = 2 * 1024 * 1024) {
        // Create PACKET_MMAP socket
        socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        
        // Set up ring buffer
        struct tpacket_req req;
        req.tp_block_size = getpagesize();
        req.tp_block_nr = buffer_size / req.tp_block_size;
        req.tp_frame_size = 2048;  // 2KB frames
        req.tp_frame_nr = (req.tp_block_size * req.tp_block_nr) / req.tp_frame_size;
        
        setsockopt(socket_fd, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req));
        
        // Map ring buffer
        ring_size = req.tp_block_size * req.tp_block_nr;
        ring_buffer = mmap(NULL, ring_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, socket_fd, 0);
    }
    
    void processPackets() {
        struct tpacket_hdr* header = (struct tpacket_hdr*)ring_buffer;
        
        while(true) {
            if(header->tp_status & TP_STATUS_USER) {
                uint8_t* packet_data = (uint8_t*)header + header->tp_mac;
                size_t packet_len = header->tp_len;
                
                // Process packet without copying
                processPacket(packet_data, packet_len);
                
                header->tp_status = TP_STATUS_KERNEL;
            }
            
            header = (struct tpacket_hdr*)((uint8_t*)header + req.tp_frame_size);
            if((uint8_t*)header >= (uint8_t*)ring_buffer + ring_size) {
                header = (struct tpacket_hdr*)ring_buffer;
            }
        }
    }
};
```

## Troubleshooting Guide

### Common Issues

1. **Link is down**
   - Check cable connection
   - Verify switch/router port is active
   - Check LED indicators

2. **Slow performance**
   - Check for duplex mismatch
   - Verify cable quality (CAT5e or better for Gigabit)
   - Check for CRC errors: `ethtool -S eth0`

3. **Packet loss**
   - Increase socket buffer size
   - Check for network congestion
   - Verify hardware checksum offload

### Debug Commands
```bash
# Check interface status
ip link show eth0
ethtool eth0

# Monitor network traffic
tcpdump -i eth0 -n
iftop -i eth0

# Check statistics
netstat -i
ethtool -S eth0

# Test connectivity
ping -c 4 google.com
iperf3 -c server_ip

# Debug PHY
ethtool -d eth0
mii-tool eth0
```

## Best Practices

1. **Use jumbo frames** (9000 MTU) for high-throughput applications
2. **Enable hardware checksum offload** to reduce CPU load
3. **Use TCP_NODELAY** for low-latency applications
4. **Increase socket buffer sizes** for high-bandwidth transfers
5. **Use separate cores** for network processing
6. **Implement connection pooling** for frequent connections
7. **Use zero-copy techniques** for bulk data transfer
8. **Monitor error counters** to detect problems early

## Industrial Applications

### Modbus TCP Master
```cpp
class ModbusTCPMaster {
private:
    TCPClient tcp;
    uint16_t transaction_id;
    uint8_t unit_id;
    
    uint16_t calculateCRC(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for(size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for(int j = 0; j < 8; j++) {
                if(crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
    
public:
    ModbusTCPMaster(uint8_t unit = 1) : unit_id(unit), transaction_id(0) {}
    
    bool connect(const std::string& host, uint16_t port = 502) {
        return tcp.connect(host, port);
    }
    
    std::vector<uint16_t> readHoldingRegisters(uint16_t address, uint16_t count) {
        uint8_t request[12];
        uint8_t response[256];
        
        // MBAP header
        transaction_id++;
        request[0] = (transaction_id >> 8) & 0xFF;
        request[1] = transaction_id & 0xFF;
        request[2] = 0x00;  // Protocol ID
        request[3] = 0x00;
        request[4] = 0x00;  // Length (will fill later)
        request[5] = 0x06;  // Length = 6 bytes + unit + function
        request[6] = unit_id;
        request[7] = 0x03;  // Read holding registers
        request[8] = (address >> 8) & 0xFF;
        request[9] = address & 0xFF;
        request[10] = (count >> 8) & 0xFF;
        request[11] = count & 0xFF;
        
        // Set length in MBAP
        request[4] = 0x00;
        request[5] = 0x06;
        
        tcp.send(request, 12);
        
        ssize_t bytes = tcp.receive(response, sizeof(response), 1000);
        if(bytes < 9) return {};
        
        std::vector<uint16_t> values;
        int data_len = response[8];
        for(int i = 0; i < data_len / 2; i++) {
            uint16_t value = (response[9 + 2*i] << 8) | response[10 + 2*i];
            values.push_back(value);
        }
        
        return values;
    }
    
    void disconnect() {
        tcp.disconnect();
    }
};
```

