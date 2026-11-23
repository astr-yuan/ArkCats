cmake_minimum_required(VERSION 3.20)
project(ArkCats LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ArkCats WIN32 src/main.cpp)
target_compile_definitions(ArkCats PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
set(fox_resources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/FoxResourceIds.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/red.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/arctic.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/red-lids.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/arctic-lids.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/red-paw-backs.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/art/foxes/arctic-paw-backs.png")
target_include_directories(ArkCats PRIVATE "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_SOURCE_DIR}>")

if(MSVC)
    enable_language(RC)
    target_sources(ArkCats PRIVATE app.manifest version.rc foxes.rc)
    set_property(SOURCE foxes.rc APPEND PROPERTY OBJECT_DEPENDS "${fox_resources}")
    set_property(TARGET ArkCats PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(ArkCats PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/EHsc;/W3>")
else()
    enable_language(RC)
    # Old windres invokes its preprocessor through a shell and loses quotes in paths
    # with spaces. Supply a bare executable name and put the selected toolchain first.
    if(CMAKE_HOST_WIN32 AND CMAKE_RC_COMPILER MATCHES " ")
        get_filename_component(toolchain_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(preprocessor "${CMAKE_CXX_COMPILER}" NAME)
        set(resource_dir "${CMAKE_CURRENT_BINARY_DIR}/resources/$<CONFIG>")
        set(resource_object "${resource_dir}/app.res.o")
        add_custom_command(OUTPUT "${resource_object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${resource_dir}"
            COMMAND "${CMAKE_COMMAND}" -E env "PATH=${toolchain_dir};$ENV{PATH}" "${CMAKE_RC_COMPILER}"
                "--preprocessor=${preprocessor}" --preprocessor-arg=-E --preprocessor-arg=-xc
                --preprocessor-arg=-DRC_INVOKED -i app.rc -O coff -o "${resource_object}"
            DEPENDS app.rc app.manifest version.rc foxes.rc ${fox_resources}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
        target_sources(ArkCats PRIVATE "${resource_object}")
    else()
        target_sources(ArkCats PRIVATE app.rc)
        set_property(SOURCE app.rc APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest"
            "${CMAKE_CURRENT_SOURCE_DIR}/version.rc" "${CMAKE_CURRENT_SOURCE_DIR}/foxes.rc" "${fox_resources}")
    endif()
    target_link_options(ArkCats PRIVATE -municode -static)
endif()

target_link_libraries(ArkCats PRIVATE user32 gdi32 gdiplus shell32 advapi32 wtsapi32 ole32)

set(helper_dir "${CMAKE_CURRENT_BINARY_DIR}/notifications/$<CONFIG>")
set(helper_output "${helper_dir}/ArkCatsNotifications.exe")
add_custom_command(OUTPUT "${helper_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${helper_dir}"
    COMMAND cmd /d /c call "${CMAKE_CURRENT_SOURCE_DIR}/notifications/build.bat" "${helper_output}"
    DEPENDS notifications/build.bat notifications/Program.cs notifications/ParentWindow.cs
            notifications/SessionWindow.cs notifications/ToastSource.cs notifications/ToastTracker.cs
    VERBATIM)
add_custom_target(ArkCatsNotifications
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:ArkCats>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${helper_output}" "$<TARGET_FILE_DIR:ArkCats>/ArkCatsNotifications.exe"
    DEPENDS "${helper_output}" VERBATIM)
add_dependencies(ArkCats ArkCatsNotifications)
