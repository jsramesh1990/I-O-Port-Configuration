#ifndef JETSON_CAN_HPP
#define JETSON_CAN_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

class CAN {
public:
    struct CANFrame {
        uint32_t id;
        uint8_t data[8];
        uint8_t len;
        bool extended;      // 29-bit ID
        bool rtr;           // Remote transmission request
        bool error;         // Error frame
        
        std::string toString() const;
    };
    
    struct Config {
        int baudrate = 500000;      // 125k, 250k, 500k, 1M
        bool sample_point = true;   // true=87.5%, false=75%
        bool sjw = false;            // Synchronization jump width
        bool listen_only = false;
        bool loopback = false;
        bool triple_sampling = false;
        bool one_shot = false;       // No retransmission
    };
    
    // Constructor/Destructor
    CAN(const std::string& interface = "can0");
    ~CAN();
    
    // Interface operations
    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }
    bool setConfig(const Config& config);
    Config getConfig() const;
    bool setBitTiming(int baudrate);
    
    // Frame operations
    bool sendFrame(const CANFrame& frame);
    bool receiveFrame(CANFrame& frame, int timeout_ms = 100);
    bool sendRemoteFrame(uint32_t id, bool extended = false);
    
    // Filter configuration
    struct Filter {
        uint32_t id;
        uint32_t mask;
        bool extended;
    };
    
    bool setFilter(const Filter& filter);
    bool setFilters(const std::vector<Filter>& filters);
    void clearFilters();
    
    // Error handling and statistics
    struct Statistics {
        uint32_t tx_frames = 0;
        uint32_t rx_frames = 0;
        uint32_t tx_errors = 0;
        uint32_t rx_errors = 0;
        uint32_t rx_overruns = 0;
        uint32_t arbitration_lost = 0;
        uint32_t bus_errors = 0;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Bus status
    enum class BusStatus {
        ACTIVE,
        PASSIVE,
        BUS_OFF
    };
    
    BusStatus getBusStatus();
    bool recoverBus();
    bool isBusOff() const;
    
    // Callbacks
    using FrameCallback = std::function<void(const CANFrame&)>;
    void startAsyncReceive(FrameCallback callback);
    void stopAsyncReceive();
    
    // CANOpen support
    class CANOpen {
    public:
        CANOpen(CAN& can, uint8_t node_id);
        
        bool sendHeartbeat(uint8_t status);
        bool sendSDO(uint16_t index, uint8_t subindex, uint32_t data, uint8_t size);
        bool receiveSDO(uint16_t index, uint8_t subindex, uint32_t& data);
        bool sendPDO(uint16_t cob_id, const uint8_t* data, uint8_t len);
        
    private:
        CAN& can_;
        uint8_t node_id_;
    };
    
    // J1939 support (heavy vehicle)
    class J1939 {
    public:
        J1939(CAN& can, uint8_t source_address = 0x00);
        
        bool sendPGN(uint32_t pgn, const uint8_t* data, uint8_t len, uint8_t priority = 3);
        bool receivePGN(uint32_t pgn, uint8_t* data, uint8_t& len, int timeout_ms = 100);
        bool requestPGN(uint32_t pgn, uint8_t destination = 0xFF);
        
        // Common PGNs
        static constexpr uint32_t PGN_EEC1 = 0xF004;    // Engine speed
        static constexpr uint32_t PGN_VW = 0xFEF1;      // Vehicle speed
        static constexpr uint32_t PGN_FC = 0xFEE8;      // Fuel consumption
        
    private:
        CAN& can_;
        uint8_t source_addr_;
    };
    
    // Utility
    std::string getInterface() const { return interface_; }
    static std::vector<std::string> getAvailableInterfaces();
    static std::string errorToString(int error);
    
private:
    std::string interface_;
    int fd_;
    Config config_;
    Statistics stats_;
    std::thread async_thread_;
    std::atomic<bool> async_running_;
    FrameCallback async_callback_;
    struct sockaddr_can addr_;
    struct ifreq ifr_;
    
    void setupSocket();
    void asyncReceiveLoop();
    void parseCANFrame(const struct can_frame& frame, CANFrame& out);
    void fillCANFrame(const CANFrame& frame, struct can_frame& out);
};

#endif // JETSON_CAN_HPP
