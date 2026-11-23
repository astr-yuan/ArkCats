cmake_minimum_required(VERSION 3.20)
project(KuroCat LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(KuroCat WIN32 src/main.cpp)
target_compile_definitions(KuroCat PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)

if(MSVC)
    target_sources(KuroCat PRIVATE app.manifest)
    set_property(TARGET KuroCat PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(KuroCat PRIVATE /utf-8 /EHsc /W3)
else()
    enable_language(RC)
    target_sources(KuroCat PRIVATE app.rc)
    set_property(SOURCE app.rc APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
    # Older windres builds cannot quote their inferred preprocessor under Program Files.
    if(CMAKE_HOST_WIN32 AND CMAKE_RC_COMPILER MATCHES " ")
        set_property(SOURCE app.rc APPEND PROPERTY COMPILE_OPTIONS
            "--preprocessor=\"${CMAKE_CXX_COMPILER}\"" "--preprocessor-arg=-E"
            "--preprocessor-arg=-xc" "--preprocessor-arg=-DRC_INVOKED")
    endif()
    target_link_options(KuroCat PRIVATE -municode -static)
endif()

target_link_libraries(KuroCat PRIVATE user32 gdi32 gdiplus shell32 advapi32)
