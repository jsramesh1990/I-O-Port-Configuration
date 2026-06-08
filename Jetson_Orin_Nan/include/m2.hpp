#ifndef JETSON_M2_HPP
#define JETSON_M2_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "pcie.hpp"

class M2 {
public:
    enum class KeyType {
        M_KEY,      // NVMe, PCIe x4
        B_KEY,      // USB, audio, PCIe x2
        A_KEY,      // WiFi, PCIe x2
        E_KEY       // WiFi, Bluetooth, PCIe x2, USB
    };
    
    enum class FormFactor {
        SIZE_2242,   // 22x42mm
        SIZE_2260,   // 22x60mm
        SIZE_2280,   // 22x80mm
        SIZE_22110   // 22x110mm
    };
    
    struct DeviceInfo {
        KeyType key_type;
        FormFactor form_factor;
        std::string manufacturer;
        std::string model;
        std::string serial;
        uint64_t capacity;  // bytes
        int pcie_speed;     // GT/s
        int pcie_lanes;     // 1, 2, 4
        int power_consumption;  // mA
        double temperature;     // Celsius
    };
    
    // Constructor/Destructor
    M2();
    ~M2();
    
    // Detection
    bool detectDevice(DeviceInfo& info);
    bool isNVMePresent();
    bool isWiFiPresent();
    bool isCellularPresent();
    
    // NVMe SSD
    class NVMeSSD : public PCIe::NVMeDevice {
    public:
        NVMeSSD();
        ~NVMeSSD();
        
        bool initialize();
        uint64_t getCapacity() const;
        std::string getModel() const;
        std::string getSerial() const;
        int getTemperature() const;  // Celsius
        bool getSMART(uint8_t* data, size_t size);
        
        // Performance
        double readSpeed();   // MB/s
        double writeSpeed();  // MB/s
        
        // Power management
        bool enableASPM();
        bool setPowerState(int state);  // 0=active, 1=idle, 2=standby, 3=sleep
        bool enablePowerSaving();
        
    private:
        PCIe::DeviceInfo pcie_dev_;
        void* nvme_handle_;
    };
    
    // WiFi module (M.2 E-key)
    class WiFiModule {
    public:
        WiFiModule();
        ~WiFiModule();
        
        bool initialize();
        bool scanNetworks(std::vector<std::string>& networks);
        bool connect(const std::string& ssid, const std::string& password);
        void disconnect();
        bool isConnected() const;
        std::string getIPAddress() const;
        int getSignalStrength() const;  // dBm
        
    private:
        std::string interface_;
        bool connected_;
    };
    
    // Cellular module (4G/5G)
    class CellularModule {
    public:
        enum class NetworkType {
            LTE,
            UMTS,
            GSM,
            NR  // 5G
        };
        
        CellularModule();
        ~CellularModule();
        
        bool initialize();
        bool connect();
        void disconnect();
        bool isConnected() const;
        std::string getIPAddress() const;
        int getSignalStrength() const;  // dBm
        NetworkType getNetworkType() const;
        std::string getIMEI() const;
        std::string getICCID() const;
        
        // SMS
        bool sendSMS(const std::string& number, const std::string& message);
        std::vector<std::string> receiveSMS();
        
    private:
        std::string device_;
        int fd_;
        bool connected_;
    };
    
    // Power management
    bool setPowerEnable(bool enable);
    bool setPowerLimit(int watts);
    int getPowerConsumption() const;
    
    // Thermal management
    double getTemperature() const;
    bool enableThermalMonitoring();
    bool setCriticalTemperature(double temp_celsius);
    
    // LED control
    bool setLED(bool enable);
    bool setLEDBlink(int frequency_hz);
    
    // Utility
    static std::string keyTypeToString(KeyType type);
    static std::string formFactorToString(FormFactor factor);
    static std::vector<FormFactor> getSupportedFormFactors();
    
private:
    DeviceInfo info_;
    bool power_enabled_;
    std::unique_ptr<NVMeSSD> nvme_;
    std::unique_ptr<WiFiModule> wifi_;
    std::unique_ptr<CellularModule> cellular_;
    
    bool detectKeyType();
    bool detectFormFactor();
    bool readDeviceInfo();
    bool setupPCIe();
    void monitorTemperature();
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_;
};

#endif // JETSON_M2_HPP
