#include <stdexcept>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "controller/controller.h"

namespace py = pybind11;

void start(int width, int height, double fps, int delay) {
    (void)delay;
    if (!virtual_output_start(width, height, fps))
        throw std::runtime_error("error starting virtual camera output");
}

void send(py::array_t<uint8_t, py::array::c_style> frame) {
    py::buffer_info buf = frame.request();    
    virtual_video((uint8_t*)buf.ptr);
}

PYBIND11_MODULE(_native_windows, m) {
    m.def("start", &start, R"pbdoc(
        Start the virtual cam output.
    )pbdoc");

    m.def("stop", &virtual_output_stop, R"pbdoc(
        Stop the virtual cam output.
    )pbdoc");

    m.def("send", &send, R"pbdoc(
        Send frame to the virtual cam.
    )pbdoc");
}
