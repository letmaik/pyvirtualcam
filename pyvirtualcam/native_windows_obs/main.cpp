#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "virtual_output.h"

namespace py = pybind11;

struct Camera {
    Camera(uint32_t width, uint32_t height, double fps) {
        virtual_output_start(width, height, fps);
    }

    void close() {
        virtual_output_stop();
    }

    const char* device() {
        return virtual_output_device();
    }

    void send(py::array_t<uint8_t, py::array::c_style> frame) {
        py::buffer_info buf = frame.request();    
        virtual_output_send(static_cast<uint8_t*>(buf.ptr));
    }
};

PYBIND11_MODULE(_native_windows_obs, m) {
    py::class_<Camera>(m, "Camera")
        .def(py::init<uint32_t, uint32_t, double>(),
             py::arg("width"), py::arg("height"), py::arg("fps"))
        .def("close", &Camera::close)    
        .def("send", &Camera::send)
        .def("device", &Camera::device);
}
