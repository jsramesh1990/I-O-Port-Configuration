#ifndef JETSON_ETHERNET_HPP
#define JETSON_ETHERNET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

class Ethernet {
public:
    struct MACAddress {
        uint8_t bytes[6];
        
        std::string toString() const;
        bool fromString(const std::string& str);
        bool isBroadcast() const;
        bool isMulticast() const;
        bool isUnicast() const;
    };
    
    struct IPAddress {
        uint32_t addr;
        
        std::string toString() const;
        bool fromString(const std::string& str);
        bool isPrivate() const;
    };
    
    struct LinkStats {
        uint64_t rx_bytes;
        uint64_t tx_bytes;
        uint64_t rx_packets;
        uint64_t tx_packets;
        uint64_t rx_errors;
        uint64_t tx_errors;
        uint64_t rx_dropped;
        uint64_t tx_dropped;
        uint64_t rx_multicast;
        uint32_t link_speed;      // Mbps
        std::string duplex;       // "full" or "half"
    };
    
    // Constructor/Destructor
    Ethernet(const std::string& interface = "eth0");
    ~Ethernet();
    
    // Interface configuration
    bool setMACAddress(const MACAddress& mac);
    MACAddress getMACAddress() const;
    bool setIPAddress(const IPAddress& ip, const IPAddress& netmask);
    bool setGateway(const IPAddress& gateway);
    bool setDNS(const std::vector<IPAddress>& dns_servers);
    bool setMTU(int mtu);
    int getMTU() const;
    bool setLinkUp(bool up);
    bool isLinkUp() const;
    
    // Statistics
    LinkStats getStats() const;
    void printStats() const;
    
    // PHY configuration
    bool setAutoNegotiation(bool enable);
    bool setSpeed(int speed_mbps);  // 10, 100, 1000
    bool setDuplex(const std::string& duplex);  // "half", "full"
    bool setWakeOnLAN(bool enable);
    
    // Raw socket operations
    class RawSocket {
    public:
        RawSocket(const std::string& interface);
        ~RawSocket();
        
        bool send(const uint8_t* data, size_t len);
        ssize_t receive(uint8_t* buffer, size_t max_len, int timeout_ms = 100);
        void setPromiscuous(bool enable);
        
    private:
        int fd_;
        std::string interface_;
    };
    
    // TCP/UDP sockets
    class TCPSocket {
    public:
        TCPSocket();
        ~TCPSocket();
        
        bool connect(const IPAddress& ip, uint16_t port, int timeout_ms = 5000);
        void disconnect();
        ssize_t send(const uint8_t* data, size_t len);
        ssize_t receive(uint8_t* buffer, size_t max_len, int timeout_ms = 5000);
        bool isConnected() const;
        
    private:
        int fd_;
        bool connected_;
    };
    
    class UDPSocket {
    public:
        UDPSocket();
        ~UDPSocket();
        
        bool bind(uint16_t port, const IPAddress& ip = IPAddress{INADDR_ANY});
        ssize_t sendTo(const uint8_t* data, size_t len, const IPAddress& ip, uint16_t port);
        ssize_t receiveFrom(uint8_t* buffer, size_t max_len, IPAddress* ip = nullptr, uint16_t* port = nullptr);
        void setBroadcast(bool enable);
        void joinMulticast(const IPAddress& multicast_ip);
        
    private:
        int fd_;
    };
    
    // HTTP client
    class HTTPClient {
    public:
        HTTPClient();
        ~HTTPClient();
        
        bool connect(const std::string& host, uint16_t port = 80);
        std::string get(const std::string& path);
        std::string post(const std::string& path, const std::string& data,
                        const std::string& content_type = "application/x-www-form-urlencoded");
        
    private:
        std::unique_ptr<TCPSocket> socket_;
        std::string host_;
    };
    
    // Utility
    std::string getInterface() const { return interface_; }
    static std::vector<std::string> getAvailableInterfaces();
    
private:
    std::string interface_;
    int socket_fd_;
    
    bool ioctlCommand(unsigned long request, struct ifreq& ifr);
};

#endif // JETSON_ETHERNET_HPP
