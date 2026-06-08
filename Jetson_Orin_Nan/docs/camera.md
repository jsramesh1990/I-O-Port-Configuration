
### docs/camera.md

```markdown
# CSI Camera Interface

## Overview

The Jetson Orin Nano supports MIPI CSI-2 cameras through dedicated camera connectors.

## Specifications

| Parameter | Value |
|-----------|-------|
| CSI Lanes | 2 (x2 or x1 configuration) |
| Max Data Rate | 1.5 Gbps per lane |
| Supported Sensors | IMX219, IMX477, OV5640 |
| Resolution | Up to 8MP (3280x2464) |
| Frame Rate | Up to 60fps at 1080p |
| V4L2 Support | Yes |
| GStreamer | Full hardware acceleration |

## Implementation

```cpp
class CSICamera {
private:
    int fd;
    struct v4l2_format fmt;
    void* buffers[4];
    
public:
    CSICamera(const std::string& device, int width, int height) {
        fd = open(device.c_str(), O_RDWR);
        
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        
        ioctl(fd, VIDIOC_S_FMT, &fmt);
        
        // Request and map buffers
        struct v4l2_requestbuffers req = {0};
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        ioctl(fd, VIDIOC_REQBUFS, &req);
        
        for(int i = 0; i < 4; i++) {
            struct v4l2_buffer buf = {0};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(fd, VIDIOC_QUERYBUF, &buf);
            
            buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
            ioctl(fd, VIDIOC_QBUF, &buf);
        }
        
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMON, &type);
    }
    
    bool captureFrame(uint8_t** data, size_t* size) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        
        if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0) return false;
        
        *data = (uint8_t*)buffers[buf.index];
        *size = buf.bytesused;
        
        ioctl(fd, VIDIOC_QBUF, &buf);
        return true;
    }
};

