cmake_minimum_required(VERSION 3.8)

# Better behavior of VS generator.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# Allow to specify VERSION in project()
cmake_policy(SET CMP0048 NEW)

# Will be used later.
#include(${PROJECT_SOURCE_DIR}/cmake/version_detect.cmake)
set(HEXICORD_VERSION 0.0.0)

project(Hexicord VERSION ${HEXICORD_VERSION})

#------------------------------------------------------------------------------
# Global building configuration (C++ standard, defines, warning flags, cotire).

add_definitions(-DHEXICORD_VERSION="${HEXICORD_VERSION}" 
                -DHEXICORD_GITHUB="https://github.com/HexwellC/Hexicord")

# Generate compilation database for YouCompleteMe and stuff like clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Obviously we need C++11 for entire source tree. So setting it here.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow some tweaks in build process without changing CMakeLists.
option(HEXICORD_WERROR  "Build with -Werror flag."                 OFF)
option(HEXICORD_WSTRICT "Build with really strict warning flags."  OFF)

# Check if used compiler supports warning flags we use.
if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR
   ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR
   ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang"))
    # I'm not sure about differences between LLVM's Clang and Apple one.

    if (HEXICORD_WSTRICT)
        set(WARNING_FLAGS ${WARNING_FLAGS} -Wall -Wextra -pedantic -Weffc++
                -Wcast-align -Wcast-qual -Wno-ctor-dtor-privacy
                -Wdisabled-optimization -Wformat=2 -Winit-self
                -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast
                -Woverloaded-virtual -Wredundant-decls -Wshadow
                -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch
                -Wundef -Wno-unused -Wnon-virtual-dtor -Wreorder -Wdeprecated
                -Wno-float-equal
                )
    endif()

    if(HEXICORD_WERROR)
        set(WARNING_FLAGS ${WARNING_FLAGS} -Werror)
    endif()
else()
    message(WARNING "Using WERROR/WSTRICT with non-GCC like compiler is not supported. Ignoring.")
endif()

#------------------------------------------------------------------------------
# Documentation (Doxygen)

find_package(Doxygen)
option(HEXICORD_DOCS    "Generate HTML API documentation (Doxygen)"    OFF)
option(HEXICORD_IDOCS   "Generate documentation for internal classes." OFF)
if(HEXICORD_DOCS)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is required to build documentation.")
        # Don't have to put everything else in else branch because FATAL_ERROR aborts configuration.
    endif()

    if(HEXICORD_IDOCS)
        set(_INTERNAL_DOCS "YES")
    else()
        set(_INTERNAL_DOCS "NO")
    endif()

    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(doc
                      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
                      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                      COMMENT "Generating HTML API documentation using Doxygen"
                      VERBATIM
                      )

    install(DIRECTORY ${PROJECT_BINARY_DIR}/html DESTINATION share/doc)
endif()


#------------------------------------------------------------------------------
# Boost

set(Boost_USE_STATIC_LIBS    ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost REQUIRED COMPONENTS system)
link_directories(${Boost_LIBRARY_DIRS})
include_directories(${Boost_INCLIDE_DIRS})

if(MINGW)
    link_libraries(ws2_32 mswsock)
endif()


#------------------------------------------------------------------------------
# OpenSSL

find_package(OpenSSL REQUIRED)
link_directories(${OpenSSL_LIBRARY_DIRS})
include_directories(${OpenSSL_INCLUDE_DIRS})


#------------------------------------------------------------------------------
# POSIX Threads

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


#------------------------------------------------------------------------------
# Beast

#set(Beast_BUILD_EXAMPLES OFF)
#set(Beast_BUILD_TESTS    OFF)

# Doesn't required actually. All dependencies of beast handled by this
# CMakeLists.txt. Also inclusion of beast's CMakeLists forces us to build
# tests and examples (painful and slow due to high memory usage of compiler 
# when parsing dense templates (up to 700 MiB)). set's above seems to be 
# ignored.
#add_subdirectory(${PROJECT_SOURCE_DIR}/lib/beast)
include_directories(${PROJECT_SOURCE_DIR}/lib/beast/include)


#------------------------------------------------------------------------------
# nlohmann/json

include_directories(${PROJECT_SOURCE_DIR}/lib)

#------------------------------------------------------------------------------
# Library

option(STATIC_HEXICORD   "Build static library." ON )
option(SHARED_HEXICORD   "Build shared library." OFF)
option(HEXICORD_EXAMPLES "Build examples."       OFF)

file(GLOB_RECURSE HEXICORD_HEADERS ${PROJECT_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE HEXICORD_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

include_directories(hexicord ${PROJECT_SOURCE_DIR}/src)

if(STATIC_HEXICORD)
    add_library(hexicord STATIC ${HEXICORD_HEADERS} ${HEXICORD_SOURCES})
endif()

if(SHARED_HEXICORD)
    add_library(hexicord SHARED ${HEXICORD_HEADERS} ${HEXICORD_SOURCES})
endif()

target_link_libraries(hexicord Beast Threads::Threads Boost::system)
target_compile_options(hexicord PRIVATE ${WARNING_FLAGS})

# XXX: Look into path relocation for headers.
#install(FILES ${HEXICORD_HEADERS} DESTINATION include/)
install(TARGETS hexicord 
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

if(HEXICORD_EXAMPLES)
    add_subdirectory(${PROJECT_SOURCE_DIR}/examples)
endif()
