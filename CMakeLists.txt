cmake_minimum_required(VERSION 3.1)

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
option(HEXICORD_DOCS    "Generate HTML documentation (Doxygen)"    ${DOXYGEN_FOUND})
if(HEXICORD_DOCS)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is required to build documentation.")
        # Don't have to put everything else in else branch because FATAL_ERROR aborts configuration.
    endif()

    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)

    add_custom_target(doc ALL
                      COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
                      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
                      COMMENT "Generating HTML API documentation using Doxygen"
                      SOURCES ${PROJECT_SOURCE_DIR}/Doxyfile.in 
                      BYPRODUCTS ${PROJECT_BINARY_DIR}/docs 
                      VERBATIM
                      )

    install(DIRECTORY ${PROJECT_BINARY_DIR}/docs/ DESTINATION share/doc/hexicord)
endif()


#------------------------------------------------------------------------------
# External dependencies

option(Boost_USE_STATIC_LIBS    "Use static boost libraries." OFF)
option(Boost_USE_STATIC_RUNTIME "Use static boost runtime."   OFF)
option(OPENSSL_USE_STATIC_LIBS  "Use static OpenSSL."         OFF)

find_package(Boost COMPONENTS system)
find_package(OpenSSL REQUIRED)


#------------------------------------------------------------------------------
# POSIX Threads

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


#------------------------------------------------------------------------------
# Old CMake compatibility

include(cmake/external-targets.cmake)

if(CMAKE_VERSION VERSION_LESS 3.5)
    define_boost_target(system)
endif()

if(CMAKE_VERSION VERSION_LESS 3.4)
    define_openssl_ssl_target()
    define_openssl_crypto_target()
endif()

if(CMAKE_VERSION VERSION_LESS 3.1)
    define_threads_target()
endif()


#------------------------------------------------------------------------------
# Beast

find_package(Boost OPTIONAL_COMPONENTS beast)

if(NOT Boost_beast_FOUND)
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/lib/beast/include)
        message(FATAL_ERROR "Your system boost doesnt have beast component. Initialize lib/beast submodule using git submodule update --init.")
    endif()

    message(STATUS "No system boost.beast found, using lib/beast...")

    add_library(__Boost_beast INTERFACE)
    target_link_libraries(__Boost_beast INTERFACE Boost::system OpenSSL::SSL OpenSSL::Crypto)
    target_include_directories(__Boost_beast INTERFACE ${PROJECT_SOURCE_DIR}/lib/beast/include)

    add_library(Boost::beast ALIAS __Boost_beast)
elseif(CMAKE_VERSION VERSION_LESS 3.5)
    define_boost_target(beast)
endif()


#------------------------------------------------------------------------------
# Library

option(HEXICORD_STATIC "Build static library." ON )
option(HEXICORD_SHARED "Build shared library." OFF)

file(GLOB_RECURSE HEXICORD_HEADERS ${PROJECT_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE HEXICORD_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

if(HEXICORD_STATIC)
    add_library(hexicord STATIC ${HEXICORD_HEADERS} ${HEXICORD_SOURCES})
endif()

if(HEXICORD_SHARED)
    add_library(hexicord SHARED ${HEXICORD_HEADERS} ${HEXICORD_SOURCES})
endif()

target_link_libraries(hexicord PUBLIC Threads::Threads Boost::beast Boost::system OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(hexicord PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(hexicord PRIVATE ${WARNING_FLAGS})

# NOT SUPPORTED until we remove references to beast from headers.
#install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/hexicord
#        DESTINATION include
#        FILES_MATCHING PATTERN "*.hpp")
#install(TARGETS hexicord 
#        ARCHIVE DESTINATION lib
#        LIBRARY DESTINATION lib)

option(HEXICORD_EXAMPLES "Build examples." OFF)

if(HEXICORD_EXAMPLES)
    add_subdirectory(${PROJECT_SOURCE_DIR}/examples)
endif()

