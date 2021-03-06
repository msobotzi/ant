# - Finds ROOT instalation
# This module sets up ROOT information
# It defines:
# ROOT_FOUND          If the ROOT is found
# ROOT_INCLUDE_DIR    PATH to the include directory
# ROOT_INCLUDE_DIRS   PATH to the include directories (not cached)
# ROOT_LIBRARIES      Most common libraries
# ROOT_<name>_LIBRARY Full path to the library <name>
# ROOT_LIBRARY_DIR    PATH to the library directory
# ROOT_DEFINITIONS    Compiler definitions and flags
#
# Updated by K. Smith (ksmith37@nd.edu) to properly handle
#  dependencies in ROOT_GENERATE_DICTIONARY

find_program(ROOT_CONFIG_EXECUTABLE root-config
  PATHS
  $ENV{ROOTSYS}/bin
  $ENV{HOME}/opt/root/bin
  /opt/root/bin
  /cern/root/bin
  /etc/root/bin
)

execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --prefix
    OUTPUT_VARIABLE ROOTSYS
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --version
    OUTPUT_VARIABLE ROOT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --incdir
    OUTPUT_VARIABLE ROOT_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(ROOT_INCLUDE_DIRS ${ROOT_INCLUDE_DIR})

execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --libdir
    OUTPUT_VARIABLE ROOT_LIBRARY_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(ROOT_LIBRARY_DIRS ${ROOT_LIBRARY_DIR})

set(rootlibs Core Cint RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread Gui HistPainter)
set(ROOT_LIBRARIES)
foreach(_cpt ${rootlibs} ${ROOT_FIND_COMPONENTS})
  find_library(ROOT_${_cpt}_LIBRARY ${_cpt} HINTS ${ROOT_LIBRARY_DIR})
  if(ROOT_${_cpt}_LIBRARY)
    mark_as_advanced(ROOT_${_cpt}_LIBRARY)
    list(APPEND ROOT_LIBRARIES ${ROOT_${_cpt}_LIBRARY})
    list(REMOVE_ITEM ROOT_FIND_COMPONENTS ${_cpt})
  endif()
endforeach()
list(REMOVE_DUPLICATES ROOT_LIBRARIES)

execute_process(
    COMMAND ${ROOT_CONFIG_EXECUTABLE} --cflags
    OUTPUT_VARIABLE ROOT_DEFINITIONS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "(^|[ ]*)-I[^ ]*" "" ROOT_DEFINITIONS ${ROOT_DEFINITIONS})

execute_process(
  COMMAND ${ROOT_CONFIG_EXECUTABLE} --features
  OUTPUT_VARIABLE _root_options
  OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(_root_options)
foreach(_opt ${_root_options})
  set(ROOT_${_opt}_FOUND TRUE)
endforeach()

find_program(ROOT_CINT_EXECUTABLE rootcint PATHS ${ROOTSYS}/bin NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ROOT DEFAULT_MSG ROOT_CONFIG_EXECUTABLE
    ROOTSYS ROOT_VERSION ROOT_INCLUDE_DIR ROOT_LIBRARIES ROOT_LIBRARY_DIR ROOT_CINT_EXECUTABLE)

mark_as_advanced(ROOT_CONFIG_EXECUTABLE)
mark_as_advanced(ROOT_CINT_EXECUTABLE)

include(CMakeParseArguments)

# This generates a ROOT dictionary from a LinkDef file by using rootcint
function (ROOT_GENERATE_DICTIONARY HEADERS LINKDEF_FILE DICTFILE)

  # construct -I arguments
  get_property(includedirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY INCLUDE_DIRECTORIES)
  foreach(f ${includedirs})
    list(APPEND INCLUDE_DIRS_ARGS -I"${f}")
  endforeach()

  # construct -D arguments
  get_directory_property(DirDefs
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMPILE_DEFINITIONS)
  foreach(f ${DirDefs})
    list(APPEND DEF_ARGS -D${f})
  endforeach()


  # also add the outfile with extension .h
  get_filename_component(DICTFILEDIR ${DICTFILE} PATH)
  get_filename_component(DICTFILENAME_WE ${DICTFILE} NAME_WE)
  get_filename_component(DICTFILENAME ${DICTFILE} NAME)
  set(DICTFILES ${DICTFILE} "${DICTFILEDIR}/${DICTFILENAME_WE}.h")

  # and ensure the output directory exists
  file(MAKE_DIRECTORY ${DICTFILEDIR})

  # prepare rootcint command
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(LDPREFIX "LD_LIBRARY_PATH")
  elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
    set(LDPREFIX "DYLD_LIBRARY_PATH")
  else()
    message(FATAL_ERROR "Unsupported System for ROOT Dictionary generation")
  endif()


  add_custom_command(OUTPUT ${DICTFILES}
    COMMAND
    ${LDPREFIX}=${ROOT_LIBRARY_DIR}:$ENV{${LDPREFIX}}
    ROOTSYS=${ROOTSYS}
    ${ROOT_CINT_EXECUTABLE}
    -f "${DICTFILE}" -c -p ${INCLUDE_DIRS_ARGS} ${DEF_ARGS} ${HEADERS} "${LINKDEF_FILE}"
    DEPENDS ${HEADERS} ${LINKDEF_FILE}
    )

  # this little trick re-runs cmake if the LINKDEF_FILE was changed
  # this is needed since rootcint needs an up-to-date list of input files
  file(RELATIVE_PATH STAMP_FILE ${CMAKE_SOURCE_DIR} ${LINKDEF_FILE})
  string(REPLACE "/" "_" STAMP_FILE ${STAMP_FILE})
  set(STAMP_FILE "${CMAKE_BINARY_DIR}/cmake/stamps/${STAMP_FILE}.stamp")
  configure_file("${LINKDEF_FILE}" "${STAMP_FILE}" COPYONLY)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${STAMP_FILE}")

endfunction()
