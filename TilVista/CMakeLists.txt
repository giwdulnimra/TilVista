cmake_minimum_required(VERSION 3.20)

# ── Version ───────────────────────────────────────────────────────────────────
# string(FORMAT ...) requires CMake 3.29 – use if/set for zero-padding instead.
set(TV_VERSION_MAJOR 0)
set(TV_VERSION_MINOR 5)
set(TV_VERSION_PATCH 31)

# Zero-pad minor and patch to 2 digits
if(TV_VERSION_MINOR LESS 10)
    set(TV_MINOR_PAD "0${TV_VERSION_MINOR}")
else()
    set(TV_MINOR_PAD "${TV_VERSION_MINOR}")
endif()
if(TV_VERSION_PATCH LESS 10)
    set(TV_PATCH_PAD "0${TV_VERSION_PATCH}")
else()
    set(TV_PATCH_PAD "${TV_VERSION_PATCH}")
endif()

# Internal ID: v0_0531  (used in file names, window title raw)
set(APPVERSION "v${TV_VERSION_MAJOR}_${TV_MINOR_PAD}${TV_PATCH_PAD}")

# Human-readable display: v0.05.31
set(APPVERSION_DISPLAY "v${TV_VERSION_MAJOR}.${TV_MINOR_PAD}.${TV_PATCH_PAD}")

# Full semver for Qt app metadata
set(TV_SEMVER
    "${TV_VERSION_MAJOR}.${TV_VERSION_MINOR}.${TV_VERSION_PATCH}")

project(TilVista VERSION "${TV_SEMVER}" LANGUAGES CXX)
message(STATUS "TilVista  VERSION=${APPVERSION_DISPLAY}")

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

# ── Version defines available in C++ ─────────────────────────────────────────
target_compile_definitions(TilVista PRIVATE
    TV_APPVERSION="${APPVERSION}"
    TV_APPVERSION_DISPLAY="${APPVERSION_DISPLAY}"
    TV_SEMVER="${TV_SEMVER}"
)

setup_qt_target(TilVista)
install(TARGETS TilVista DESTINATION bin)
