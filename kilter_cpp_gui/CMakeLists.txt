cmake_minimum_required(VERSION 3.16)
project(kilter_gui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

add_executable(kilter_gui
  src/main.cpp
  src/generator_process.cpp
  src/json_util.cpp
  src/util.cpp
  src/route.cpp
  src/board_db.cpp
  src/results_db.cpp
  src/token_loader.cpp
  src/ui.cpp
)

target_include_directories(kilter_gui PRIVATE include)

target_link_libraries(kilter_gui
  PRIVATE
    PkgConfig::GTK4
    PkgConfig::SQLITE3
)
