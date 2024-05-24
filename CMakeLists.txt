cmake_minimum_required(VERSION 3.28)
project("wayland_module")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(PkgConfig REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client Cursor)
find_package(WaylandProtocols REQUIRED)
find_package(WaylandScanner REQUIRED)
find_package(X11 REQUIRED COMPONENTS xkbcommon)

include(FetchContent)
FetchContent_Declare(
  std
  URL "file:///home/karenw/src/llvm-project-build/modules/c++/v1/"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM
)
FetchContent_MakeAvailable(std)

add_executable(demo main.cpp)
ecm_add_wayland_client_protocol(demo PROTOCOL ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml BASENAME xdg-shell)
ecm_add_wayland_client_protocol(demo PROTOCOL ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml BASENAME xdg-decoration)
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(demo std Wayland::Client Wayland::Cursor X11::xkbcommon)
target_sources(demo PRIVATE FILE_SET CXX_MODULES FILES wayland.cppm wayland/buffer.cppm wayland/common.cppm wayland/display.cppm wayland/external.cppm wayland/seat.cppm wayland/window.cppm xkb.cppm xkb/external.cppm)
