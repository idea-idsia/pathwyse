from setuptools import setup, Extension
from Cython.Build import cythonize
import sys, os

# Extension module
# MSVC is assumed on Windows unless the build is run with --compiler=mingw32
msvc = sys.platform.startswith("win") and not any("mingw" in a for a in sys.argv)
if msvc:
    ext_modules = [
        Extension(
            "wrapper",                                      # Module name
            sources=["src/wrapper_python/wrapper.pyx"],    # Cython source file
            libraries=["pathwyse_core"],
            library_dirs=['bin/'],                          # Directory containing pw.so
            include_dirs=['src/core/'],                     # Directory containing solver.h
            language="c++",
            extra_compile_args=["/std:c++20", "/O2"],
        )
    ]
    os.add_dll_directory(os.getcwd()+"\\bin")
elif sys.platform.startswith("win"):
    ext_modules = [
        Extension(
            "wrapper",
            sources=["src/wrapper_python/wrapper.pyx"],
            libraries=["pathwyse_core"],
            library_dirs=['bin/'],
            include_dirs=['src/core/'],
            language="c++",
            extra_compile_args=["-O3", "-std=c++20"],
        )
    ]
else:
    ext_modules = [
        Extension(
            "wrapper",                                      # Module name
            sources=["src/wrapper_python/wrapper.pyx"],    # Cython source file
            libraries=["pathwyse_core"],
            library_dirs=['bin/'],                          # Directory containing pw.so
            include_dirs=['src/core/'],                     # Directory containing solver.h
            language="c++",
            extra_compile_args=["-O3", "-std=c++20"],
            runtime_library_dirs=["bin/"],
        )
    ]

setup(ext_modules=cythonize(ext_modules, compiler_directives={'language_level': "3"}),)
