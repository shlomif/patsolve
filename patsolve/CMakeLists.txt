CMAKE_MINIMUM_REQUIRED (VERSION 3.15)

PROJECT(patsolve C)

INCLUDE ("${CMAKE_SOURCE_DIR}/cmake/shlomif_common_bootstrap.cmake")
SHLOMIF_COMMON_SETUP("${private_mod_path}")

INCLUDE(CheckCCompilerFlag)
INCLUDE(FindPkgConfig)

SET (STATES_TYPE "COMPACT_STATES" CACHE STRING "States Type ('INDIRECT_STACK_STATES', or 'COMPACT_STATES'). No need to usually change.")
SET (FCS_WITH_TEST_SUITE "1" CACHE BOOL "Also build and run the test suite.")
SET (FCS_HARD_CODED_NUM_FCS_FOR_FREECELL_ONLY "4" CACHE STRING "The hard-coded number of freecells (4, 2, etc.). Usually ignored")
SET (FCS_FREECELL_ONLY "" CACHE BOOL "Configure Freecell Solver to only be able to solve Freecell (not recommended)")
SET (MAX_NUM_FREECELLS 8 CACHE STRING "Maximal Number of Freecells")
SET (MAX_NUM_STACKS 10 CACHE STRING "Maximal Number of Stacks")
SET (MAX_NUM_INITIAL_CARDS_IN_A_STACK 20 CACHE STRING
    "Maximal Number of Initial Cards in a Stack")
SET (MAX_NUM_DECKS 1 CACHE STRING "Maximal Number of Decks (must be 1)")
SET (FC_SOLVE_SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE STRING "Path to the Freecell Solver sources")

SET(AUTOGENERATED_CONFIG_H "config.h was auto-generated from config.h.in . Do not modify directly")
SET(AUTOGENERATED_BACK_COMPAT_H "fcs_back_compat.h was auto-generated from fcs_back_compat.h.in . Do not modify directly")

IF (NOT (STATES_TYPE MATCHES "^(INDIRECT_STACK_STATES|COMPACT_STATES)$"))
    MESSAGE(FATAL_ERROR "STATES_TYPE should be ('INDIRECT_STACK_STATES', or 'COMPACT_STATES')")
ENDIF()

SET ("${STATES_TYPE}" 1)

SHLOMIF_ADD_COMMON_C_FLAGS()
SHLOMIF_FINALIZE_FLAGS()

# This is the equivalent to perform a "make dist"/"make distdir" etc.
SET(CPACK_PACKAGE_NAME "patsolve-shlomif")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Patsolve - a solver for some Solitaire (Patience) variants")
SET(CPACK_PACKAGE_VENDOR "Shlomi Fish")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")

# Process and extract the version number.
FILE( READ "ver.txt" VERSION)

CHOMP (VERSION)

STRING (REGEX MATCHALL "([0-9]+)" VERSION_DIGITS "${VERSION}")

LIST(GET VERSION_DIGITS 0 CPACK_PACKAGE_VERSION_MAJOR)
LIST(GET VERSION_DIGITS 1 CPACK_PACKAGE_VERSION_MINOR)
LIST(GET VERSION_DIGITS 2 CPACK_PACKAGE_VERSION_PATCH)

SET(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY} ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

SET(CPACK_SOURCE_IGNORE_FILES
    "/.deps/"
    "/tags$"
    "\\\\.tidyall\\\\.d"
    "~$"
)

IF(WIN32 AND NOT UNIX)
    # There is a bug in NSI that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backlasshes.
    SET(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\\\dondorf-king.bmp")
    SET(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\fc-solve.exe")
    SET(CPACK_NSIS_HELP_LINK "http:\\\\\\\\fc-solve.berlios.de")
    SET(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\fc-solve.berlios.de")
    SET(CPACK_NSIS_DISPLAY_NAME "Patsolve")
    SET(CPACK_NSIS_CONTACT "shlomif@iglu.org.il")
    SET(CPACK_NSIS_MODIFY_PATH ON)
    # Setting for NSIS :
    SET(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\fc-solve.ico")
    SET(CPACK_NSIS_MUI_UNIICON ${CPACK_NSIS_MUI_ICON})
    SET(CPACK_PACKAGE_ICON ${CPACK_NSIS_MUI_ICON})
    SET(CPACK_NSIS_MODIFY_PATH "ON")
ELSE()
  SET(CPACK_STRIP_FILES "patsolve")
  SET(CPACK_SOURCE_STRIP_FILES "")
ENDIF()

SET(CPACK_PACKAGE_EXECUTABLES
    "patsolve" "Patsolve"
)

INCLUDE ("${CMAKE_SOURCE_DIR}/cmake/rinutils_bootstrap.cmake")

RINUTILS_SET_UP_FLAGS()

### This is to set the RPATH correctly, so when installed under a prefix
### the executables will find the libraries.
###
### See:
###
### http://www.cmake.org/Wiki/CMake_RPATH_handling
###
### (Taken from that wiki page)

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

INCLUDE(CPack)

EXECUTE_PROCESS(COMMAND "python3" "${CMAKE_CURRENT_SOURCE_DIR}/param.py" "${CMAKE_CURRENT_SOURCE_DIR}/param.dat")
SET (is_parent_gen "${FC_SOLVE_SRC_PATH}/scripts/gen-c-lookup-files.pl")
SET (is_parent_args "${is_parent_gen}")
SET (is_parent_output
    "${CMAKE_CURRENT_BINARY_DIR}/debondt__card_pos.c" "${CMAKE_CURRENT_BINARY_DIR}/debondt__card_pos.h"
    "${CMAKE_CURRENT_BINARY_DIR}/debondt__state_pos.c" "${CMAKE_CURRENT_BINARY_DIR}/debondt__state_pos.h"
    "${CMAKE_CURRENT_BINARY_DIR}/is_king.c" "${CMAKE_CURRENT_BINARY_DIR}/is_king.h"
    "${CMAKE_CURRENT_BINARY_DIR}/is_parent.c" "${CMAKE_CURRENT_BINARY_DIR}/is_parent.h"
    "${CMAKE_CURRENT_BINARY_DIR}/pos_by_rank__freecell.c" "${CMAKE_CURRENT_BINARY_DIR}/pos_by_rank__freecell.h"
    )
ADD_CUSTOM_COMMAND(
    OUTPUT ${is_parent_output}
    COMMAND "${PERL_EXECUTABLE}"
    ARGS ${is_parent_args}
    DEPENDS "${is_parent_gen}"
)

ADD_LIBRARY(fcs_patsolve_lib
    STATIC
    "${FC_SOLVE_SRC_PATH}/card.c"
    "${FC_SOLVE_SRC_PATH}/state.c"
    is_king.c is_king.h param.c pat.c patsolve.c tree.c
)

ADD_EXECUTABLE(patsolve patmain.c)

TARGET_LINK_LIBRARIES(patsolve fcs_patsolve_lib)

IF (UNIX)
    ADD_EXECUTABLE(threaded-pats threaded_main.c)

    TARGET_LINK_LIBRARIES(threaded-pats fcs_patsolve_lib "pthread" "m")
ENDIF()

ADD_EXECUTABLE(pats-msdeal
    msdeal.c
    )

SET(COMPILER_FLAGS_TO_CHECK "-Wall" "-Werror=implicit-function-declaration")

IF (CPU_ARCH)
    LIST(APPEND COMPILER_FLAGS_TO_CHECK "-march=${CPU_ARCH}")
ENDIF()

IF (OPTIMIZATION_OMIT_FRAME_POINTER)
    LIST(APPEND COMPILER_FLAGS_TO_CHECK "-fomit-frame-pointer")
ENDIF()

FOREACH (CFLAG_TO_CHECK ${COMPILER_FLAGS_TO_CHECK})
    CHECK_C_COMPILER_FLAG(${CFLAG_TO_CHECK} FLAG_EXISTS)
    IF ("${FLAG_EXISTS}")
        ADD_DEFINITIONS(${CFLAG_TO_CHECK})
    ENDIF ()
ENDFOREACH()

TARGET_LINK_LIBRARIES (patsolve "m")

IF (FCS_WITH_TEST_SUITE)

    ADD_CUSTOM_TARGET(
        "check"
        "perl" "${CMAKE_CURRENT_SOURCE_DIR}/run-tests.pl"
    )

# ADD_DEPENDENCIES(
# "check"
# "all"
# )

ENDIF()

CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/freecell-solver/config.h
    )

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/fcs_back_compat.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/freecell-solver/fcs_back_compat.h"
    )

SET_SOURCE_FILES_PROPERTIES( tags PROPERTIES GENERATED true)

ADD_CUSTOM_TARGET (
    tags
    COMMAND ctags -R .
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# So it can find config.h
INCLUDE_DIRECTORIES(BEFORE "${FC_SOLVE_SRC_PATH}/include" "${FC_SOLVE_SRC_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/patsolve-specific" "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_BINARY_DIR} )
