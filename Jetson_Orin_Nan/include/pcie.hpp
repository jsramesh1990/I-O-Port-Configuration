#ifndef JETSON_PCIE_HPP
#define JETSON_PCIE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

class PCIe {
public:
    struct DeviceInfo {
        uint16_t domain;
        uint8_t bus;
        uint8_t slot;
        uint8_t function;
        uint16_t vendor_id;
        uint16_t device_id;
        uint8_t class_code;
        uint8_t subclass;
        uint8_t prog_if;
        uint8_t revision_id;
        uint32_t bar[6];
        uint8_t irq_line;
        uint8_t irq_pin;
        uint32_t link_speed;    // GT/s
        uint32_t link_width;    // lanes
        std::string driver;
        
        std::string toString() const;
    };
    
    struct BARInfo {
        int index;
        uint64_t address;
        size_t size;
        bool prefetchable;
        bool is64bit;
        int type;  // 0=memory, 1=I/O
    };
    
    // Constructor/Destructor
    PCIe();
    ~PCIe();
    
    // Device enumeration
    std::vector<DeviceInfo> scanDevices();
    bool findDevice(uint16_t vendor_id, uint16_t device_id, DeviceInfo& device);
    bool findDeviceByClass(uint8_t class_code, uint8_t subclass, std::vector<DeviceInfo>& devices);
    
    // Device configuration
    bool enableDevice(const DeviceInfo& device);
    bool disableDevice(const DeviceInfo& device);
    bool resetDevice(const DeviceInfo& device);
    bool setPowerState(const DeviceInfo& device, int state);  // D0, D1, D2, D3hot
    
    // BAR operations
    std::vector<BARInfo> getBARs(const DeviceInfo& device);
    void* mapBAR(const DeviceInfo& device, int bar_index);
    void unmapBAR(void* mapped_addr, size_t size);
    uint32_t readConfig(const DeviceInfo& device, uint8_t offset, uint8_t size);
    bool writeConfig(const DeviceInfo& device, uint8_t offset, uint32_t value, uint8_t size);
    
    // DMA operations
    class DMAEngine {
    public:
        DMAEngine();
        ~DMAEngine();
        
        void* allocBuffer(size_t size);
        void freeBuffer(void* buffer);
        bool transferToDevice(void* buffer, uint64_t device_addr, size_t size);
        bool transferFromDevice(uint64_t device_addr, void* buffer, size_t size);
        
    private:
        int dma_fd_;
        void* dma_buf_;
    };
    
    // MSI/MSI-X interrupts
    bool setupMSI(const DeviceInfo& device, int num_vectors);
    bool setupMSIX(const DeviceInfo& device, int num_vectors);
    bool disableMSI(const DeviceInfo& device);
    
    // Hotplug
    class HotplugMonitor {
    public:
        HotplugMonitor();
        ~HotplugMonitor();
        
        void start(std::function<void(const DeviceInfo&, bool connected)> callback);
        void stop();
        
    private:
        std::thread monitor_thread_;
        std::atomic<bool> running_;
        std::function<void(const DeviceInfo&, bool)> callback_;
    };
    
    // Utility
    static std::string vendorToString(uint16_t vendor_id);
    static std::string deviceToString(uint16_t vendor_id, uint16_t device_id);
    static std::string classToString(uint8_t class_code, uint8_t subclass);
    static bool isNVMeDevice(const DeviceInfo& device);
    static bool isGPUDevice(const DeviceInfo& device);
    
    // NVMe specific
    class NVMeDevice {
    public:
        NVMeDevice(const DeviceInfo& device);
        ~NVMeDevice();
        
        bool identify();
        bool readSectors(uint64_t lba, uint32_t count, void* buffer);
        bool writeSectors(uint64_t lba, uint32_t count, void* buffer);
        uint64_t getCapacity() const;  // in bytes
        std::string getModel() const;
        std::string getSerial() const;
        
    private:
        std::unique_ptr<DMAEngine> dma_;
        DeviceInfo device_;
        void* identify_buffer_;
    };
    
private:
    std::vector<DeviceInfo> devices_;
    void scanBus(uint8_t bus);
    uint32_t readPCIConfig(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint8_t size);
    void writePCIConfig(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value, uint8_t size);
};

#endif // JETSON_PCIE_HPP
