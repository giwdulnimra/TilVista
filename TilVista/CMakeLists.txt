cmake_minimum_required(VERSION 3.20)
project(TilVista VERSION 0.5.41 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Version Padding ───────────────────────────────────────────────────────────
if(PROJECT_VERSION_MINOR LESS 10)
    set(DISP_MINOR "0${PROJECT_VERSION_MINOR}")
else()
    set(DISP_MINOR "${PROJECT_VERSION_MINOR}")
endif()
if(PROJECT_VERSION_PATCH LESS 10)
    set(DISP_PATCH "0${PROJECT_VERSION_PATCH}")
else()
    set(DISP_PATCH "${PROJECT_VERSION_PATCH}")
endif()

set(APPVERSION         "v${PROJECT_VERSION_MAJOR}_${DISP_MINOR}${DISP_PATCH}")
set(APPVERSION_DISPLAY "v${PROJECT_VERSION}")

message(STATUS "TilVista APPVERSION=${APPVERSION} display=${APPVERSION_DISPLAY}")

# ── SetupQt einbinden ─────────────────────────────────────────────────────────
include("${CMAKE_SOURCE_DIR}/SetupQt.cmake")

# ── Sources ───────────────────────────────────────────────────────────────────
set(TV_SOURCES
        src/main.cpp
        src/core/pathutils.h          src/core/pathutils.cpp
        src/core/config.h             src/core/config.cpp
        src/workers/scanworker.h      src/workers/scanworker.cpp
        src/workers/dbworkers.h       src/workers/dbworkers.cpp
        src/workers/catalogueworkers.h src/workers/catalogueworkers.cpp
        src/workers/videoframesworker.h src/workers/videoframesworker.cpp
        src/ui/previewlabel.h         src/ui/previewlabel.cpp
        src/ui/videopreviewwidget.h   src/ui/videopreviewwidget.cpp
        src/ui/dirbar.h               src/ui/dirbar.cpp
        src/ui/dirdatabasepanel.h     src/ui/dirdatabasepanel.cpp
        src/ui/shujukopanel.h         src/ui/shujukopanel.cpp
        src/ui/aleavuetab.h           src/ui/aleavuetab.cpp
        src/ui/sattumapictab.h        src/ui/sattumapictab.cpp
        src/ui/slideshowwindow.h      src/ui/slideshowwindow.cpp
        src/ui/madooverlay.h          src/ui/madooverlay.cpp
        src/ui/madolodostab.h         src/ui/madolodostab.cpp
        src/ui/shortcutstab.h         src/ui/shortcutstab.cpp
        src/ui/mainwindow.h           src/ui/mainwindow.cpp
        resources/resources.qrc
)

# ── Windows: embed icon into .exe ─────────────────────────────────────────────
if(WIN32)
    set(TV_ICO "${CMAKE_SOURCE_DIR}/resources/tilvista.ico")
    set(TV_RC  "${CMAKE_BINARY_DIR}/tilvista_app.rc")
    if(EXISTS "${TV_ICO}")
        file(WRITE "${TV_RC}" "// Auto-generated – do not edit.\nIDI_ICON1 ICON \"${TV_ICO}\"\n")
        list(APPEND TV_SOURCES "${TV_RC}")
        message(STATUS "[Icon] Embedding ${TV_ICO}")
    endif()
endif()

# ── Executable & Makros ───────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${TV_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        TV_APPVERSION="${APPVERSION}"
        TV_APPVERSION_DISPLAY="${APPVERSION_DISPLAY}"
        TV_SEMVER="${PROJECT_VERSION}"
)

# ── Target Konfiguration (Qt) ─────────────────────────────────────────────────
setup_qt_target(${PROJECT_NAME}) # Hier übernimmt SetupQt.cmake den Rest!

install(TARGETS ${PROJECT_NAME} DESTINATION bin)