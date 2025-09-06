#include <stdexcept>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <cstdint>
#include <string>
#include "virtual_output.hpp"

namespace py = pybind11;

class Camera {
    VirtualOutput virtualOutput;

  public:
    Camera(uint32_t width, uint32_t height, __unused double fps,
           uint32_t fourcc, std::optional<std::string> device_)
     : virtualOutput {width, height, fourcc, device_} {
    }

    void close() {
        virtualOutput.stop();
    }

    std::string device() {
        return virtualOutput.device();
    }

    uint32_t native_fourcc() {
        return virtualOutput.native_fourcc();
    }

    void send(py::array_t<uint8_t, py::array::c_style> frame) {
        py::buffer_info buf = frame.request();
        virtualOutput.send(static_cast<uint8_t*>(buf.ptr));
    }
};

PYBIND11_MODULE(_native_macos_obs_cmioextension, m) {
    py::class_<Camera>(m, "Camera")
        .def(py::init<uint32_t, uint32_t, double, uint32_t, std::optional<std::string>>(),
             py::kw_only(),
             py::arg("width"), py::arg("height"), py::arg("fps"),
             py::arg("fourcc"), py::arg("device"))
        .def("close", &Camera::close)
        .def("send", &Camera::send)
        .def("device", &Camera::device)
        .def("native_fourcc", &Camera::native_fourcc);
}
