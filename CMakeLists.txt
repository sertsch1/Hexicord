cmake_minimum_required(VERSION 3.0)

# Better behavior of VS generator.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

if(DEFINED HEXICORD_VERSION)
    message("Skipping version detection using Git tags...")
    set(HEXICORD_SEMVER "${HEXICORD_VERSION}" PARENT_SCOPE)
else()
    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        execute_process(
                COMMAND           "${GIT_EXECUTABLE} status"
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                RESULT_VARIABLE   git_status_result
                OUTPUT_VARIABLE   git_status_output
                ERROR_VARIABLE    git_status_error
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
            include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_from_git.cmake)
            version_from_git(GIT_EXECUTABLE ${GIT_EXECUTABLE}
                             INCLUDE_HASH ON)

            set(HEXICORD_DETECTED_VERSION ${VERSION})
            set(HEXICORD_SEMVER  "${SEMVER}")
        else()
            message(WARNING "Setting version to default string, because building outside of Git repository, please specify version by setting HEXICORD_VERSION CMake variable.")
        endif()
    else()
        message(WARNING "Setting version to default string, because Git executable not found, please specify version by setting HEXICORD_VERSION CMake variable.")
    endif()
endif()

project(HEXICORD VERSION ${HEXICORD_DETECTED_VERSION})

message(STATUS "Configuring Hexicord ${HEXICORD_VERSION}")

#------------------------------------------------------------------------------
# Global building configuration (C++ standard, defines, warning flags).

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

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

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
option(Boost_USE_FROM_DEPS "Use boost from deps" OFF)
option(Boost_USE_STATIC_LIBS    "Use static boost libraries." OFF)
option(Boost_USE_STATIC_RUNTIME "Use static boost runtime."   OFF)
option(Boost_USE_BEAST_FROM_DEPS "Use boost beast from deps" OFF)

option(OpenSSL_USE_FROM_DEPS "Use openssl from deps" OFF)
option(OPENSSL_USE_STATIC_LIBS  "Use static OpenSSL."         OFF)
option(USE_BORING_SSL "Use boring ssl fork" OFF)

# Default cryptop libs#
set(crypto_lib OpenSSL::Crypto)
set(ssl_lib OpenSSL::SSL)

# Boost lib differs
set(boost_lib Boost::system)

if ("${EXTERNAL_BASE_PATH}" STREQUAL "")
	# Set paths
	SET(EXTERNAL_BASE_PATH ${PROJECT_SOURCE_DIR}/external)
endif()

if(NOT Boost_USE_FROM_DEPS)
	find_package(Boost REQUIRED COMPONENTS system)
else()
	add_subdirectory(${EXTERNAL_BASE_PATH}/boost ../bin_hexicord_boost)
	set(boost_lib boost::system boost::asio)
endif()

if (USE_BORING_SSL)
	add_subdirectory(${EXTERNAL_BASE_PATH}/boringssl ../bin_hexicord_boringssl)

	# Paul's openssl lib differs from original lib names
	set(crypto_lib crypto)
	set(ssl_lib ssl)
else()
	if(NOT OpenSSL_USE_FROM_DEPS)
		SET(OPENSSL_ROOT_DIR ${EXTERNAL_BASE_PATH}/openssl)
		find_package(OpenSSL REQUIRED)
	else()
		add_subdirectory(${EXTERNAL_BASE_PATH}/openssl ../bin_hexicord_openssl)
		
		# Paul's openssl lib differs from original lib names
		set(crypto_lib crypto)
		set(ssl_lib ssl)
	endif()
endif()
set(HEXICORD_DEPENDENCIES Threads::Threads ${boost_lib} ${ssl_lib} ${crypto_lib})

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
    define_zlib_target()
    include(cmake/cmake_cxx11_standard.cmake)
endif()

#------------------------------------------------------------------------------
# Beast

if(NOT Boost_USE_BEAST_FROM_DEPS)
	find_package(Boost OPTIONAL_COMPONENTS beast)

	if(NOT Boost_beast_FOUND)
		if(NOT EXISTS ${PROJECT_SOURCE_DIR}/lib/beast/include)
			message(FATAL_ERROR "Your system boost doesnt have beast component. Initialize lib/beast submodule using git submodule update --init.")
		endif()

		message(STATUS "No system boost.beast found, using lib/beast...")

		add_library(__Boost_beast INTERFACE)
		target_link_libraries(__Boost_beast INTERFACE ${boost_lib} ${ssl_lib} ${crypto_lib})
		target_include_directories(__Boost_beast SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/lib/beast/include)

		add_library(Boost::beast ALIAS __Boost_beast)
	elseif(CMAKE_VERSION VERSION_LESS 3.5)
		define_boost_target(beast)
	endif()

else()
	SET(BOOST_BEAST_ROOT ${EXTERNAL_BASE_PATH}/beast)
	add_library(__Boost_beast INTERFACE)
	
	target_link_libraries(__Boost_beast INTERFACE ${boost_lib} ${ssl_lib} ${crypto_lib})
	target_include_directories(__Boost_beast SYSTEM INTERFACE ${BOOST_BEAST_ROOT}/include)
endif()

add_library(Boost::beast ALIAS __Boost_beast)
set(HEXICORD_DEPENDENCIES ${HEXICORD_DEPENDENCIES} Boost::beast)

#------------------------------------------------------------------------------
# Library Configuration

macro(hexicord_config type option_name docstring default_value)
    set(${option_name} ${default_value} CACHE ${type} ${docstring})
    message(STATUS "Hexicord configuration: ${option_name} = ${${option_name}}")
endmacro()

hexicord_config(STRING HEXICORD_GITHUB "URL to send in UA." "https://github.com/foxcpp/Hexicord")

hexicord_config(BOOL HEXICORD_DEBUG_LOG "Write debug messages to stderr. Warning: VERY VERBOSE."  OFF)

# Ratelimits handling
hexicord_config(BOOL HEXICORD_RATELIMIT_PREDICTION   "Attempt to predict and avoid ratelimit hit. Experimental."    OFF)
hexicord_config(BOOL HEXICORD_RATELIMIT_HIT_AS_ERROR "Throw exception on ratelimit hit instead of silent retrying." OFF)

hexicord_config(STRING HEXICORD_RATELIMIT_CACHE_SIZE   "Limit count of entries with information about ratelimits per route" "512")

hexicord_config(BOOL HEXICORD_ZLIB "Use optional zlib compression" OFF)

configure_file(${HEXICORD_SOURCE_DIR}/src/hexicord/config.hpp.in
               ${HEXICORD_BINARY_DIR}/hexicord/config.hpp @ONLY)

#------------------------------------------------------------------------------
# Optional dependencies

find_package(ZLIB)

if (ZLIB_FOUND AND HEXICORD_ZLIB)
    set(HEXICORD_DEPENDENCIES ${HEXICORD_DEPENDENCIES} ZLIB::ZLIB)
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

target_link_libraries(hexicord PUBLIC ${HEXICORD_DEPENDENCIES})
# to access config.hpp
target_include_directories(hexicord PUBLIC
                           ${PROJECT_SOURCE_DIR}/src
                           ${PROJECT_BINARY_DIR})
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
