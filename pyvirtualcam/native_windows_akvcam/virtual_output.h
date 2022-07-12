#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <vector>
#include "../native_shared/image_formats.h"

class VirtualOutput {
  private:
    std::string _device_name;

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
        
        //Get number of cameras available
        if(!getDwordRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras","size",number_cameras)){
            throw std::runtime_error("Unable to get number of available cameras");
        }

        //Get names of cameras available
        std::string cameraDescription;
        for(int i = 1; i <= number_cameras; i++){
            if(!getStringRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i),"description", cameraDescription)){
                throw std::runtime_error("Unable to get camera " + std::to_string(i) + " description.");
            }
            std::cout<<cameraDescription << std::endl;

            int cameraFormats;
            if(!getDwordRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i) + "\\Formats","size", cameraFormats)){
                throw std::runtime_error("Unable to get number camera formats");
            }

            int width;
            int height;
            for(int j = 1; j <= cameraFormats; j++){

                std::cout<< "Format #" << std::to_string(j) << ":" << std::endl;
                if(!getDwordRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i) + "\\Formats\\" + std::to_string(j),"height", height)){
                    throw std::runtime_error("Unable to get camera " + std::to_string(i) + " format "+ std::to_string(j) +" height.");
                }

                if(!getDwordRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Webcamoid\\VirtualCamera\\Cameras\\" + std::to_string(i) + "\\Formats\\" + std::to_string(j),"width", width)){
                    throw std::runtime_error("Unable to get camera " + std::to_string(i) + " format "+ std::to_string(j) +" width.");
                }
                std::cout<< " > height: " << std::to_string(height) << std::endl;
                std::cout<< " > width: " << std::to_string(width) << std::endl;
            }


        }

    }

    void stop()
    {

    }

    void send(const uint8_t *frame)
    {
       
    }

    std::string device()
    {
        return _device_name;
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_YUY2;
    }
};
