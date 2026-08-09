cmake_minimum_required(VERSION 3.22)

project(SC220WasapiMonitorSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(JUCE)

set(SC220_PC_CAPTURE_SOURCE
    "${CMAKE_CURRENT_LIST_DIR}/../../live/PcAudioLoopbackCapture.cpp")
set(SC220_PC_CAPTURE_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PcAudioLoopbackCapture_Recovery.cpp")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/GenerateSC220PcCaptureRecovery.cmake")

add_executable(SC220WasapiMonitorSmoke
    main.cpp
    ../../live/MonitorOutputClient.cpp
    "${SC220_PC_CAPTURE_GENERATED_SOURCE}"
)

target_include_directories(SC220WasapiMonitorSmoke PRIVATE
    ../../live
)

target_compile_definitions(SC220WasapiMonitorSmoke PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(SC220WasapiMonitorSmoke PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

if(WIN32)
    target_link_libraries(SC220WasapiMonitorSmoke PRIVATE
        ole32
        propsys
        uuid
    )
endif()
