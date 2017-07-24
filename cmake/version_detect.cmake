if(DEFINED HEXICORD_VERSION)
    message("Skipping version detection using Git tags...")
    add_definitions(-DHEXICORD_VERSION="${HEXICORD_VERSION}")
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
    if (EXISTS ${PROJECT_SOURCE_DIR}/.git)
            include(${PROJECT_SOURCE_DIR}/cmake/version_from_git.cmake)
            version_from_git(GIT_EXECUTABLE ${GIT_EXECUTABLE}
                             INCLUDE_HASH ON
                             )
            add_definitions(-DHEXICORD_VERSION="${SEMVER}")
            set(HEXICORD_VERSION ${VERSION})
        else()
            message(WARNING "Setting version to null string, because building outside of Git repository, please specify version by setting HEXICORD_VERSION CMake variable.")
            add_definitions(-DHEXICORD_VERSION="unknown")
            set(HEXICORD_VERSION 0.0.0)
        endif()
    else()
        message(WARNING "Setting version to null string, because Git executable not found, please specify version by setting HEXICORD_VERSION CMake variable.")
        add_definitions(-DHEXICORD_VERSION="unknown")
        set(HEXICORD_VERSION 0.0.0)
    endif()
endif()
