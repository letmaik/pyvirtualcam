#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "virtual_output.h"

namespace py = pybind11;

class Camera {
  private:
    VirtualOutput virtual_output;

  public:
    Camera(uint32_t width, uint32_t height, [[maybe_unused]] double fps, uint32_t fourcc)
     : virtual_output {width, height, fourcc} {
    }

    void close() {
        virtual_output.stop();
    }

    std::string device() {
        return virtual_output.device();
    }

    uint32_t native_fourcc() {
        return virtual_output.native_fourcc();
    }

    void send(py::array_t<uint8_t, py::array::c_style> frame) {
        py::buffer_info buf = frame.request();    
        virtual_output.send(static_cast<uint8_t*>(buf.ptr));
    }
};

PYBIND11_MODULE(_native_linux_v4l2loopback, m) {
    py::class_<Camera>(m, "Camera")
        .def(py::init<uint32_t, uint32_t, double, uint32_t>(),
             py::arg("width"), py::arg("height"), py::arg("fps"),
             py::kw_only(), py::arg("fourcc"))
        .def("close", &Camera::close)
        .def("send", &Camera::send)
        .def("device", &Camera::device)
        .def("native_fourcc", &Camera::native_fourcc);
}
