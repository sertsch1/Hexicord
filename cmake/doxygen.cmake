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
