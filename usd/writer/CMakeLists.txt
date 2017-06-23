set(PLUGIN_NAME vdbVisualizerWriter)

file(GLOB SRC *.cpp)

include_directories(SYSTEM ${USD_MAYA_INCLUDE_DIR})
link_directories(${USD_MAYA_LIBRARY_DIR})

find_package(Maya)

include_directories(${MAYA_INCLUDE_DIRS})
link_directories(${MAYA_LIBRARY_DIR})

set(MAYA_LIBS OpenMaya Foundation OpenMayaAnim OpenMayaFX OpenMayaUI OpenMayaRender)

add_library(${PLUGIN_NAME} SHARED ${SRC})
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PLUGIN_NAME} ${PARTIO_LIBRARIES} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${MAYA_LIBS} tf gf sdf usd usdGeom usdMaya)

install(TARGETS ${PLUGIN_NAME}
        DESTINATION usd/plug-ins)

set(PLUGINFO_OUT ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json)
configure_file(plugInfo.json.in ${PLUGINFO_OUT})

install(FILES ${PLUGINFO_OUT}
        DESTINATION usd)