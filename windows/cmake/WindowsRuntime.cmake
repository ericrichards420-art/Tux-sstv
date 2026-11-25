cmake_minimum_required(VERSION 3.22)
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
find_program(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND NAMES objdump REQUIRED)
foreach(required STAGE QT_BIN HAMLIB_BIN)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "Missing ${required}")
  endif()
endforeach()
file(GLOB_RECURSE plugins "${STAGE}/*.dll")
set(programs "${STAGE}/tux-sstv.exe" "${STAGE}/tux-sstv-audio.exe" "${STAGE}/rigctl.exe")
if(EXISTS "${STAGE}/rigctld.exe")
  list(APPEND programs "${STAGE}/rigctld.exe")
endif()
file(GET_RUNTIME_DEPENDENCIES
  EXECUTABLES ${programs}
  LIBRARIES ${plugins}
  DIRECTORIES "${STAGE}" "${QT_BIN}" "${HAMLIB_BIN}"
  RESOLVED_DEPENDENCIES_VAR resolved
  UNRESOLVED_DEPENDENCIES_VAR unresolved
  PRE_EXCLUDE_REGEXES "^api-ms-win-.*" "^ext-ms-win-.*"
  POST_EXCLUDE_REGEXES [=[^[A-Za-z]:[/\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\]]=])
if(unresolved)
  message(FATAL_ERROR "Unresolved runtime DLLs: ${unresolved}")
endif()
foreach(library IN LISTS resolved)
  get_filename_component(directory "${library}" DIRECTORY)
  if(NOT directory STREQUAL STAGE)
    file(INSTALL "${library}" DESTINATION "${STAGE}" TYPE FILE)
  endif()
endforeach()
