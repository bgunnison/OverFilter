cmake_minimum_required(VERSION 3.20)
project(OverFilterVST3 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{VST3_SDK_ROOT} AND NOT VST3_SDK_ROOT)
  message(FATAL_ERROR "Set VST3_SDK_ROOT to the Steinberg VST3 SDK path.")
endif()

if(NOT VST3_SDK_ROOT)
  set(VST3_SDK_ROOT $ENV{VST3_SDK_ROOT})
endif()

file(TO_CMAKE_PATH "${VST3_SDK_ROOT}" VST3_SDK_ROOT)

list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")
include("${VST3_SDK_ROOT}/cmake/modules/SMTG_VST3_SDK.cmake")
include("${VST3_SDK_ROOT}/cmake/modules/SMTG_AddVST3Library.cmake")

set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_UTILITIES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)

set(OverFilter_SOURCES
    src/OverFilterEngine.cpp
    src/OverFilterProcessor.cpp
    src/OverFilterController.cpp
    src/OverFilterPluginFactory.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp
)

set(OverFilter_HEADERS
    src/OverFilterEngine.h
    src/OverFilterProcessor.h
    src/OverFilterController.h
    src/OverFilterIDs.h
    src/OverFilterParams.h
    src/dsp/FractionalDelay.h
    src/dsp/PeakLimiter.h
    src/dsp/TwoPoleResonator.h
)

smtg_add_vst3plugin(OverFilter
    PACKAGE_NAME "OverFilter"
    SOURCES_LIST ${OverFilter_SOURCES} ${OverFilter_HEADERS}
)

target_include_directories(OverFilter PRIVATE
    ${VST3_SDK_ROOT}
)

target_link_libraries(OverFilter PRIVATE
    sdk
    sdk_common
    base
    pluginterfaces
    vstgui_support
)

target_compile_definitions(OverFilter PRIVATE
    VSTGUI_LIVE_EDITING=1
    $<$<CONFIG:Debug>:OVERFILTER_DEBUG_NAME>
    $<$<CONFIG:Debug>:OVERFILTER_DEBUG_UIDS>
)

set_target_properties(OverFilter PROPERTIES
    OUTPUT_NAME "OverFilter"
    OUTPUT_NAME_DEBUG "DebugOverFilter"
    OUTPUT_NAME_RELEASE "OverFilter"
    OUTPUT_NAME_RELWITHDEBINFO "OverFilter"
    OUTPUT_NAME_MINSIZEREL "OverFilter"
)

set(OverFilter_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/overfilter.uidesc
    ${CMAKE_CURRENT_SOURCE_DIR}/logo_strip.png
)

smtg_target_add_plugin_resources(OverFilter
    RESOURCES
        ${OverFilter_RESOURCES}
)

# The SDK resource helper keys off OUTPUT_NAME_DEBUG on this local setup, while
# the actual debug bundle keeps PACKAGE_NAME. Copy resources into the real bundle
# as a post-build step so Debug loads with the same UI as Release.
add_custom_command(TARGET OverFilter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:OverFilter>/../Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/overfilter.uidesc
        "$<TARGET_FILE_DIR:OverFilter>/../Resources/overfilter.uidesc"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/logo_strip.png
        "$<TARGET_FILE_DIR:OverFilter>/../Resources/logo_strip.png"
    VERBATIM
)
