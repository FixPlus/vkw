include(GenerateExportHeader)
generate_export_header(${MAIN_LIB_NAME})
generate_export_header(${MAIN_LIB_NAME}s)
set_property(TARGET ${MAIN_LIB_NAME} PROPERTY VERSION ${PROJECT_VERSION})
set_property(TARGET ${MAIN_LIB_NAME}s PROPERTY VERSION ${PROJECT_VERSION})

install(TARGETS LOADER_LIB EXPORT ${PROJECT_NAME}Targets)
install(TARGETS ${MAIN_LIB_NAME} ${MAIN_LIB_NAME}s EXPORT ${PROJECT_NAME}Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)
install(DIRECTORY include DESTINATION .)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)
export(EXPORT ${PROJECT_NAME}Targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake"
        NAMESPACE ${PROJECT_NAME}::
)
configure_file(cmake/${PROJECT_NAME}Config.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
        COPYONLY
)
file(READ cmake/find_dependencies.cmake DEPS_CONTENT)
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" ${DEPS_CONTENT})

if(VKW_ENABLE_REFERENCE_GUARD)
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" "\nadd_definitions(-DVKW_ENABLE_REFERENCE_GUARD)")
endif()

if(VKW_ENABLE_EXCEPTIONS)
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" "\nadd_definitions(-DVKW_ENABLE_EXCEPTIONS)")
endif()

set(ConfigPackageLocation lib/cmake/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}Targets
        FILE
        ${PROJECT_NAME}Targets.cmake
        NAMESPACE
        ${PROJECT_NAME}::
        DESTINATION
        ${ConfigPackageLocation}
)
install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION
        ${ConfigPackageLocation}
        COMPONENT
        Devel
)