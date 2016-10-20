set(PLUGIN_NAME openvdb_render)

add_definitions(-DLINUX -D_BOOL -DREQUIRE_IOSTREAM -pthread)

find_package(Maya)
find_package(Boost COMPONENTS regex)

if (NOT ILMBASE_ROOT)
    set(ILMBASE_ROOT $ENV{ILMBASE_ROOT})
endif ()

find_path(GLEW_INCLUDE NAMES GL/glew.h)
find_path(OPENVDB_INCLUDE NAMES openvdb/openvdb.h)
include_directories(${LUMA_NODE_ID_INCLUDE})
include_directories(${OPENVDB_INCLUDE})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${ILMBASE_ROOT}/include) #register is deprecated with c++11

include_directories(${MAYA_INCLUDE_DIRS})
link_directories(${MAYA_LIBRARY_DIR})

set(MAYA_LIBS OpenMaya Foundation OpenMayaAnim OpenMayaFX OpenMayaUI OpenMayaRender)

find_library(OPENVDB_STATIC_LIBRARY NAMES libopenvdb.a)

file(GLOB SRC *.cpp)

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
if (WIN32)
    set_target_properties(${PLUGIN_NAME} PROPERTIES SUFFIX ".mll")
endif ()
target_link_libraries(${PLUGIN_NAME} ${MAYA_LIBS} ${OPENVDB_STATIC_LIBRARY} ${Boost_LIBRARIES})

install(TARGETS ${PLUGIN_NAME}
        DESTINATION plug-ins)

file(GLOB TEMPLATES template/*.py)

install(FILES ${TEMPLATES}
        DESTINATION scripts)
