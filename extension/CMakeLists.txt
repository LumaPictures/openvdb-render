set(PLUGIN_NAME openvdb_extension)

add_definitions(-DLINUX -D_BOOL -DREQUIRE_IOSTREAM -pthread -D_LINUX)

find_package(Arnold)
find_package(MtoA)
find_package(Maya)

include_directories(${ARNOLD_INCLUDE_DIR})
include_directories(${MAYA_INCLUDE_DIRS})
include_directories(SYSTEM ${MTOA_INCLUDE_DIR})
link_directories(${MAYA_LIBRARY_DIR})

set(MAYA_LIBS OpenMaya Foundation OpenMayaAnim OpenMayaFX OpenMayaUI OpenMayaRender)

file(GLOB SRC *.cpp)

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} ${ARNOLD_LIBRARY} ${MAYA_LIBS} ${MTOA_mtoa_api_LIBRARY})

install(TARGETS ${PLUGIN_NAME}
        DESTINATION extensions)
