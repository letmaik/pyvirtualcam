#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "CommandCam.h"

namespace py = pybind11;

class DShowCapture {
  private:
    std::string _device;
    uint32_t _width;
    uint32_t _height;
    bool _is_nv12;

  public:
    DShowCapture(char* device, uint32_t width, uint32_t height)
        : _device(device), _width(width), _height(height) {
    }

    py::array_t<uint8_t> capture() {
        std::vector<std::string> args {
            "CommandCam.exe",
            "/devname", _device,
            "/size", std::to_string(_width) + "x" + std::to_string(_height)
        };
        constexpr int argc = 5;
        char* argv[argc];
        for (size_t i=0; i < argc; i++) {
            argv[i] = (char*) args[i].c_str();
        }
        
        std::vector<uint8_t> img;
        uint32_t width, height;

        commandcam::main(argc, argv, img, width, height, _is_nv12);

        return py::array_t<uint8_t>({height, width, (uint32_t)3}, img.data());
    }

    bool is_nv12() {
        return _is_nv12;
    }
};

PYBIND11_MODULE(_win_dshow_capture, m) {
    py::class_<DShowCapture>(m, "DShowCapture")
        .def(py::init<char*, uint32_t, uint32_t>(),
             py::arg("device"), py::arg("width"), py::arg("height"))
        .def("capture", &DShowCapture::capture)
        .def("is_nv12", &DShowCapture::is_nv12);
}
