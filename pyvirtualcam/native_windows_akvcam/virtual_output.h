#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <map>
#include "../native_shared/image_formats.h"

struct StreamProcess{
    HANDLE stdinReadPipe;
    HANDLE stdinWritePipe;
    SECURITY_ATTRIBUTES pipeAttributes;
    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION procInfo;
};

struct CameraInfo {
    std::string desc;
    std::string id;
};

static std::set<std::string> ACTIVE_DEVICES;

class VirtualOutput {
  private:
    char * buffer;
    std::string _device_name;
    std::optional<CameraInfo> _camera_info;
    uint32_t _width;
    uint32_t _height;
    uint32_t _fourcc;

    bool _running = false;

    StreamProcess stream_proc;

    bool getDwordRegistry(HKEY key, std::string subkey, std::string valueName, int& data){
        DWORD dataSize = sizeof(data);

        if (RegGetValueA(
            key, 
            subkey.c_str(),
            valueName.c_str(), 
            RRF_RT_REG_DWORD, 
            nullptr,
            &data,
            &dataSize) != ERROR_SUCCESS){
                return false;
            }

        return true; 
    }

    bool getStringRegistry(HKEY key, std::string subkey, std::string valueName, std::string& data){
        DWORD dataSize = sizeof(data);
        HRESULT hr;

        hr = RegGetValueA(key, subkey.c_str(), valueName.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &dataSize);
        
        if (hr != ERROR_SUCCESS){
            return false;
        }

        data.resize(dataSize);

        hr = RegGetValueA(key, subkey.c_str(), valueName.c_str(), RRF_RT_REG_SZ, nullptr, data.data(), &dataSize);

        if (hr != ERROR_SUCCESS){
            return false;
        }
        
        return true; 
    }

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc, std::optional<std::string> device) {

        int number_cameras;
        std::cout<<" enter" <<std::endl;
        //Get number of cameras available
        if(!getDwordRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras","size",number_cameras)){
            throw std::runtime_error("Unable to get number of available cameras");
        }
        if(number_cameras < 0){
            throw std::runtime_error("No cameras avaliable. Did you add one device using AkVCamManager add-device?");
        }

        std::cout<< "Cameras available: " << std::to_string(number_cameras)<<std::endl;
        
        if(device.has_value()){
            std::string device_name = *device;
            if(ACTIVE_DEVICES.count(device_name) > 0){
                throw std::invalid_argument("Device " + device_name + " is already in use.");
            }
        }

        std::cout<<"Looking for cameras..." <<std::endl;

        //Get info on cameras
        std::string tmpCameraDesc;
        std::string tmpCameraID;
        for(int i = 1; i <= number_cameras; i++){

            std::cout<<"Getting camera " << std::to_string(i) << " info" <<std::endl;

            if(!getStringRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i),"description", tmpCameraDesc)){
                throw std::runtime_error("Unable to get camera " + std::to_string(i) + " description.");
            }

            if(!getStringRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i),"id", tmpCameraID)){
                throw std::runtime_error("Unable to get camera " + std::to_string(i) + " id.");
            }

            std::cout<<"Camera desc: " << tmpCameraDesc <<std::endl;
            std::cout<<"Camera id: " << tmpCameraID <<std::endl;

            if((device.has_value() && tmpCameraDesc == *device) || (!device.has_value() && ACTIVE_DEVICES.count(tmpCameraDesc) == 0)){
                std::cout<< "Saving Camera!" << std::endl;
                _camera_info ={tmpCameraDesc, tmpCameraID};
                break;
            }
        }


        if(!_camera_info.has_value() && number_cameras > 0){
            throw std::runtime_error("All cameras being used...");
        }

        _width = width;
        _height = height;
        _fourcc = libyuv::CanonicalFourCC(fourcc);
        std::string format;
        
        switch(_fourcc) {
            case libyuv::FOURCC_RAW:
                format = "RGB24";
                break;
            case libyuv::FOURCC_NV12:
                format = "NV12";
                break;
            case libyuv::FOURCC_YUY2:
                format = "YUY2";
                break;
            case libyuv::FOURCC_UYVY:
                format = "UYVY";
                break;
            default:
                throw std::runtime_error("Unsupported image format.");
        }

        char cmd[1024];

        std::string akvcam_mamanger_path;
        if(!getStringRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera","installPath", akvcam_mamanger_path)){
            throw std::runtime_error("Unable to get akvcam installation path.");
        }

        memset(cmd, 0, 1024);
        snprintf(cmd, 1024, "\"%s\\x64\\AkVCamManager.exe\" stream %s %s %d %d", akvcam_mamanger_path.c_str(), _camera_info->id.c_str(), format.c_str(), _width, _height);

        // Get the handles to the standard input and standard output.
        memset(&stream_proc, 0, sizeof(StreamProcess));
        stream_proc.stdinReadPipe = NULL;
        stream_proc.stdinWritePipe = NULL;
        memset(&stream_proc.pipeAttributes, 0, sizeof(SECURITY_ATTRIBUTES));
        stream_proc.pipeAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        stream_proc.pipeAttributes.bInheritHandle = true;
        stream_proc.pipeAttributes.lpSecurityDescriptor = NULL;
        CreatePipe(&stream_proc.stdinReadPipe,&stream_proc.stdinWritePipe,&stream_proc.pipeAttributes,0);
        SetHandleInformation(stream_proc.stdinWritePipe,HANDLE_FLAG_INHERIT,0);

        STARTUPINFOA startupInfo;
        memset(&startupInfo, 0, sizeof(STARTUPINFOA));
        startupInfo.cb = sizeof(STARTUPINFOA);
        startupInfo.hStdInput = stream_proc.stdinReadPipe;
        startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        startupInfo.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION procInfo;
        memset(&procInfo, 0, sizeof(PROCESS_INFORMATION));

        // Start the stream.
        if(!CreateProcessA(NULL,cmd,NULL,NULL,true,0,NULL,NULL,&startupInfo,&procInfo)){
            throw std::runtime_error("Unable to create AkVCam process." + std::to_string(HRESULT_FROM_WIN32(GetLastError())));
        }

        // Allocate the frame buffer.
        size_t buffer_size = 3 * _width;
        buffer = (char *)malloc(buffer_size);

        ACTIVE_DEVICES.insert(_camera_info->desc);
        _running = true;

    }   
    void stop()
    {
        if(!_running){
            return;
        }

        free(buffer);
        ACTIVE_DEVICES.erase(_camera_info->desc);
        _running = false;

        CloseHandle(stream_proc.stdinWritePipe);
        CloseHandle(stream_proc.stdinReadPipe);

        WaitForSingleObject(stream_proc.procInfo.hProcess, INFINITE);
        CloseHandle(stream_proc.procInfo.hProcess);
        CloseHandle(stream_proc.procInfo.hThread);
    }

    void send(const uint8_t *frame)
    {
        if(!_running){
            return;
        }

       for (uint32_t y = 0; y < _height; y++) {
            for (size_t byte = 0; byte < 3*_width; byte++)
                buffer[byte] = frame[0,0] & 0xff;

            DWORD bytesWritten = 0;
            WriteFile(stream_proc.stdinWritePipe, buffer,DWORD(3*_width),&bytesWritten,NULL);
        }
    }

    std::string device()
    {
        return _device_name;
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_YUY2;
    }
};
