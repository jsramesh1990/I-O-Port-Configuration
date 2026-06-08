
## docs/can.md

```markdown
# CAN (Controller Area Network)

## Overview

The Jetson Orin Nano does not have built-in CAN controllers but supports external CAN controllers via SPI (MCP2515) or USB (CANable).

## Hardware Specifications (MCP2515)

| Parameter | Value |
|-----------|-------|
| Controller | MCP2515 (SPI) |
| Max Speed | 1 Mbps |
| Standard | CAN 2.0A (11-bit) and 2.0B (29-bit) |
| Buffers | 3 TX, 2 RX |
| Filters | 6 acceptance filters |
| Masks | 2 acceptance masks |
| SPI Speed | Up to 10 MHz |
| Interrupt | Yes (INT pin) |

## Pin Configuration

### MCP2515 Connection
```
Jetson Orin Nano         MCP2515
┌─────────────┐         ┌─────────┐
│ SPI1_MOSI   ├────────►│ SI      │
│ (Pin 19)    │         │         │
│ SPI1_MISO   │◄────────┤ SO      │
│ (Pin 21)    │         │         │
│ SPI1_SCK    ├────────►│ SCK     │
│ (Pin 23)    │         │         │
│ SPI1_CS0    ├────────►│ CS      │
│ (Pin 24)    │         │         │
│ GPIO17      ├────────►│ INT     │
│ (Pin 11)    │         │         │
└─────────────┘         └─────────┘
                              │
                              ├─► CAN_H
                              ├─► CAN_L
                              │
                            ┌─┴─┐
                            │120Ω│ Termination
                            └───┘
```

## Implementation

### MCP2515 Driver
```cpp
class MCP2515 {
private:
    SPI spi;
    GPIO interrupt_pin;
    
    // Registers
    static constexpr uint8_t CANSTAT = 0x0E;
    static constexpr uint8_t CANCTRL = 0x0F;
    static constexpr uint8_t BFPCTRL = 0x0C;
    static constexpr uint8_t TXRTSCTRL = 0x0D;
    
    // Receive buffers
    static constexpr uint8_t RXB0CTRL = 0x60;
    static constexpr uint8_t RXB1CTRL = 0x70;
    
    // Transmit buffers
    static constexpr uint8_t TXB0CTRL = 0x30;
    static constexpr uint8_t TXB1CTRL = 0x40;
    static constexpr uint8_t TXB2CTRL = 0x50;
    
    // Filters and masks
    static constexpr uint8_t RXF0SIDH = 0x00;
    static constexpr uint8_t RXF1SIDH = 0x04;
    static constexpr uint8_t RXF2SIDH = 0x08;
    static constexpr uint8_t RXF3SIDH = 0x10;
    static constexpr uint8_t RXF4SIDH = 0x14;
    static constexpr uint8_t RXF5SIDH = 0x18;
    static constexpr uint8_t RXM0SIDH = 0x20;
    static constexpr uint8_t RXM1SIDH = 0x24;
    
    uint8_t readRegister(uint8_t address) {
        uint8_t tx[2] = {0x03, address};  // Read command
        uint8_t rx[2];
        spi.transfer(tx, rx, 2);
        return rx[1];
    }
    
    void writeRegister(uint8_t address, uint8_t value) {
        uint8_t tx[3] = {0x02, address, value};  // Write command
        spi.write(tx, 3);
    }
    
    void modifyRegister(uint8_t address, uint8_t mask, uint8_t value) {
        uint8_t tx[4] = {0x05, address, mask, value};  // Bit modify
        spi.write(tx, 4);
    }
    
public:
    struct CANFrame {
        uint32_t id;
        uint8_t data[8];
        uint8_t len;
        bool extended;
        bool rtr;  // Remote transmission request
    };
    
    MCP2515(const std::string& spi_dev, unsigned int int_pin) 
        : spi(spi_dev, 10000000), interrupt_pin(int_pin, GPIO::Direction::INPUT) {
        
        // Reset device
        uint8_t reset_cmd = 0xC0;
        spi.write(&reset_cmd, 1);
        usleep(10000);
        
        // Configure
        setMode(Mode::CONFIG);
        
        // Set bit timing for 500kbps (16MHz crystal)
        writeRegister(0x2A, 0x00);  // CNF1: 1TQ
        writeRegister(0x29, 0xF0);  // CNF2: Phase segment 1 = 3TQ
        writeRegister(0x28, 0x86);  // CNF3: Phase segment 2 = 3TQ
        
        // Enable rollover and interrupts
        writeRegister(RXB0CTRL, 0x60);
        writeRegister(RXB1CTRL, 0x60);
        writeRegister(CANCTRL, 0x07);  // Normal mode
        
        setMode(Mode::NORMAL);
    }
    
    enum class Mode {
        NORMAL = 0x00,
        SLEEP = 0x20,
        LOOPBACK = 0x40,
        LISTEN_ONLY = 0x60,
        CONFIG = 0x80
    };
    
    void setMode(Mode mode) {
        modifyRegister(CANCTRL, 0xE0, (uint8_t)mode);
        while((readRegister(CANSTAT) & 0xE0) != (uint8_t)mode) {
            usleep(100);
        }
    }
    
    bool sendFrame(const CANFrame& frame) {
        // Find available TX buffer
        uint8_t txbctrl[] = {TXB0CTRL, TXB1CTRL, TXB2CTRL};
        int tx_buffer = -1;
        
        for(int i = 0; i < 3; i++) {
            if(!(readRegister(txbctrl[i]) & 0x08)) {
                tx_buffer = i;
                break;
            }
        }
        
        if(tx_buffer < 0) return false;
        
        uint8_t base = TXB0CTRL + tx_buffer * 0x10;
        
        // Write ID
        if(frame.extended) {
            uint32_t id = frame.id;
            writeRegister(base + 0, (id >> 21) & 0xFF);
            writeRegister(base + 1, ((id >> 13) & 0xE0) | ((id >> 16) & 0x03));
            writeRegister(base + 2, (id >> 8) & 0xFF);
            writeRegister(base + 3, id & 0xFF);
            writeRegister(base + 4, (frame.rtr ? 0x40 : 0x00) | frame.len);
        } else {
            uint16_t id = frame.id;
            writeRegister(base + 0, (id >> 3) & 0xFF);
            writeRegister(base + 1, (id << 5) & 0xE0);
            writeRegister(base + 4, (frame.rtr ? 0x40 : 0x00) | frame.len);
        }
        
        // Write data
        for(int i = 0; i < frame.len && i < 8; i++) {
            writeRegister(base + 5 + i, frame.data[i]);
        }
        
        // Request transmission
        uint8_t txrtsctrl = readRegister(TXRTSCTRL);
        txrtsctrl |= (1 << tx_buffer);
        writeRegister(TXRTSCTRL, txrtsctrl);
        
        return true;
    }
    
    bool receiveFrame(CANFrame& frame, int timeout_ms = 100) {
        // Check for received message
        uint8_t rx0if = (readRegister(CANSTAT) & 0x40) ? 0 : 1;
        uint8_t rxctrl = readRegister(rx0if ? RXB0CTRL : RXB1CTRL);
        
        if(!(rxctrl & 0x01)) {
            // No message
            return false;
        }
        
        uint8_t base = rx0if ? RXB0CTRL : RXB1CTRL;
        
        // Read ID
        uint8_t sidh = readRegister(base + 0);
        uint8_t sidl = readRegister(base + 1);
        
        if(sidl & 0x08) {  // Extended ID
            frame.extended = true;
            uint8_t eid8 = readRegister(base + 2);
            uint8_t eid0 = readRegister(base + 3);
            frame.id = ((sidh & 0xFF) << 21) |
                       (((sidl & 0xE0) >> 5) << 16) |
                       ((eid8 & 0xFF) << 8) |
                       (eid0 & 0xFF);
        } else {
            frame.extended = false;
            frame.id = ((sidh & 0xFF) << 3) | ((sidl & 0xE0) >> 5);
        }
        
        // Read DLC
        uint8_t dlc = readRegister(base + 4);
        frame.rtr = dlc & 0x40;
        frame.len = dlc & 0x0F;
        
        // Read data
        for(int i = 0; i < frame.len; i++) {
            frame.data[i] = readRegister(base + 5 + i);
        }
        
        // Clear interrupt
        modifyRegister(CANSTAT, 0x40, 0x00);
        
        return true;
    }
    
    void setFilter(uint8_t filter, uint32_t id, bool extended = false) {
        setMode(Mode::CONFIG);
        
        uint8_t base = RXF0SIDH + filter * 4;
        
        if(extended) {
            writeRegister(base + 0, (id >> 21) & 0xFF);
            writeRegister(base + 1, ((id >> 13) & 0xE0) | ((id >> 16) & 0x03) | 0x08);
            writeRegister(base + 2, (id >> 8) & 0xFF);
            writeRegister(base + 3, id & 0xFF);
        } else {
            writeRegister(base + 0, (id >> 3) & 0xFF);
            writeRegister(base + 1, (id << 5) & 0xE0);
            writeRegister(base + 2, 0);
            writeRegister(base + 3, 0);
        }
        
        setMode(Mode::NORMAL);
    }
    
    void setMask(uint8_t mask, uint32_t mask_val, bool extended = false) {
        setMode(Mode::CONFIG);
        
        uint8_t base = RXM0SIDH + mask * 4;
        
        if(extended) {
            writeRegister(base + 0, (mask_val >> 21) & 0xFF);
            writeRegister(base + 1, ((mask_val >> 13) & 0xE0) | 
                         ((mask_val >> 16) & 0x03) | 0x08);
            writeRegister(base + 2, (mask_val >> 8) & 0xFF);
            writeRegister(base + 3, mask_val & 0xFF);
        } else {
            writeRegister(base + 0, (mask_val >> 3) & 0xFF);
            writeRegister(base + 1, (mask_val << 5) & 0xE0);
            writeRegister(base + 2, 0);
            writeRegister(base + 3, 0);
        }
        
        setMode(Mode::NORMAL);
    }
    
    bool isInterruptPending() {
        return interrupt_pin.read() == GPIO::Value::HIGH;
    }
};
```

### CANOpen Implementation
```cpp
class CANOpenNode {
private:
    MCP2515 can;
    uint8_t node_id;
    
    struct COBID {
        static constexpr uint16_t NMT = 0x000;
        static constexpr uint16_t SYNC = 0x080;
        static constexpr uint16_t EMERGENCY = 0x080;
        static constexpr uint16_t PDO1_TX = 0x180;
        static constexpr uint16_t PDO1_RX = 0x200;
        static constexpr uint16_t PDO2_TX = 0x280;
        static constexpr uint16_t PDO2_RX = 0x300;
        static constexpr uint16_t PDO3_TX = 0x380;
        static constexpr uint16_t PDO3_RX = 0x400;
        static constexpr uint16_t PDO4_TX = 0x480;
        static constexpr uint16_t PDO4_RX = 0x500;
        static constexpr uint16_t SDO_TX = 0x580;
        static constexpr uint16_t SDO_RX = 0x600;
        static constexpr uint16_t HEARTBEAT = 0x700;
    };
    
    struct ObjectDictionary {
        // Communication objects (Index 0x1000-0x1FFF)
        uint32_t device_type;
        uint32_t error_register;
        uint32_t predefine_error;
        
        // Manufacturer objects (Index 0x2000-0x5FFF)
        // User-specific
        
        // Application objects (Index 0x6000-0x9FFF)
        // User-specific
    } od;
    
public:
    CANOpenNode(const std::string& spi_dev, unsigned int int_pin, uint8_t id)
        : can(spi_dev, int_pin), node_id(id) {}
    
    void sendHeartbeat(uint8_t status) {
        MCP2515::CANFrame frame;
        frame.id = COBID::HEARTBEAT + node_id;
        frame.len = 1;
        frame.data[0] = status;
        can.sendFrame(frame);
    }
    
    bool writeSDO(uint16_t index, uint8_t subindex, uint32_t data, uint8_t size) {
        MCP2515::CANFrame frame;
        frame.id = COBID::SDO_TX + node_id;
        frame.len = 8;
        
        // SDO command specifier
        frame.data[0] = (size == 1 ? 0x2F : size == 2 ? 0x2B : size == 4 ? 0x23 : 0x2F);
        frame.data[1] = index & 0xFF;
        frame.data[2] = (index >> 8) & 0xFF;
        frame.data[3] = subindex;
        
        // Data bytes
        for(int i = 0; i < size; i++) {
            frame.data[4 + i] = (data >> (8 * i)) & 0xFF;
        }
        
        return can.sendFrame(frame);
    }
    
    bool readSDO(uint16_t index, uint8_t subindex, uint32_t& data) {
        MCP2515::CANFrame frame;
        frame.id = COBID::SDO_TX + node_id;
        frame.len = 8;
        
        frame.data[0] = 0x40;  // Read request
        frame.data[1] = index & 0xFF;
        frame.data[2] = (index >> 8) & 0xFF;
        frame.data[3] = subindex;
        
        if(!can.sendFrame(frame)) return false;
        
        // Wait for response
        MCP2515::CANFrame response;
        auto start = std::chrono::steady_clock::now();
        
        while(true) {
            if(can.receiveFrame(response)) {
                if((response.id & 0x780) == COBID::SDO_RX) {
                    data = response.data[4] |
                           (response.data[5] << 8) |
                           (response.data[6] << 16) |
                           (response.data[7] << 24);
                    return true;
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 100) return false;
            
            usleep(1000);
        }
    }
};
```

## Performance Benchmark

### CAN Bus Test
```cpp
class CANBenchmark {
private:
    MCP2515 can;
    std::chrono::steady_clock::time_point start;
    
public:
    CANBenchmark(const std::string& spi_dev, unsigned int int_pin)
        : can(spi_dev, int_pin) {}
    
    double testThroughput() {
        MCP2515::CANFrame frame;
        frame.id = 0x100;
        frame.len = 8;
        for(int i = 0; i < 8; i++) frame.data[i] = i;
        
        start = std::chrono::steady_clock::now();
        int count = 0;
        
        while(count < 10000) {
            if(can.sendFrame(frame)) {
                count++;
            }
        }
        
        auto end = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        
        return count / elapsed;  // Frames per second
    }
    
    double testLatency() {
        std::vector<double> latencies;
        
        for(int i = 0; i < 1000; i++) {
            MCP2515::CANFrame tx_frame;
            tx_frame.id = 0x100;
            tx_frame.len = 1;
            tx_frame.data[0] = i;
            
            auto send_time = std::chrono::steady_clock::now();
            can.sendFrame(tx_frame);
            
            MCP2515::CANFrame rx_frame;
            while(!can.receiveFrame(rx_frame)) {
                usleep(10);
            }
            
            auto receive_time = std::chrono::steady_clock::now();
            double latency = std::chrono::duration<double, std::micro>(receive_time - send_time).count();
            latencies.push_back(latency);
        }
        
        // Calculate statistics
        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        double mean = sum / latencies.size();
        
        double sq_sum = std::inner_product(latencies.begin(), latencies.end(),
                                          latencies.begin(), 0.0);
        double stddev = std::sqrt(sq_sum / latencies.size() - mean * mean);
        
        printf("Latency - Mean: %.2f us, StdDev: %.2f us\n", mean, stddev);
        
        return mean;
    }
};
```

## Troubleshooting Guide

### Common Issues

1. **No communication**
   - Check termination resistor (120Ω at both ends)
   - Verify CAN_H and CAN_L polarity
   - Check baudrate matching

2. **Bus errors**
   - Monitor bus load
   - Check for missing termination
   - Verify common ground

3. **Filter issues**
   - Verify acceptance filters configuration
   - Check mask settings
   - Test with listen-only mode

### Debug Commands
```bash
# Install CAN utilities
sudo apt install can-utils

# Configure CAN interface (with slcan for USB adapters)
sudo slcand -o -c -f -s8 /dev/ttyACM0 can0
sudo ip link set can0 up

# Monitor CAN traffic
candump can0

# Send test message
cansend can0 123#1122334455667788

# Check bus statistics
ip -details link show can0
```

## Best Practices

1. **Always terminate both ends** of CAN bus
2. **Use twisted pair cable** for CAN_H/CAN_L
3. **Keep stub lengths short** (< 0.3m)
4. **Implement bus guardians** for critical nodes
5. **Monitor error counters** for preventive maintenance
6. **Use galvanic isolation** for noisy environments
7. **Implement node guarding** or heartbeat protocol
8. **Use CANOpen or J1939** for standardized communication

## Industrial Applications

### J1939 Implementation (Heavy Vehicle)
```cpp
class J1939 {
private:
    MCP2515 can;
    
    struct PGN {
        static constexpr uint32_t EEC1 = 0xF004;   // Engine speed
        static constexpr uint32_t EEC2 = 0xF005;   // Engine torque
        static constexpr uint32_t EEC3 = 0xF00C;   // Engine demand
        static constexpr uint32_t LFC = 0xFEE8;    // Fuel consumption
        static constexpr uint32_t VW = 0xFEF1;     // Vehicle speed
        static constexpr uint32_t TCO1 = 0xFEEE;   // Transmission
    };
    
public:
    J1939(const std::string& spi_dev, unsigned int int_pin)
        : can(spi_dev, int_pin) {}
    
    void sendEngineData(uint16_t rpm, uint8_t torque_percent) {
        uint32_t pgn = PGN::EEC1;
        uint8_t priority = 3;
        uint8_t source = 0x00;
        
        uint32_t id = (priority << 26) | (pgn << 8) | (source);
        
        MCP2515::CANFrame frame;
        frame.id = id;
        frame.extended = true;
        frame.len = 8;
        
        // Engine speed (0.125 rpm/bit)
        uint16_t rpm_scaled = rpm / 8;  // 125 rpm/bit, but J1939 uses 0.125 rpm/bit
        frame.data[0] = rpm_scaled & 0xFF;
        frame.data[1] = (rpm_scaled >> 8) & 0xFF;
        
        // Engine torque
        frame.data[2] = torque_percent;
        
        // Other data...
        frame.data[3] = 0xFF;  // Not available
        frame.data[4] = 0xFF;  // Not available
        frame.data[5] = 0xFF;  // Not available
        frame.data[6] = 0xFF;  // Not available
        frame.data[7] = 0xFF;  // Not available
        
        can.sendFrame(frame);
    }
    
    uint16_t getVehicleSpeed() {
        uint32_t pgn = PGN::VW;
        uint8_t priority = 6;
        uint8_t source = 0x00;
        
        uint32_t id = (priority << 26) | (pgn << 8) | (source);
        
        // Request vehicle speed
        MCP2515::CANFrame request;
        request.id = 0x18EA00F9;  // Request PGN
        request.len = 3;
        request.data[0] = pgn & 0xFF;
        request.data[1] = (pgn >> 8) & 0xFF;
        request.data[2] = (pgn >> 16) & 0xFF;
        can.sendFrame(request);
        
        // Wait for response
        MCP2515::CANFrame response;
        auto start = std::chrono::steady_clock::now();
        
        while(true) {
            if(can.receiveFrame(response)) {
                if(response.id == 0x18FEF100) {  // Vehicle speed PGN response
                    uint16_t speed = response.data[0] | (response.data[1] << 8);
                    return speed / 256;  // km/h (1/256 km/h per bit)
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if(elapsed.count() > 100) return 0;
            
            usleep(1000);
        }
    }
};
