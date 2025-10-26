include(GenerateExportHeader)
generate_export_header(${VKW_RUNTIME_LIB} EXPORT_FILE_NAME ${VKW_GENERATED_DIR}/vkwrt.h)
install(FILES ${VKW_GENERATED_DIR}/vkwrt.h DESTINATION include/vkw)
set_property(TARGET ${VKW_RUNTIME_LIB} PROPERTY VERSION ${PROJECT_VERSION})

install(TARGETS ${VKW_RUNTIME_LIB} EXPORT ${PROJECT_NAME}Targets
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
file(READ cmake/find_dependencies.cmake VKW_DEPS_CONTENT)
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake" ${VKW_DEPS_CONTENT})

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