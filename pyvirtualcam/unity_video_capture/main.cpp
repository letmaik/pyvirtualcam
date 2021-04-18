#include <stdexcept>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "virtual_output.h"

namespace py = pybind11;

class UnityCaptureCamera {
  private:
    VirtualOutput virtual_output;

  public:
    UnityVideoCapture(uint32_t width, uint32_t height, double fps, uint32_t fourcc, std::optional<std::wstring> device) : virtual_output {width, height, fps, fourcc, device} {
    }

    void close() {
        virtual_output.stop();
    }

    std::wstring device() {
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

PYBIND11_MODULE(_unity_video_capture, n) {
    py::class_<UnityVideoCapture>(n, "Camera")
        .def(py::init<uint32_t, uint32_t, double, uint32_t, std::optional<std::wstring>>(),
             py::kw_only(),
             py::arg("width"), py::arg("height"), py::arg("fps"),
             py::arg("fourcc"), py::arg("device"))
        .def("close", &UnityCaptureCamera::close)
        .def("send", &UnityCaptureCamera::send)
        .def("device", &UnityCaptureCamera::device)
        .def("native_fourcc", &UnityCaptureCamera::native_fourcc);
}
