# Framework-independent DSP tests (no JUCE required).
add_executable(dsp_tests dsp_tests.cpp)
target_link_libraries(dsp_tests PRIVATE octa_dsp)
target_compile_options(dsp_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

add_test(NAME dsp_tests COMMAND dsp_tests)
