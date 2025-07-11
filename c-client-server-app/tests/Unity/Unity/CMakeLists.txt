###################################################################################
#                                                                                 #
# NAME: CMakeLists.txt                                                            #
#                                                                                 #
# AUTHOR: Mike Karlesky, Mark VanderVoord, Greg Williams.                         #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################

cmake_minimum_required(VERSION 3.12)

# Read src/unity.h file and get project version from it
set(UNITY_HEADER "src/unity.h")

file(STRINGS "${UNITY_HEADER}" UNITY_HEADER_CONTENT 
    REGEX "^#define UNITY_VERSION_(MAJOR|MINOR|BUILD) +[0-9]+$"
)

set(UNITY_HEADER_VERSION_MAJOR 0)
set(UNITY_HEADER_VERSION_MINOR 0)
set(UNITY_HEADER_VERSION_BUILD 0)

foreach(VERSION_LINE IN LISTS UNITY_HEADER_CONTENT)
    foreach(VERSION_PART MAJOR MINOR BUILD)
        string(CONCAT REGEX_STRING "#define UNITY_VERSION_" 
                                   "${VERSION_PART}" 
                                   " +([0-9]+)"
        )

        if(VERSION_LINE MATCHES "${REGEX_STRING}")
            set(UNITY_HEADER_VERSION_${VERSION_PART} "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endforeach()

project(unity
    VERSION     ${UNITY_HEADER_VERSION_MAJOR}.${UNITY_HEADER_VERSION_MINOR}.${UNITY_HEADER_VERSION_BUILD}
    LANGUAGES   C
    DESCRIPTION "C Unit testing framework."
)

# Options to Build With Extras -------------------------------------------------
option(UNITY_EXTENSION_FIXTURE "Compiles Unity with the \"fixture\" extension." OFF)
option(UNITY_EXTENSION_MEMORY "Compiles Unity with the \"memory\" extension." OFF)

set(UNITY_EXTENSION_FIXTURE_ENABLED $<BOOL:${UNITY_EXTENSION_FIXTURE}>)
set(UNITY_EXTENSION_MEMORY_ENABLED $<OR:${UNITY_EXTENSION_FIXTURE_ENABLED},$<BOOL:${UNITY_EXTENSION_MEMORY}>>)

if(${UNITY_EXTENSION_FIXTURE})
    message(STATUS "Unity: Building with the fixture extension.")
endif()

if(${UNITY_EXTENSION_MEMORY})
    message(STATUS "Unity: Building with the memory extension.")
endif()

# Main target ------------------------------------------------------------------
add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::framework ALIAS ${PROJECT_NAME})

# Includes ---------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/unity.c
        $<$<BOOL:${UNITY_EXTENSION_FIXTURE_ENABLED}>:extras/fixture/src/unity_fixture.c>
        $<$<BOOL:${UNITY_EXTENSION_MEMORY_ENABLED}>:extras/memory/src/unity_memory.c>
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<BUILD_INTERFACE:$<$<BOOL:${UNITY_EXTENSION_MEMORY_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/extras/memory/src>>
        $<BUILD_INTERFACE:$<$<BOOL:${UNITY_EXTENSION_FIXTURE_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/extras/fixture/src>>
)

set(${PROJECT_NAME}_PUBLIC_HEADERS
        src/unity.h
        src/unity_internals.h
        $<$<BOOL:${UNITY_EXTENSION_FIXTURE_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/extras/fixture/src/unity_fixture.h>
        $<$<BOOL:${UNITY_EXTENSION_FIXTURE_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/extras/fixture/src/unity_fixture_internals.h>
        $<$<BOOL:${UNITY_EXTENSION_MEMORY_ENABLED}>:${CMAKE_CURRENT_SOURCE_DIR}/extras/memory/src/unity_memory.h>
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        C_STANDARD          11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF
        PUBLIC_HEADER       "${${PROJECT_NAME}_PUBLIC_HEADERS}"
        EXPORT_NAME         framework
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        # Clang
        $<$<C_COMPILER_ID:Clang>:
            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wexit-time-destructors                            
            -Wglobal-constructors
            -Wmissing-noreturn
            -Wmissing-prototypes
            -Wno-missing-braces
            -Wold-style-cast
            -Wshadow
            -Wweak-vtables
            -Werror
            -Wall
            $<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8.0.0>:-Wextra-semi-stmt>
        >
        
        # GCC
        $<$<C_COMPILER_ID:GNU>:
            -Waddress
            -Waggregate-return
            -Wformat-nonliteral
            -Wformat-security
            -Wformat
            -Winit-self
            -Wmissing-declarations
            -Wmissing-include-dirs
            -Wno-multichar
            -Wno-parentheses
            -Wno-type-limits
            -Wno-unused-parameter
            -Wunreachable-code
            -Wwrite-strings
            -Wpointer-arith
            -Werror
            -Wall
        >

        # MSVC
        $<$<C_COMPILER_ID:MSVC>:
            /Wall
        >
)

write_basic_package_version_file(${PROJECT_NAME}ConfigVersion.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion 
)

## Target installation
install(TARGETS   ${PROJECT_NAME}
    EXPORT        ${PROJECT_NAME}Targets
    ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    COMPONENT     library
)

## Target's cmake files: targets export
install(EXPORT  ${PROJECT_NAME}Targets
    NAMESPACE   ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

## Target's cmake files: config and version config for find_package()
install(FILES   ${PROJECT_NAME}Config.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
