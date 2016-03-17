set(PLUGIN_NAME openvdb_shader)

find_package(Arnold)

include_directories(${ARNOLD_INCLUDE_DIR})

file(GLOB SRC *.cpp)

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} ${ARNOLD_LIBRARY})

install(TARGETS ${PLUGIN_NAME}
        DESTINATION shaders)
