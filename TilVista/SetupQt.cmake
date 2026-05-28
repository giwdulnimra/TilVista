set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/build/debug_app_${APPVERSION})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/build/${PROJECT_NAME}_${APPVERSION})

set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")

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
