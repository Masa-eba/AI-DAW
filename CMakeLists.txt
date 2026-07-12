cmake_minimum_required(VERSION 3.22)

project(MiniDAW
    VERSION 0.1.0
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found. Add JUCE as a Git submodule at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE "
        "before configuring this project.")
endif()

add_subdirectory(JUCE)

juce_add_gui_app(MiniDAW
    PRODUCT_NAME "Mini DAW"
    COMPANY_NAME "Masatoshi"
    BUNDLE_ID "com.masatoshi.minidaw")

target_sources(MiniDAW
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/AudioEngine.cpp)

target_include_directories(MiniDAW
    PRIVATE
        Source)

target_compile_definitions(MiniDAW
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(MiniDAW
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_options(MiniDAW
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic)
endif()

juce_generate_juce_header(MiniDAW)
