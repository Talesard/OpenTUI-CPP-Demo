cmake_minimum_required(VERSION 3.20)
project(opentui_cpp_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OTUI_VERSION "0.5.10" CACHE STRING "OpenTUI prebuilt package version")
set(OTUI_VENDOR_DIR "${CMAKE_SOURCE_DIR}/third_party/opentui")
set(OTUI_DLL "${OTUI_VENDOR_DIR}/opentui.dll")
set(OTUI_IMPLIB "${OTUI_VENDOR_DIR}/opentui.lib")

find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)

# ---------------------------------------------------------------------------
# Fetch opentui.dll if missing
# ---------------------------------------------------------------------------
if(NOT EXISTS "${OTUI_DLL}")
  execute_process(
    COMMAND "${POWERSHELL_EXECUTABLE}" -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/fetch_opentui.ps1"
            -Version "${OTUI_VERSION}"
            -OutDir "${OTUI_VENDOR_DIR}"
    RESULT_VARIABLE _otui_fetch_rc
  )
  if(NOT _otui_fetch_rc EQUAL 0)
    message(FATAL_ERROR "Failed to download OpenTUI DLL (exit ${_otui_fetch_rc})")
  endif()
endif()

if(NOT EXISTS "${OTUI_DLL}")
  message(FATAL_ERROR "opentui.dll not found at ${OTUI_DLL}")
endif()

# ---------------------------------------------------------------------------
# Generate import library from DLL exports (MSVC)
# ---------------------------------------------------------------------------
if(MSVC AND NOT EXISTS "${OTUI_IMPLIB}")
  execute_process(
    COMMAND "${POWERSHELL_EXECUTABLE}" -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/gen_implib.ps1"
            -Dll "${OTUI_DLL}"
            -OutDir "${OTUI_VENDOR_DIR}"
    RESULT_VARIABLE _otui_lib_rc
  )
  if(NOT _otui_lib_rc EQUAL 0)
    message(FATAL_ERROR
      "Failed to generate opentui.lib. Configure from a VS Developer environment "
      "so dumpbin/lib are on PATH (exit ${_otui_lib_rc}).")
  endif()
endif()

add_library(otui_app STATIC src/otui_app.cpp)
target_include_directories(otui_app PUBLIC "${CMAKE_SOURCE_DIR}/include")

function(otui_add_app target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE otui_app)
  if(MSVC)
    if(NOT EXISTS "${OTUI_IMPLIB}")
      message(FATAL_ERROR "opentui.lib missing at ${OTUI_IMPLIB}")
    endif()
    target_link_libraries(${target_name} PRIVATE "${OTUI_IMPLIB}")
    target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_link_libraries(${target_name} PRIVATE "${OTUI_DLL}")
  endif()
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OTUI_DLL}"
            "$<TARGET_FILE_DIR:${target_name}>/opentui.dll"
    COMMENT "Copy opentui.dll next to ${target_name}"
  )
endfunction()

otui_add_app(opentui_demo src/demo.cpp)
otui_add_app(opentui_procs src/procs.cpp)

if(WIN32)
  target_link_libraries(opentui_procs PRIVATE Psapi)
endif()

message(STATUS "OpenTUI DLL: ${OTUI_DLL}")
