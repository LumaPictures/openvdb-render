set(PLUGIN_NAME openvdb_visualizer)

find_package(Maya)

add_definitions(-DLINUX -Werror -D_BOOL -DREQUIRE_IOSTREAM -fPIC -pthread)

find_path(LUMA_NODE_ID_INCLUDE NAMES lumaNodeId.h)
include_directories("${LUMA_NODE_ID_INCLUDE}")

include_directories(${MAYA_INCLUDE_DIRS})
link_directories(${MAYA_LIBRARY_DIR})

set(MAYA_LIBS OpenMaya Foundation OpenMayaAnim OpenMayaFX OpenMayaUI OpenMayaRender)

find_library(GLEW_STATIC_LIBRARY NAMES libGLEW.a)

file(GLOB SRC *.cpp)

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} ${MAYA_LIBS} GL ${GLEW_STATIC_LIBRARY})

install(TARGETS ${PLUGIN_NAME}
        DESTINATION plug-ins)
