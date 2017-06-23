# Simple module to find USD.

if (EXISTS "$ENV{USD_ROOT}")
    set(USD_ROOT $ENV{USD_ROOT})
endif ()

# USD core components

find_path(USD_INCLUDE_DIR pxr/pxr.h
          PATHS ${USD_ROOT}/include
          DOC "USD Include directory")

find_path(USD_LIBRARY_DIR libusd.so
          PATHS ${USD_ROOT}/lib
          DOC "USD Librarires directory")

find_file(USD_GENSCHEMA
          names usdGenSchema
          PATHS ${USD_ROOT}/bin
          DOC "USD Gen schema application")

# USD Maya components

find_path(USD_MAYA_INCLUDE_DIR usdMaya/api.h
          PATHS ${USD_ROOT}/third_party/maya/include
          DOC "USD Maya Include directory")

find_path(USD_MAYA_LIBRARY_DIR libusdMaya.so
          PATHS ${USD_ROOT}/third_party/maya/lib
          DOC "USD Maya Library directory")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    USD
    REQUIRED_VARS
    USD_INCLUDE_DIR
    USD_LIBRARY_DIR
    USD_GENSCHEMA)
