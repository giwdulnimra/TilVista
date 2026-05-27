cmake_minimum_required(VERSION 3.20)
project(TilVista VERSION 0.5.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include("${CMAKE_SOURCE_DIR}/SetupQt.cmake")

set(TV_SOURCES
    src/main.cpp

    # core
    src/core/pathutils.h        src/core/pathutils.cpp
    src/core/config.h           src/core/config.cpp

    # workers
    src/workers/scanworker.h    src/workers/scanworker.cpp
    src/workers/dbworkers.h     src/workers/dbworkers.cpp
    src/workers/catalogueworkers.h src/workers/catalogueworkers.cpp
    src/workers/videoframesworker.h src/workers/videoframesworker.cpp

    # ui
    src/ui/previewlabel.h       src/ui/previewlabel.cpp
    src/ui/videopreviewwidget.h src/ui/videopreviewwidget.cpp
    src/ui/dirbar.h             src/ui/dirbar.cpp
    src/ui/dirdatabasepanel.h   src/ui/dirdatabasepanel.cpp
    src/ui/reviewdbpanel.h      src/ui/reviewdbpanel.cpp
    src/ui/aleavuetab.h         src/ui/aleavuetab.cpp
    src/ui/sattumapictab.h      src/ui/sattumapictab.cpp
    src/ui/slideshowwindow.h    src/ui/slideshowwindow.cpp
    src/ui/mainwindow.h         src/ui/mainwindow.cpp

    resources/resources.qrc
)

add_executable(TilVista ${TV_SOURCES})
target_include_directories(TilVista PRIVATE src)
setup_qt_target(TilVista)
install(TARGETS TilVista DESTINATION bin)
