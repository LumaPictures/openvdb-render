add_compile_options(-DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")

find_package(Boost COMPONENTS regex python REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(TBB REQUIRED)
find_package(USD REQUIRED)
find_package(USDArnold REQUIRED)

include_directories(SYSTEM ${USD_INCLUDE_DIR})
link_directories(${USD_LIBRARY_DIR})

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})
include_directories(SYSTEM ${TBB_INCLUDE_DIRS})

include_directories(SYSTEM ${USD_ARNOLD_INCLUDE_DIR})

add_subdirectory(writer)