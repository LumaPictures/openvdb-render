set(PLUGIN_NAME openvdb_shader)

find_package(Arnold)

include_directories(${ARNOLD_INCLUDE_DIR})

if (ARNOLD_VERSION_ARCH_NUM VERSION_GREATER "4")
    set(SRC
        loader.cpp
        openvdb_sampler.cpp
        openvdb_simple_shader.cpp)
else ()
    file(GLOB SRC *.cpp)
endif ()
if (INCLUDE_HEADERS_IN_BUILD)
    file(GLOB HDR *.hpp *.h ../util/*.hpp ../util/*.h)
    set(SRC ${SRC} ${HDR})
endif ()

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} ${ARNOLD_LIBRARY})
if (ARNOLD_VERSION_ARCH_NUM VERSION_GREATER "4")
    target_compile_definitions(${PLUGIN_NAME} PRIVATE "-DARNOLD5")
endif ()

install(TARGETS ${PLUGIN_NAME}
        DESTINATION shaders)
