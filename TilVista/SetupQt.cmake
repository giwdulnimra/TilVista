if(NOT QT_DIR)
    if(DEFINED ENV{QTDIR})
        set(QT_DIR "$ENV{QTDIR}")
        message(STATUS "[SetupQt] Qt from environment: ${QT_DIR}")
    else()
        set(QT_DIR "C:/Qt/6.11.0/mingw_64")
        message(STATUS "[SetupQt] Qt fallback: ${QT_DIR}")
    endif()
else()
    message(STATUS "[SetupQt] Qt from -DQT_DIR: ${QT_DIR}")
endif()

set(CMAKE_PREFIX_PATH "${QT_DIR}")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 QUIET COMPONENTS Multimedia MultimediaWidgets)

if(Qt6Multimedia_FOUND)
    message(STATUS "[SetupQt] Multimedia found – native video playback enabled.")
else()
    message(STATUS "[SetupQt] Multimedia NOT found – ffmpeg frame-cycling fallback only.")
    add_compile_definitions(TV_NO_MULTIMEDIA)
endif()

function(setup_qt_target target_name)
    set_target_properties(${target_name} PROPERTIES
        AUTOMOC ON AUTOUIC ON AUTORCC ON)
    target_link_libraries(${target_name} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    if(Qt6Multimedia_FOUND)
        target_link_libraries(${target_name} PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
    endif()
    if(WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt6 HINTS "${QT_DIR}/bin")
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                ARGS --no-translations --compiler-runtime
                     "$<TARGET_FILE:${target_name}>"
                COMMENT "windeployqt6 deploying Qt DLLs…")
        endif()
        set_target_properties(${target_name} PROPERTIES
            WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
    endif()
endfunction()
