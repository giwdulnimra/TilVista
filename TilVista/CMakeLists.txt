cmake_minimum_required(VERSION 3.20)
set(PROJ_NAME "TilVista")
# ── Version ───────────────────────────────────────────────────────────────────
# APPVERSION format: v{MAJOR}_{MINOR_2D}{PATCH_2D}
set(TV_VERSION_MAJOR 0)
set(TV_VERSION_MINOR 5)
set(TV_VERSION_PATCH 30)

string(FORMAT TV_MINOR_STR "%02d" ${TV_VERSION_MINOR})
string(FORMAT TV_PATCH_STR "%02d" ${TV_VERSION_PATCH})
set(APPVERSION "v${TV_VERSION_MAJOR}_${TV_MINOR_STR}${TV_PATCH_STR}")

project(TilVista
    VERSION "${TV_VERSION_MAJOR}.${TV_VERSION_MINOR}.${TV_VERSION_PATCH}"
    LANGUAGES CXX)

message(STATUS "TilVista APPVERSION: ${APPVERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_SOURCE_DIR}/SetupQt.cmake")

# ── Sources ───────────────────────────────────────────────────────────────────
set(TV_SOURCES
    src/main.cpp

    src/core/pathutils.h        src/core/pathutils.cpp
    src/core/config.h           src/core/config.cpp

    src/workers/scanworker.h    src/workers/scanworker.cpp
    src/workers/dbworkers.h     src/workers/dbworkers.cpp
    src/workers/catalogueworkers.h src/workers/catalogueworkers.cpp
    src/workers/videoframesworker.h src/workers/videoframesworker.cpp

    src/ui/previewlabel.h       src/ui/previewlabel.cpp
    src/ui/videopreviewwidget.h src/ui/videopreviewwidget.cpp
    src/ui/dirbar.h             src/ui/dirbar.cpp
    src/ui/dirdatabasepanel.h   src/ui/dirdatabasepanel.cpp
    src/ui/shujukopanel.h       src/ui/shujukopanel.cpp
    src/ui/aleavuetab.h         src/ui/aleavuetab.cpp
    src/ui/sattumapictab.h      src/ui/sattumapictab.cpp
    src/ui/slideshowwindow.h    src/ui/slideshowwindow.cpp
    src/ui/mainwindow.h         src/ui/mainwindow.cpp

    resources/resources.qrc
)

add_executable(TilVista ${TV_SOURCES})

target_include_directories(TilVista PRIVATE src)

# Expose version strings to C++ code
target_compile_definitions(TilVista PRIVATE
    TV_APPVERSION="${APPVERSION}"
    TV_VERSION="${TV_VERSION_MAJOR}.${TV_VERSION_MINOR}.${TV_VERSION_PATCH}"
)

setup_qt_target(TilVista)
install(TARGETS TilVista DESTINATION bin)
