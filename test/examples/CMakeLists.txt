function(add_example example_name)
    add_executable(${example_name} ${example_name}/main.cpp)
    target_link_libraries(${example_name} PRIVATE ${VKW_RUNTIME_LIB})
    message(STATUS "adding '${example_name}' example")
endfunction()

file(GLOB VKW_EXAMPLE_LIST RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")

foreach(name ${VKW_EXAMPLE_LIST})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${name})
        add_example(${name})
    endif()
endforeach()
