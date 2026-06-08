#ifndef JETSON_CAMERA_HPP
#define JETSON_CAMERA_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

class Camera {
public:
    struct Resolution {
        int width;
        int height;
        
        bool operator==(const Resolution& other) const {
            return width == other.width && height == other.height;
        }
    };
    
    struct Frame {
        uint8_t* data;
        size_t size;
        int width;
        int height;
        int format;     // V4L2_PIX_FMT_*
        uint64_t timestamp;  // microseconds
    };
    
    struct Config {
        Resolution resolution{1920, 1080};
        int fps = 30;
        int format = 0;     // V4L2_PIX_FMT_YUYV, MJPEG, etc.
        int bitrate = 0;    // For compressed formats
        bool auto_focus = true;
        bool auto_white_balance = true;
        bool auto_exposure = true;
    };
    
    // Constructor/Destructor
    Camera(const std::string& device = "/dev/video0");
    ~Camera();
    
    // Device operations
    bool open(const Config& config);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    bool setConfig(const Config& config);
    Config getConfig() const;
    
    // Frame capture
    bool captureFrame(Frame& frame, int timeout_ms = 1000);
    bool captureFrameBlocking(Frame& frame);
    bool startStreaming(std::function<void(const Frame&)> callback);
    void stopStreaming();
    
    // Properties
    bool setBrightness(int value);      // 0-255
    bool setContrast(int value);        // 0-255
    bool setSaturation(int value);      // 0-255
    bool setHue(int value);             // 0-255
    bool setGamma(int value);           // 0-255
    bool setGain(int value);            // 0-255
    bool setExposure(int value);        // microseconds
    bool setWhiteBalance(int value);    // Kelvin
    bool setFocus(int value);           // diopters
    
    bool getBrightness(int& value);
    bool getContrast(int& value);
    bool getSaturation(int& value);
    bool getHue(int& value);
    bool getGamma(int& value);
    bool getGain(int& value);
    bool getExposure(int& value);
    bool getWhiteBalance(int& value);
    bool getFocus(int& value);
    
    // Controls
    bool setAutoExposure(bool enable);
    bool setAutoWhiteBalance(bool enable);
    bool setAutoFocus(bool enable);
    bool triggerAutoFocus();
    
    // ISP (Image Signal Processing)
    bool enableDenoise(bool enable);
    bool enableSharpening(bool enable);
    bool enableHDR(bool enable);
    bool setROI(int x, int y, int width, int height);
    void clearROI();
    
    // GStreamer integration
    std::string getGStreamerPipeline() const;
    bool saveFrame(const Frame& frame, const std::string& filename);
    
    // Utility
    std::string getDevice() const { return device_; }
    std::vector<Resolution> getSupportedResolutions() const;
    std::vector<int> getSupportedFormats() const;
    static std::vector<std::string> getAvailableCameras();
    static std::string formatToString(int format);
    
    // IMX219 specific (Raspberry Pi Camera Module v2)
    class IMX219 : public Camera {
    public:
        IMX219(const std::string& device = "/dev/video0");
        bool setGainAnalog(int gain);      // 0-255
        bool setGainDigital(int gain);     // 0-255
        bool setExposureLines(int lines);  // 1-0xFFFF
        bool setTestPattern(bool enable);
    };
    
    // IMX477 specific (High Quality Camera)
    class IMX477 : public Camera {
    public:
        IMX477(const std::string& device = "/dev/video0");
        bool setGainAnalog(int gain);      // 0-255
        bool setExposureTime(int us);      // microseconds
        bool setBlackLevel(int level);     // 0-4095
        bool setTestPattern(bool enable);
    };
    
private:
    std::string device_;
    int fd_;
    Config config_;
    void* buffers_[4];
    std::thread stream_thread_;
    std::atomic<bool> streaming_;
    std::function<void(const Frame&)> callback_;
    
    void initMMAP();
    void initUserPtr();
    void initDMABUF();
    void streamLoop();
    bool xioctl(int request, void* arg);
};

#endif // JETSON_CAMERA_HPP
