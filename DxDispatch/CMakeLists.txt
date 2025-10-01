cmake_minimum_required(VERSION 3.19)
project(dxdispatch VERSION 0.18.1 LANGUAGES CXX)

# ==============================================================================
# External Libraries/Helpers
# ==============================================================================

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
include(cmake/helper_platform.cmake)

if(NOT TARGET_XBOX)
    # Statically link runtime library to avoid runtime dependency on Visual C++ redistributable.
    # On Xbox we deploy these dependencies for now.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
endif()

if(TARGET_WSL)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$\{ORIGIN\}")
endif()

include(cmake/gsl.cmake)
include(cmake/rapidjson.cmake)
include(cmake/flatbuffer.cmake)
include(cmake/fmt.cmake)
include(cmake/wil.cmake)
include(cmake/cxxopts.cmake)

if(NOT TARGET_XBOX)
    include(cmake/gtest.cmake)
endif()

include(cmake/wil.cmake)
add_wil_target(wil CACHE_PREFIX DXD)

include(cmake/gdk.cmake)
add_gdk_target(gdk CACHE_PREFIX DXD)
get_target_property(gdk_dxcompiler_path gdk DX_COMPILER_PATH)

include(cmake/pix.cmake)
add_pix_target(pix CACHE_PREFIX DXD)

include(cmake/dxcompiler.cmake)
add_dxcompiler_target(dxcompiler CACHE_PREFIX DXD GDK_DXCOMPILER_PATH ${gdk_dxcompiler_path})
get_target_property(dxcompiler_type dxcompiler DX_COMPONENT_CONFIG)

include(cmake/d3d12.cmake)
add_d3d12_target(d3d12 CACHE_PREFIX DXD)

include(cmake/directml.cmake)
add_directml_target(directml CACHE_PREFIX DXD)

include(cmake/onnxruntime.cmake)
add_onnxruntime_target(onnxruntime CACHE_PREFIX DXD)
get_target_property(onnxruntime_type onnxruntime DX_COMPONENT_CONFIG)

include(cmake/onnxruntime_extensions.cmake)
add_onnxruntime_extensions_target(onnxruntime_extensions CACHE_PREFIX DXD)

# ==============================================================================
# Global Defines for CMAKE
# ==============================================================================
if(MSVC)
    add_compile_options(/guard:cf /Qspectre /GS /sdl  /W3 )    
    if( TARGET_ARCH STREQUAL ARM64 OR
        TARGET_ARCH STREQUAL ARM)
        add_link_options(/GUARD:CF /DYNAMICBASE)
    else()
        add_link_options(/GUARD:CF /DYNAMICBASE /CETCOMPAT)
    endif()
endif()

# ==============================================================================
# Model Library
# ==============================================================================
add_library(
    model STATIC 
    src/model/JsonParsers.cpp 
    src/model/JsonParsers.h
    src/model/Model.cpp
    src/model/Model.h
    src/model/NpyReaderWriter.cpp
    src/model/NpyReaderWriter.h
    src/model/ImageReaderWriter.cpp
    src/model/ImageReaderWriter.h
)

target_link_libraries(
    model 
    PRIVATE 
    fmt::fmt-header-only
    PUBLIC
    Microsoft.GSL::GSL
    rapidjson::rapidjson
    wil
    d3d12
    directml
)

if(TARGET_WSL)
    set_target_properties(model PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_compile_features(model PRIVATE cxx_std_17)
target_precompile_headers(model PRIVATE src/model/pch.h)
target_include_directories(model INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/model)
target_include_directories(model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/external)

# ==============================================================================
# Dll
# ==============================================================================
get_target_property(directml_config directml DX_COMPONENT_CONFIG)
get_target_property(d3d12_config d3d12 DX_COMPONENT_CONFIG)
get_target_property(dxcompiler_config dxcompiler DX_COMPONENT_CONFIG)
get_target_property(pix_config pix DX_COMPONENT_CONFIG)
get_target_property(gdk_config gdk DX_COMPONENT_CONFIG)
get_target_property(ort_config onnxruntime DX_COMPONENT_CONFIG)
get_target_property(ort_extensions_config onnxruntime_extensions DX_COMPONENT_CONFIG)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch/config.h.in config.h)

add_library(
    dxdispatchImpl SHARED
    src/dxdispatch/DxDispatchInterface.h
    src/dxdispatch/dllMain.cpp
    src/dxdispatch/dxDispatchWrapper.cpp
    src/dxdispatch/dxDispatchWrapper.h
    src/dxdispatch/Adapter.cpp
    src/dxdispatch/Adapter.h
    src/dxdispatch/Device.cpp
    src/dxdispatch/Device.h
    src/dxdispatch/Executor.cpp
    src/dxdispatch/Executor.h
    src/dxdispatch/CommandLineArgs.cpp
    src/dxdispatch/CommandLineArgs.h
    src/dxdispatch/Logging.cpp
    src/dxdispatch/Logging.h
    src/dxdispatch/PixCaptureHelper.cpp
    src/dxdispatch/PixCaptureHelper.h
    src/dxdispatch/DxModules.cpp
    src/dxdispatch/DxModules.h
    src/dxdispatch/ModuleInfo.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)
target_include_directories(dxdispatchImpl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch)

if(NOT dxcompiler_type STREQUAL None)
    target_sources(dxdispatchImpl PRIVATE src/dxdispatch/HlslDispatchable.cpp)
endif()

# ONNX and DirectML dispatchable paths removed (HLSL only build).

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT dxdispatch)

if(WIN32)
    if(TARGET_WINDOWS)
        target_compile_definitions(dxdispatchImpl PRIVATE INCLUDE_DXGI=1)
        target_link_libraries(dxdispatchImpl PRIVATE dxgi)
    endif()
    target_sources(dxdispatchImpl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dxdispatchImpl.rc)
    target_link_libraries(dxdispatchImpl PRIVATE version.lib)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch/dxdispatchImpl.rc.in dxdispatchImpl.rc)
endif()

if(TARGET_WSL)
    target_link_libraries(dxdispatchImpl PRIVATE -ldl)
endif()

target_include_directories(dxdispatchImpl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(
    dxdispatchImpl
    PRIVATE 
    Microsoft.GSL::GSL
    fmt::fmt-header-only
    model
    cxxopts
    directml
    d3d12
    dxcompiler
    pix
    gdk
    wil
    flatbuffer
)

target_compile_features(dxdispatchImpl PRIVATE cxx_std_17)
target_precompile_headers(dxdispatchImpl PRIVATE src/dxdispatch/pch.h)
target_copy_redist_dependencies(dxdispatchImpl)

if(MSVC)
    # Include the import definition, add VS debugger visualizations, remove unused functions, and fold identical code segments.
    set_target_properties(dxdispatchImpl PROPERTIES LINK_FLAGS "/DEF:${PROJECT_SOURCE_DIR}/src/dxdispatch/dxdispatch.def ")
endif()

set_target_properties(dxdispatchImpl PROPERTIES PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch/DxDispatchInterface.h)

install(TARGETS dxdispatchImpl
    RUNTIME DESTINATION bin 
    LIBRARY DESTINATION bin
    PUBLIC_HEADER DESTINATION bin)
install(FILES $<TARGET_LINKER_FILE:dxdispatchImpl> 
    DESTINATION bin
    OPTIONAL)
if(WIN32)
    install(FILES $<TARGET_PDB_FILE:dxdispatchImpl>
        DESTINATION bin
        OPTIONAL)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/models DESTINATION bin)
install(
    FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdPartyNotices.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/Guide.md
    DESTINATION bin
)

if(WIN32)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/tools/AnalyzePixGpuCapture.ps1 DESTINATION bin/tools)
endif()

if(TARGET_XBOX)
    # Deploy to the console instead of running on local machine.
    set_property(TARGET dxdispatchImpl PROPERTY VS_SOLUTION_DEPLOY ON)

    # No need for logos/assets; this is a developer-only app.
    target_sources(dxdispatchImpl PRIVATE ${CMAKE_BINARY_DIR}/MicrosoftGame.config )
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch/dxdispatch.config.in MicrosoftGame.config)
    set_source_files_properties(${CMAKE_BINARY_DIR}/MicrosoftGame.config PROPERTIES VS_TOOL_OVERRIDE "MGCCompile")

    # Copy MSVC/UCRT redist files matching the current toolset to the build output directory. Not all of the DLLs
    # are necessary but this approach is simple and should avoid missing dependencies.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    # set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    # set(CMAKE_INSTALL_DEBUG_LIBRARIES_ONLY TRUE)
    # set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    add_custom_command(
        TARGET dxdispatchImpl 
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} $<TARGET_FILE_DIR:dxdispatchImpl>
    )

    # For Copy models to the deployment directory even if other targets don't need to be built.
    add_custom_target(
        copy_models ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/models" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_VS_PLATFORM_NAME}/Layout/Image/Loose/models"
    )
endif()

# ==============================================================================
# Main Executable
# ==============================================================================
add_executable(
    dxdispatch
    src/exe/main.cpp
)

target_include_directories(
    dxdispatch 
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(
    dxdispatch
    PRIVATE 
    wil
    dxdispatchImpl
    d3d12
    directml
)

target_compile_features(dxdispatch PRIVATE cxx_std_17)

if(WIN32)
    target_sources(dxdispatch PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dxdispatch.rc)
    target_link_libraries(dxdispatch PRIVATE version.lib)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/exe/dxdispatch.rc.in dxdispatch.rc)
endif()

target_copy_redist_dependencies(dxdispatch)
install(
    FILES 
    $<TARGET_FILE:dxdispatch>
    DESTINATION bin
)
if(WIN32)
    install(FILES $<TARGET_PDB_FILE:dxdispatch>
        DESTINATION bin
        OPTIONAL)
endif()

# ==============================================================================
# Tests
# ==============================================================================
if((TARGET_WINDOWS OR TARGET_WSL) AND (TARGET_ARCH MATCHES "^X64|X86$"))
    option(DXD_TESTS "Build DxDispatch tests" ON)
else()
    option(DXD_TESTS "Build DxDispatch tests" OFF)
endif()

if(DXD_TESTS)
    enable_testing()
    include(GoogleTest)

    add_executable(
        jsontests 
        src/test/JsonParserTests.cpp
    )

    target_compile_features(jsontests PRIVATE cxx_std_17)
    target_link_libraries(
        jsontests 
        PRIVATE 
        gtest_main 
        fmt::fmt-header-only
        directml
        d3d12
        wil
        model
    )
    target_include_directories(jsontests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/dxdispatch)
    gtest_discover_tests(jsontests DISCOVERY_MODE PRE_TEST)

    # Hacky. Needed for silly reasons related to defining GUIDs in winadapter. This should
    # ideally be cleaned up at some point.
    if(NOT WIN32)
        add_dependencies(jsontests dxdispatch)
    endif()

    function(model_test model_name expected_output)
        add_test(NAME test_${model_name} COMMAND dxdispatch models/${model_name}.json WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(test_${model_name} PROPERTIES PASS_REGULAR_EXPRESSION ${expected_output})
    endfunction()

    if(NOT dxcompiler_type STREQUAL None)
        model_test(hlsl_add_fp32 "Resource 'Out': 2, 7, 6, 11, 2, 7")
    endif()
endif()

# Packaging - these commands allow CPack to bundle up the installed files of this project.
if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_NAME Microsoft.AI.DirectML.DxDispatch.${TARGET_PLATFORM}.${TARGET_ARCH})
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR DirectML)
    include(CPack)
endif()