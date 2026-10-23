cmake_minimum_required(VERSION 3.20)
project(kde-perfmon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Test)

add_library(perfmoncore STATIC
    src/procdata.cpp
    src/processmodel.cpp
    src/perfbackend.cpp
)
set_target_properties(perfmoncore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(perfmoncore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(perfmoncore PUBLIC Qt6::Core Qt6::Gui)

add_library(perfmonplugin MODULE src/perfmonplugin.cpp)
target_link_libraries(perfmonplugin PRIVATE perfmoncore Qt6::Qml)
set_target_properties(perfmonplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# Keep the source package directly runnable (preview tool, plasmawindowed, ...).
add_custom_command(TARGET perfmonplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:perfmonplugin>
        ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/lib/Hao/PerfMon/libperfmonplugin.so
)

add_executable(perfmon-preview tools/preview.cpp)
target_link_libraries(perfmon-preview PRIVATE Qt6::Quick Qt6::Qml Qt6::Test)

enable_testing()
add_executable(perfmon-tests tests/tests.cpp)
target_link_libraries(perfmon-tests PRIVATE perfmoncore Qt6::Test Qt6::Gui)
add_test(NAME perfmon-tests COMMAND perfmon-tests)
