# https://github.com/pybind/python_example/blob/master/setup.py

import platform
import sys
import glob
from distutils.unixccompiler import UnixCCompiler
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

common_src = glob.glob('external/libyuv/source/*.cc')
common_inc = [get_pybind_include(), 'external/libyuv/include']

if platform.system() == 'Windows':
    ext_modules.append(
        Extension('pyvirtualcam._native_windows_obs',
            # Sort input source files to ensure bit-for-bit reproducible builds
            # (https://github.com/pybind/python_example/pull/53)
            sorted([
                'pyvirtualcam/native_windows_obs/main.cpp',
                'pyvirtualcam/native_windows_obs/queue/shared-memory-queue.c',
            ] + common_src),
            include_dirs=['pyvirtualcam/native_windows_obs'] + common_inc,
            extra_link_args=["/DEFAULTLIB:advapi32.lib"],
            language='c++'
        )
    )
elif platform.system() == 'Darwin':
    ext_modules.append(
        Extension('pyvirtualcam._native_macos_obs',
            # Sort input source files to ensure bit-for-bit reproducible builds
            # (https://github.com/pybind/python_example/pull/53)
            sorted([
                'pyvirtualcam/native_macos_obs/main.mm',
                'pyvirtualcam/native_macos_obs/server/OBSDALMachServer.mm',
            ] + common_src),
            include_dirs=['pyvirtualcam/native_macos_obs'] + common_inc,
            extra_link_args=["-framework", "Foundation"],
            language='objc'
        )
    )
elif platform.system() == 'Linux':
    ext_modules.append(
        Extension('pyvirtualcam._native_linux_v4l2loopback',
            # Sort input source files to ensure bit-for-bit reproducible builds
            # (https://github.com/pybind/python_example/pull/53)
            sorted(['pyvirtualcam/native_linux_v4l2loopback/main.cpp'] + common_src),
            include_dirs=['pyvirtualcam/native_linux_v4l2loopback'] + common_inc,
            extra_compile_args=['-flto'],
            language='c++'
        )
    )
else:
    raise NotImplementedError('unsupported OS')

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


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc', '/std:c++17'],
        'unix': ['-std=c++17'],
    }
    l_opts = {
        'msvc': [],
        'unix': [],
    }

    UnixCCompiler.src_extensions.append(".mm")
    UnixCCompiler.language_map[".mm"] = "objc"

    if sys.platform == 'darwin':
        darwin_opts = ['-stdlib=libc++']
        c_opts['unix'] += darwin_opts
        l_opts['unix'] += darwin_opts

    def build_extensions(self):
        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        link_opts = self.l_opts.get(ct, [])
        if ct == 'unix':
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')

        for ext in self.extensions:
            ext.define_macros = [('VERSION_INFO', '"{}"'.format(self.distribution.get_version()))]
            ext.extra_compile_args += opts
            ext.extra_link_args += link_opts
        build_ext.build_extensions(self)

# make __version__ available (https://stackoverflow.com/a/16084844)
exec(open('pyvirtualcam/_version.py').read())

setup(
    name='pyvirtualcam',
    version=__version__,
    author='Maik Riechert',
    author_email='maik.riechert@arcor.de',
    url='https://github.com/letmaik/pyvirtualcam',
    description='Send frames to a virtual camera',
    long_description = open('README.md').read(),
    long_description_content_type='text/markdown',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Natural Language :: English',
        'License :: OSI Approved :: GNU General Public License v2 (GPLv2)',
        'Programming Language :: Python :: 3',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: MacOS',
        'Operating System :: POSIX :: Linux',
        'Topic :: Multimedia :: Video',
        'Topic :: Software Development :: Libraries',
    ],
    ext_modules=ext_modules,
    packages = find_packages(),
    setup_requires=['pybind11>=2.6.0'],
    install_requires=['numpy'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)