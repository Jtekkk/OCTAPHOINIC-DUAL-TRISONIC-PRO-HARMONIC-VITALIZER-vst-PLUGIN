cmake_minimum_required(VERSION 3.21)

# JUCE requires C (alongside C++) to be enabled for its modules and juceaide.
project(OctaphonicVitalizer VERSION 1.0.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
#  Options
# ---------------------------------------------------------------------------
option(OCTA_BUILD_PLUGIN "Build the JUCE VST3/AU/Standalone plugin (fetches JUCE)" ON)
option(OCTA_BUILD_TESTS  "Build the framework-independent DSP unit tests"          ON)
set(OCTA_JUCE_TAG "8.0.13" CACHE STRING "JUCE git tag to fetch when building the plugin")
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (skips the network fetch)")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
#  DSP core  --  pure C++17, no JUCE. Shared by the plugin and the tests.
# ---------------------------------------------------------------------------
add_library(octa_dsp STATIC
    src/dsp/OctaphonicEngine.cpp)
target_include_directories(octa_dsp PUBLIC src/dsp)
target_compile_features(octa_dsp PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
if(OCTA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
#  Plugin (JUCE)
# ---------------------------------------------------------------------------
if(OCTA_BUILD_PLUGIN)
    include(FetchContent)

    if(JUCE_PATH)
        message(STATUS "Using local JUCE at ${JUCE_PATH}")
        add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        message(STATUS "Fetching JUCE ${OCTA_JUCE_TAG} ...")
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${OCTA_JUCE_TAG}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(OCTA_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND OCTA_FORMATS AU)
    endif()

    juce_add_plugin(OctaphonicVitalizer
        PLUGIN_MANUFACTURER_CODE Octa
        PLUGIN_CODE Ovtz
        FORMATS ${OCTA_FORMATS}
        PRODUCT_NAME "OCTAPHONIC Vitalizer"
        COMPANY_NAME "OCTAPHONIC Audio"
        BUNDLE_ID com.octaphonic.vitalizer
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect)

    target_sources(OctaphonicVitalizer PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp)

    target_compile_definitions(OctaphonicVitalizer PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

    target_link_libraries(OctaphonicVitalizer
        PRIVATE
            octa_dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)

    # Warn at the same level as the DSP tests. (JUCE's recommended warning set
    # adds very pedantic flags such as -Wsign-conversion / -Wfloat-equal that
    # would otherwise force noisy casts throughout the tight DSP loops.)
    target_compile_options(OctaphonicVitalizer PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endif()
