cmake_minimum_required(VERSION 3.20)

# ── Version ───────────────────────────────────────────────────────────────────
set(TV_VERSION_MAJOR  0)
set(TV_VERSION_MINOR  5)
set(TV_VERSION_PATCH 41)

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

set(APPVERSION         "v${TV_VERSION_MAJOR}_${TV_MINOR_PAD}${TV_PATCH_PAD}")
set(APPVERSION_DISPLAY "v${TV_VERSION_MAJOR}.${TV_MINOR_PAD}.${TV_PATCH_PAD}")
set(TV_SEMVER          "${TV_VERSION_MAJOR}.${TV_VERSION_MINOR}.${TV_VERSION_PATCH}")

project(TilVista VERSION "${TV_SEMVER}" LANGUAGES CXX)
message(STATUS "TilVista  APPVERSION=${APPVERSION}  display=${APPVERSION_DISPLAY}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG
    "${CMAKE_SOURCE_DIR}/build/debug_app")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE
    "${CMAKE_SOURCE_DIR}/build/${PROJECT_NAME}_${APPVERSION}")

# ── Qt path ───────────────────────────────────────────────────────────────────
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 QUIET   COMPONENTS Multimedia MultimediaWidgets)

if(Qt6Multimedia_FOUND)
    message(STATUS "[Qt] Multimedia found.")
else()
    message(STATUS "[Qt] Multimedia NOT found – ffmpeg fallback only.")
    add_compile_definitions(TV_NO_MULTIMEDIA)
endif()

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
        file(WRITE "${TV_RC}"
            "// Auto-generated – do not edit.\n"
            "IDI_ICON1 ICON \"${TV_ICO}\"\n")
        list(APPEND TV_SOURCES "${TV_RC}")
        message(STATUS "[Icon] Embedding ${TV_ICO}")
    else()
        message(STATUS "[Icon] tilvista.ico not found – no icon embedded.")
    endif()
endif()

add_executable(TilVista ${TV_SOURCES})
target_include_directories(TilVista PRIVATE src)

target_compile_definitions(TilVista PRIVATE
    TV_APPVERSION="${APPVERSION}"
    TV_APPVERSION_DISPLAY="${APPVERSION_DISPLAY}"
    TV_SEMVER="${TV_SEMVER}"
)

target_link_libraries(TilVista PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
if(Qt6Multimedia_FOUND)
    target_link_libraries(TilVista PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
endif()

if(WIN32)
    find_program(WINDEPLOYQT windeployqt6 HINTS "${CMAKE_PREFIX_PATH}/bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET TilVista POST_BUILD
            COMMAND "${WINDEPLOYQT}"
            ARGS    --no-translations --compiler-runtime
                    "$<TARGET_FILE:TilVista>"
            COMMENT "windeployqt6: deploying Qt DLLs…")
    endif()
    set_target_properties(TilVista PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
endif()

install(TARGETS TilVista DESTINATION bin)
