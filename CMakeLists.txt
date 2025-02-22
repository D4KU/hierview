cmake_minimum_required(VERSION 3.29)

project(hierview)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(nfd CONFIG REQUIRED)
find_package(Stb REQUIRED)

add_executable(hierview src/main.cpp)
target_sources(hierview PRIVATE FILE_SET CXX_MODULES FILES
    src/app.ixx
    src/window.ixx
    )
set_property(TARGET hierview PROPERTY CXX_STANDARD 23)

target_include_directories(hierview PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(hierview PRIVATE
    glfw
    glad::glad
    imgui::imgui
    nfd::nfd
    Boost::program_options
    )
