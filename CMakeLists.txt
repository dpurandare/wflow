cmake_minimum_required(VERSION 3.20)
project(wflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WFLOW_USE_WHISPERCPP "Enable local Whisper transcription via whisper.cpp" OFF)

if(WFLOW_USE_WHISPERCPP)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp
        GIT_TAG v1.9.1
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(whisper)
endif()

add_executable(wflow WIN32
    src/main.cpp
    src/app/App.cpp
    src/app/resources.rc
    src/audio/AudioCapture.cpp
    src/stt/Transcriber.cpp
    src/stt/ModelBootstrap.cpp
    src/injection/TextInjector.cpp
    src/platform/TrayIcon.cpp
    src/platform/HotkeyManager.cpp
    src/platform/StartupManager.cpp
    src/platform/SettingsWindow.cpp
    src/config/SettingsStore.cpp
)

target_include_directories(wflow PRIVATE src)

target_compile_definitions(wflow PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(wflow PRIVATE WFLOW_USE_WHISPERCPP=$<BOOL:${WFLOW_USE_WHISPERCPP}>)

target_compile_options(wflow PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(wflow PRIVATE user32 shell32 ole32 comdlg32 wininet)

if(WFLOW_USE_WHISPERCPP)
    target_link_libraries(wflow PRIVATE whisper)
endif()
