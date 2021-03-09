#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "controller/controller.h"

namespace py = pybind11;

class V4L2LoopbackCamera {
  private:
    Context context;

  public:
    V4L2LoopbackCamera(uint32_t width, uint32_t height, [[maybe_unused]] double fps) {
        virtual_output_start(context, width, height);
    }

    void close() {
        virtual_output_stop(context);
    }

    std::string device() {
        return virtual_output_device(context);
    }

    void send(py::array_t<uint8_t, py::array::c_style> frame) {
        py::buffer_info buf = frame.request();    
        virtual_output_send(context, static_cast<uint8_t*>(buf.ptr));
    }
};

PYBIND11_MODULE(_native_linux, m) {
    py::class_<V4L2LoopbackCamera>(m, "V4L2LoopbackCamera")
        .def(py::init<uint32_t, uint32_t, double>(),
             py::arg("width"), py::arg("height"), py::arg("fps"))
        .def("close", &V4L2LoopbackCamera::close)
        .def("send", &V4L2LoopbackCamera::send)
        .def("device", &V4L2LoopbackCamera::device);
}
