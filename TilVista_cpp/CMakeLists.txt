cmake_minimum_required(VERSION 3.20)
project(TilVista VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/build/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/release")

include("${CMAKE_SOURCE_DIR}/SetupQt.cmake")

# ── Sources ──────────────────────────────────────────────────────────────────
set(TV_SOURCES
    src/main.cpp

    src/core/pathutils.h
    src/core/pathutils.cpp

    src/workers/scanworker.h
    src/workers/scanworker.cpp
    src/workers/dbworkers.h
    src/workers/dbworkers.cpp
    src/workers/catalogueworkers.h
    src/workers/catalogueworkers.cpp
    src/workers/videoframesworker.h
    src/workers/videoframesworker.cpp

    src/ui/previewlabel.h
    src/ui/previewlabel.cpp
    src/ui/videopreviewwidget.h
    src/ui/videopreviewwidget.cpp
    src/ui/dirbar.h
    src/ui/dirbar.cpp
    src/ui/dirdatabasepanel.h
    src/ui/dirdatabasepanel.cpp
    src/ui/aleavuetab.h
    src/ui/aleavuetab.cpp
    src/ui/sattumapictab.h
    src/ui/sattumapictab.cpp
    src/ui/slideshowwindow.h
    src/ui/slideshowwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp

    resources/resources.qrc
)

add_executable(TilVista ${TV_SOURCES})

target_include_directories(TilVista PRIVATE src)

setup_qt_target(TilVista)
install(TARGETS TilVista DESTINATION bin)
