# https://github.com/pybind/python_example/blob/master/setup.py

import glob
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
import setuptools

class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __str__(self):
        import pybind11
        return pybind11.get_include()

ext_modules = []

common_src = glob.glob('../../external/libyuv/source/*.cc')
common_inc = [get_pybind_include(), '../../external/libyuv/include']

ext_modules.append(
    Extension('pyvirtualcam_win_dshow_capture._win_dshow_capture',
        ['main.cpp', 'CommandCam.cpp'] + common_src,
        include_dirs=common_inc,
        extra_link_args=["/DEFAULTLIB:strmiids.lib"],
        language='c++'
    )
)

# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    import os
    with tempfile.NamedTemporaryFile('w', suffix='.cpp', delete=False) as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        fname = f.name
    try:
        compiler.compile([fname], extra_postargs=[flagname])
    except setuptools.distutils.errors.CompileError:
        return False
    finally:
        try:
            os.remove(fname)
        except OSError:
            pass
    return True


def cpp_flag(compiler):
    """Return the -std=c++[11/14/17] compiler flag.
    The newer version is prefered over c++11 (when it is available).
    """
    flags = ['-std=c++17', '-std=c++14', '-std=c++11']

    for flag in flags:
        if has_flag(compiler, flag):
            return flag

    raise RuntimeError('Unsupported compiler -- at least C++11 support '
                       'is needed!')


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc'],
        'unix': [],
    }
    l_opts = {
        'msvc': [],
        'unix': [],
    }

    def build_extensions(self):
        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        link_opts = self.l_opts.get(ct, [])
        if ct == 'unix':
            opts.append(cpp_flag(self.compiler))
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')

        for ext in self.extensions:
            ext.define_macros = [('VERSION_INFO', '"{}"'.format(self.distribution.get_version()))]
            ext.extra_compile_args += opts
            ext.extra_link_args += link_opts
        build_ext.build_extensions(self)

setup(
    name='pyvirtualcam_win_dshow_capture',
    version='0.1.0',
    ext_modules=ext_modules,
    packages = find_packages(),
    setup_requires=['pybind11>=2.6.0'],
    install_requires=['numpy'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)