
## docs/usb.md

```markdown
# USB (Universal Serial Bus)

## Overview

The Jetson Orin Nano provides multiple USB ports supporting USB 3.2 Gen 1 (5 Gbps) and USB 2.0 interfaces for connecting various peripherals.

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| USB 3.2 Gen 1 Ports | 3 (2x Type-A, 1x Type-C) |
| USB 2.0 Ports | 3 (shared with USB 3.0 ports) |
| Max Speed | 5 Gbps (USB 3.2 Gen 1) |
| Power Delivery | Up to 15W per port (5V/3A) |
| Controllers | XHCI compliant |
| DMA Support | Yes |
| OTG Support | Yes (Type-C port) |
| Host/Device Mode | Configurable |

## USB Controller Mapping

| Controller | Type | Ports | Location |
|------------|------|-------|----------|
| XHCI1 | USB 3.2 | 2x Type-A | Back panel |
| XHCI2 | USB 3.2 | 1x Type-C | Back panel |
| EHCI1 | USB 2.0 | All ports | Shared |
| OTG | USB 2.0/3.0 | Type-C | Configurable |

## Pin Configuration (Internal)

### USB 3.0 Type-A Connector
```
Pin 1  VBUS  ─── 5V power (max 900mA)
Pin 2  D-    ─── USB 2.0 differential data-
Pin 3  D+    ─── USB 2.0 differential data+
Pin 4  GND   ─── Ground
Pin 5  SSTX- ─── SuperSpeed transmit-
Pin 6  SSTX+ ─── SuperSpeed transmit+
Pin 7  GND   ─── Ground
Pin 8  SSRX- ─── SuperSpeed receive-
Pin 9  SSRX+ ─── SuperSpeed receive+
```

### USB Type-C Connector
```
Pin A1  GND     Pin B1  GND
Pin A2  SSTXp1  Pin B2  SSRXp1
Pin A3  SSTXn1  Pin B3  SSRXn1
Pin A4  VBUS    Pin B4  VBUS
Pin A5  CC1     Pin B5  CC2
Pin A6  Dp1     Pin B6  Dn1
Pin A7  Dn1     Pin B7  Dp1
Pin A8  SBU1    Pin B8  SBU2
Pin A9  VBUS    Pin B9  VBUS
Pin A10 SSRXn2  Pin B10 SSTXn2
Pin A11 SSRXp2  Pin B11 SSTXp2
Pin A12 GND     Pin B12 GND
```

## Implementation

### USB Device Detection and Management
```cpp
class USBDeviceManager {
private:
    struct USBDevice {
        uint16_t vendor_id;
        uint16_t product_id;
        std::string manufacturer;
        std::string product;
        std::string serial;
        std::string device_path;
        std::string bus_num;
        std::string dev_num;
        int speed;  // 1=1.5Mbps, 2=12Mbps, 3=480Mbps, 4=5000Mbps
    };
    
    std::vector<USBDevice> devices;
    
    std::string readSysfs(const std::string& path) {
        std::ifstream file(path);
        std::string content;
        if(file.is_open()) {
            std::getline(file, content);
            content.erase(content.find_last_not_of(" \n\r\t") + 1);
        }
        return content;
    }
    
    void scanUSBDevices() {
        devices.clear();
        
        DIR* dir = opendir("/sys/bus/usb/devices");
        if(!dir) return;
        
        struct dirent* entry;
        while((entry = readdir(dir)) != NULL) {
            if(entry->d_name[0] == '.') continue;
            
            std::string device_path = "/sys/bus/usb/devices/" + std::string(entry->d_name);
            
            // Check if it's a USB device (has idVendor file)
            std::string vendor_path = device_path + "/idVendor";
            if(access(vendor_path.c_str(), R_OK) == 0) {
                USBDevice dev;
                dev.device_path = device_path;
                dev.vendor_id = std::stoul(readSysfs(vendor_path), nullptr, 16);
                dev.product_id = std::stoul(readSysfs(device_path + "/idProduct"), nullptr, 16);
                dev.manufacturer = readSysfs(device_path + "/manufacturer");
                dev.product = readSysfs(device_path + "/product");
                dev.serial = readSysfs(device_path + "/serial");
                dev.bus_num = readSysfs(device_path + "/busnum");
                dev.dev_num = readSysfs(device_path + "/devnum");
                
                std::string speed_str = readSysfs(device_path + "/speed");
                if(speed_str == "1.5") dev.speed = 1;
                else if(speed_str == "12") dev.speed = 2;
                else if(speed_str == "480") dev.speed = 3;
                else if(speed_str == "5000") dev.speed = 4;
                else dev.speed = 0;
                
                devices.push_back(dev);
            }
        }
        closedir(dir);
    }
    
public:
    std::vector<USBDevice> getDevices() {
        scanUSBDevices();
        return devices;
    }
    
    bool findDevice(uint16_t vid, uint16_t pid, USBDevice& device) {
        scanUSBDevices();
        for(const auto& dev : devices) {
            if(dev.vendor_id == vid && dev.product_id == pid) {
                device = dev;
                return true;
            }
        }
        return false;
    }
    
    void printDeviceInfo() {
        scanUSBDevices();
        printf("=== USB Devices ===\n");
        for(const auto& dev : devices) {
            printf("Vendor: 0x%04X, Product: 0x%04X\n", dev.vendor_id, dev.product_id);
            printf("  Manufacturer: %s\n", dev.manufacturer.c_str());
            printf("  Product: %s\n", dev.product.c_str());
            printf("  Serial: %s\n", dev.serial.c_str());
            printf("  Bus: %s, Device: %s\n", dev.bus_num.c_str(), dev.dev_num.c_str());
            printf("  Speed: %s\n", 
                   dev.speed == 1 ? "1.5 Mbps (Low Speed)" :
                   dev.speed == 2 ? "12 Mbps (Full Speed)" :
                   dev.speed == 3 ? "480 Mbps (High Speed)" :
                   dev.speed == 4 ? "5 Gbps (SuperSpeed)" : "Unknown");
            printf("---\n");
        }
    }
    
    bool resetDevice(const std::string& bus_num, const std::string& dev_num) {
        std::string path = "/sys/bus/usb/devices/" + bus_num + "-" + dev_num + "/authorized";
        std::ofstream file(path);
        if(file.is_open()) {
            file << "0";  // Deauthorize
            file.close();
            usleep(100000);
            
            file.open(path);
            file << "1";  // Reauthorize
            return true;
        }
        return false;
    }
};
```

### USB Serial Communication
```cpp
class USBSerial {
private:
    int fd;
    std::string device_path;
    struct termios tio;
    
public:
    USBSerial(const std::string& device) : device_path(device), fd(-1) {}
    
    bool open(int baudrate = 115200) {
        fd = ::open(device_path.c_str(), O_RDWR | O_NOCTTY);
        if(fd < 0) return false;
        
        memset(&tio, 0, sizeof(tio));
        tio.c_cflag = CS8 | CLOCAL | CREAD;
        tio.c_iflag = IGNPAR;
        tio.c_oflag = 0;
        tio.c_lflag = 0;
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 10;
        
        cfsetospeed(&tio, B115200);
        cfsetispeed(&tio, B115200);
        
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd, TCSANOW, &tio);
        
        return true;
    }
    
    void close() {
        if(fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
    
    ssize_t write(const uint8_t* data, size_t len) {
        if(fd < 0) return -1;
        return ::write(fd, data, len);
    }
    
    ssize_t read(uint8_t* buffer, size_t max_len, int timeout_ms = 100) {
        if(fd < 0) return -1;
        
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int result = select(fd + 1, &fds, NULL, NULL, &timeout);
        if(result > 0) {
            return ::read(fd, buffer, max_len);
        }
        return 0;
    }
    
    bool isOpen() const {
        return fd >= 0;
    }
};
```

### USB Mass Storage Access
```cpp
class USBMassStorage {
private:
    std::string mount_point;
    bool mounted;
    
    std::string getBlockDevice(const std::string& serial) {
        std::string path = "/dev/disk/by-id/usb-*" + serial + "*";
        glob_t glob_result;
        glob(path.c_str(), GLOB_TILDE, NULL, &glob_result);
        
        if(glob_result.gl_pathc > 0) {
            std::string device = std::string(glob_result.gl_pathv[0]);
            globfree(&glob_result);
            return device;
        }
        
        globfree(&glob_result);
        return "";
    }
    
public:
    USBMassStorage() : mounted(false) {}
    
    bool mount(const std::string& serial, const std::string& mount_pt = "/mnt/usb") {
        std::string device = getBlockDevice(serial);
        if(device.empty()) return false;
        
        mount_point = mount_pt;
        
        // Create mount point if it doesn't exist
        mkdir(mount_point.c_str(), 0755);
        
        // Mount the device
        std::string command = "mount " + device + " " + mount_point;
        if(system(command.c_str()) == 0) {
            mounted = true;
            return true;
        }
        
        return false;
    }
    
    void unmount() {
        if(mounted) {
            std::string command = "umount " + mount_point;
            system(command.c_str());
            mounted = false;
        }
    }
    
    bool copyFile(const std::string& source, const std::string& dest) {
        if(!mounted) return false;
        
        std::string full_dest = mount_point + "/" + dest;
        std::ifstream src(source, std::ios::binary);
        std::ofstream dst(full_dest, std::ios::binary);
        
        dst << src.rdbuf();
        
        return !src.fail() && !dst.fail();
    }
    
    std::vector<std::string> listFiles() {
        std::vector<std::string> files;
        if(!mounted) return files;
        
        DIR* dir = opendir(mount_point.c_str());
        if(dir) {
            struct dirent* entry;
            while((entry = readdir(dir)) != NULL) {
                if(entry->d_name[0] != '.') {
                    files.push_back(std::string(entry->d_name));
                }
            }
            closedir(dir);
        }
        
        return files;
    }
};
```

### USB HID Device Communication
```cpp
class USBHIDDevice {
private:
    int fd;
    std::string device_path;
    struct hidraw_report_descriptor report_desc;
    
public:
    USBHIDDevice(const std::string& device) : device_path(device), fd(-1) {}
    
    bool open() {
        fd = ::open(device_path.c_str(), O_RDWR);
        if(fd < 0) return false;
        
        // Get report descriptor
        int res = ioctl(fd, HIDIOCGRDESCSIZE, &report_desc.size);
        if(res < 0) return false;
        
        report_desc.value = (__u8*)malloc(report_desc.size);
        res = ioctl(fd, HIDIOCGRDESC, &report_desc);
        if(res < 0) {
            free(report_desc.value);
            return false;
        }
        
        return true;
    }
    
    void close() {
        if(fd >= 0) {
            ::close(fd);
            fd = -1;
        }
        if(report_desc.value) {
            free(report_desc.value);
            report_desc.value = nullptr;
        }
    }
    
    ssize_t writeReport(const uint8_t* data, size_t len) {
        if(fd < 0) return -1;
        
        struct hidraw_report report;
        report.data = (__u8*)data;
        report.len = len;
        
        return ioctl(fd, HIDIOCWRITEREPORT, &report);
    }
    
    ssize_t readReport(uint8_t* buffer, size_t max_len, int timeout_ms = 100) {
        if(fd < 0) return -1;
        
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int res = select(fd + 1, &fds, NULL, NULL, &timeout);
        if(res > 0) {
            return ::read(fd, buffer, max_len);
        }
        return 0;
    }
    
    std::string getManufacturer() {
        char buf[256];
        if(ioctl(fd, HIDIOCGRAWNAME, buf) >= 0) {
            return std::string(buf);
        }
        return "";
    }
    
    std::string getProduct() {
        char buf[256];
        if(ioctl(fd, HIDIOCGRAWPHYS, buf) >= 0) {
            return std::string(buf);
        }
        return "";
    }
};
```

### USB Power Management
```cpp
class USBPowerManager {
private:
    std::string getPowerPath(const std::string& bus, const std::string& device) {
        return "/sys/bus/usb/devices/" + bus + "-" + device + "/power/";
    }
    
public:
    bool setPowerSave(const std::string& bus, const std::string& device, bool enable) {
        std::string path = getPowerPath(bus, device) + "control";
        std::ofstream file(path);
        if(file.is_open()) {
            file << (enable ? "auto" : "on");
            return true;
        }
        return false;
    }
    
    bool setAutosuspendDelay(const std::string& bus, const std::string& device, int delay_ms) {
        std::string path = getPowerPath(bus, device) + "autosuspend_delay_ms";
        std::ofstream file(path);
        if(file.is_open()) {
            file << delay_ms;
            return true;
        }
        return false;
    }
    
    bool wakeupEnable(const std::string& bus, const std::string& device, bool enable) {
        std::string path = getPowerPath(bus, device) + "wakeup";
        std::ofstream file(path);
        if(file.is_open()) {
            file << (enable ? "enabled" : "disabled");
            return true;
        }
        return false;
    }
    
    int getPowerUsage(const std::string& bus, const std::string& device) {
        std::string path = getPowerPath(bus, device) + "rx_bytes";
        std::ifstream file(path);
        int bytes;
        if(file >> bytes) {
            return bytes;
        }
        return 0;
    }
};
```

### USB Boot Configuration
```cpp
class USBBootConfig {
public:
    enum BootMode {
        NORMAL,
        RECOVERY,
        FORCED_RECOVERY
    };
    
    bool setBootMode(BootMode mode) {
        // Configure USB OTG for boot mode
        std::ofstream file("/sys/kernel/debug/tegra_usb/otg_mode");
        if(!file.is_open()) return false;
        
        switch(mode) {
            case NORMAL:
                file << "host";
                break;
            case RECOVERY:
                file << "device";
                usleep(100000);
                // Trigger recovery mode
                system("echo 1 > /sys/class/tegra_udrm/trigger");
                break;
            case FORCED_RECOVERY:
                // Force recovery by writing to boot ROM
                system("echo 1 > /sys/class/tegra_udrm/force_recovery");
                break;
        }
        
        return true;
    }
    
    bool isRecoveryMode() {
        std::ifstream file("/sys/class/tegra_udrm/recovery_mode");
        int mode;
        if(file >> mode) {
            return mode == 1;
        }
        return false;
    }
};
```

## Performance Optimization

### USB Transfer Optimization
```cpp
class USBOptimizedTransfer {
private:
    int fd;
    void* dma_buffer;
    size_t buffer_size;
    
public:
    USBOptimizedTransfer(const std::string& device, size_t size = 1024*1024) 
        : buffer_size(size) {
        fd = open(device.c_str(), O_RDWR | O_NOCTTY);
        
        // Allocate DMA buffer
        posix_memalign(&dma_buffer, 4096, size);
        
        // Enable DMA for USB transfers
        ioctl(fd, USBDEVFS_DMA_ALIGN, &size);
    }
    
    ~USBOptimizedTransfer() {
        if(fd >= 0) close(fd);
        if(dma_buffer) free(dma_buffer);
    }
    
    ssize_t bulkTransfer(int endpoint, const uint8_t* data, size_t len, int timeout = 5000) {
        struct usbdevfs_bulktransfer bulk;
        bulk.ep = endpoint;
        bulk.len = len;
        bulk.timeout = timeout;
        bulk.data = (void*)data;
        
        return ioctl(fd, USBDEVFS_BULK, &bulk);
    }
    
    void enableStreaming() {
        int streaming = 1;
        ioctl(fd, USBDEVFS_STREAMING, &streaming);
    }
};
```

## Troubleshooting Guide

### Common Issues

1. **Device not detected**
   - Check USB cable connection
   - Verify power supply (some devices need more power)
   - Check dmesg for USB errors

2. **Slow transfer speeds**
   - Ensure USB 3.0 cable for SuperSpeed
   - Check for USB 2.0 hub in between
   - Verify device supports high speed

3. **Device disconnects randomly**
   - Check power delivery
   - Enable USB autosuspend
   - Update USB controller firmware

### Debug Commands
```bash
# List USB devices
lsusb
lsusb -t
usb-devices

# Monitor USB events
udevadm monitor --environment --udev -s usb
dmesg -w | grep USB

# Check USB power
cat /sys/kernel/debug/usb/devices

# Reset USB controller
echo -n "0000:00:14.0" | tee /sys/bus/pci/drivers/xhci_hcd/unbind
echo -n "0000:00:14.0" | tee /sys/bus/pci/drivers/xhci_hcd/bind
```

## Best Practices

1. **Use powered USB hubs** for multiple high-power devices
2. **Implement device reconnection logic** for reliable operation
3. **Use DMA transfers** for high-speed data
4. **Monitor USB errors** in system logs
5. **Use USB 3.0 cables** for SuperSpeed connections
6. **Implement proper device synchronization** for shared devices
7. **Use isochronous transfers** for streaming data
8. **Enable power management** for battery-powered applications

## Industrial Applications

### USB Camera Capture
```cpp
class USBCameraCapture {
private:
    int fd;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    void* buffers[4];
    
public:
    USBCameraCapture(const std::string& device) {
        fd = open(device.c_str(), O_RDWR);
        
        // Set format
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 1920;
        fmt.fmt.pix.height = 1080;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        
        ioctl(fd, VIDIOC_S_FMT, &fmt);
        
        // Request buffers
        struct v4l2_requestbuffers req;
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        ioctl(fd, VIDIOC_REQBUFS, &req);
        
        // Map buffers
        for(int i = 0; i < 4; i++) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(fd, VIDIOC_QUERYBUF, &buf);
            
            buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
        }
        
        // Start streaming
        for(int i = 0; i < 4; i++) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(fd, VIDIOC_QBUF, &buf);
        }
        
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMON, &type);
    }
    
    bool captureFrame(uint8_t** data, size_t* size) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            return false;
        }
        
        *data = (uint8_t*)buffers[buf.index];
        *size = buf.bytesused;
        
        ioctl(fd, VIDIOC_QBUF, &buf);
        return true;
    }
    
    ~USBCameraCapture() {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        
        for(int i = 0; i < 4; i++) {
            munmap(buffers[i], buf.length);
        }
        
        close(fd);
    }
};
```

