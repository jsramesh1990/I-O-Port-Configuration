#ifndef JETSON_USB_HPP
#define JETSON_USB_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

class USB {
public:
    struct DeviceInfo {
        uint16_t vendor_id;
        uint16_t product_id;
        std::string manufacturer;
        std::string product;
        std::string serial_number;
        std::string device_path;
        std::string bus_number;
        std::string device_number;
        int speed;  // 1=1.5M, 2=12M, 3=480M, 4=5000M
        int max_power;  // mA
        int num_interfaces;
        
        std::string toString() const;
    };
    
    struct InterfaceInfo {
        int interface_number;
        int class_code;
        int subclass_code;
        int protocol;
        std::string driver;
    };
    
    // Constructor/Destructor
    USB();
    ~USB();
    
    // Device enumeration
    std::vector<DeviceInfo> getDevices();
    bool findDevice(uint16_t vid, uint16_t pid, DeviceInfo& device);
    bool findDeviceBySerial(const std::string& serial, DeviceInfo& device);
    
    // Device operations
    bool resetDevice(const DeviceInfo& device);
    bool setPowerSave(const DeviceInfo& device, bool enable);
    bool claimInterface(const DeviceInfo& device, int interface_num);
    bool releaseInterface(const DeviceInfo& device, int interface_num);
    
    // USB Serial
    class SerialPort {
    public:
        SerialPort(const std::string& device_path);
        ~SerialPort();
        
        bool open(int baudrate = 115200);
        void close();
        ssize_t write(const uint8_t* data, size_t len);
        ssize_t read(uint8_t* buffer, size_t max_len, int timeout_ms = 100);
        bool isOpen() const;
        
    private:
        std::string device_path_;
        int fd_;
    };
    
    // USB Mass Storage
    class MassStorage {
    public:
        MassStorage(const DeviceInfo& device);
        ~MassStorage();
        
        bool mount(const std::string& mount_point = "/mnt/usb");
        void unmount();
        bool copyFile(const std::string& source, const std::string& dest);
        std::vector<std::string> listFiles();
        bool isMounted() const { return mounted_; }
        
    private:
        std::string device_node_;
        std::string mount_point_;
        bool mounted_;
        
        std::string findDeviceNode(const DeviceInfo& device);
    };
    
    // USB HID
    class HIDDevice {
    public:
        HIDDevice(const std::string& device_path);
        ~HIDDevice();
        
        bool open();
        void close();
        ssize_t writeReport(const uint8_t* data, size_t len);
        ssize_t readReport(uint8_t* buffer, size_t max_len, int timeout_ms = 100);
        std::string getManufacturer();
        std::string getProduct();
        
    private:
        std::string device_path_;
        int fd_;
    };
    
    // USB Webcam
    class Webcam {
    public:
        Webcam(const std::string& device_path);
        ~Webcam();
        
        bool open(int width, int height, int fps = 30);
        void close();
        bool captureFrame(uint8_t** data, size_t* size);
        bool startStreaming(std::function<void(uint8_t*, size_t)> callback);
        void stopStreaming();
        
    private:
        std::string device_path_;
        int fd_;
        void* buffers_[4];
        std::thread stream_thread_;
        std::atomic<bool> streaming_;
    };
    
    // Utility
    static std::string speedToString(int speed);
    static std::string vidToString(uint16_t vid);
    static std::string pidToString(uint16_t vid, uint16_t pid);
    
private:
    std::vector<DeviceInfo> cached_devices_;
    void scanDevices();
    std::string readSysfs(const std::string& path);
};

#endif // JETSON_USB_HPP
