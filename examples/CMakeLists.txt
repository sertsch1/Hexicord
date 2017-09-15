project(hexicord-examples)

file(GLOB EXAMPLES
     LIST_DIRECTORIES TRUE
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     */)

foreach(example ${EXAMPLES})
    if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${example})
        message(STATUS "Example: ${example}")
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${example}/CMakeLists.txt)
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${example})
        else()
            file(GLOB example_sources ${CMAKE_CURRENT_SOURCE_DIR}/${example}/*.cpp
                                      ${CMAKE_CURRENT_SOURCE_DIR}/${example}/*.hpp)
            add_executable(${example} ${example_sources})
            target_link_libraries(${example} hexicord)
        endif()
    endif()
endforeach()
