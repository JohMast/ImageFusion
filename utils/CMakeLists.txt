# For each utility add in this file:
#   add_subdirectory("<YOUR_UTILITY_NAME>")
#
#
# The CMakeLists.txt of your utility should contain:
#   # build target
#   add_executable(<YOUR_UTILITY_NAME> <LIST_YOUR_SOURCE_FILES>.cpp)
#   target_link_libraries(<YOUR_UTILITY_NAME> imagefusion)
#
#   # installation
#   install(TARGETS <YOUR_UTILITY_NAME> RUNTIME DESTINATION bin COMPONENT <YOUR_UTILITY_NAME>)
#
#   # adding component to the group
#   cpack_add_component(<YOUR_UTILITY_NAME>
#                       DISPLAY_NAME "<YOUR DISPLAY NAME IN THE WINDOWS INSTALLER>"
#                       DESCRIPTION "<DESCRIBE YOUR UTILITY FOR THE WINDOWS INSTALLER>"
#                       GROUP utils
#                       DEPENDS lib)

include_directories(helpers)
set(SOURCES_HELPERS ${CMAKE_CURRENT_LIST_DIR}/helpers/utils_common.cpp)

add_subdirectory("imgcompare")
add_subdirectory("imggeocrop")
add_subdirectory("imginterp")
add_subdirectory("starfm")
add_subdirectory("estarfm")
add_subdirectory("spstfm")
add_subdirectory("fitfc")

is_symbol_defined(symdef __linux__)
if(symdef)
    install(FILES helpers/bash_completion
            DESTINATION /etc/bash_completion.d
            COMPONENT helper
            RENAME imagefusion
            EXCLUDE_FROM_ALL) # still included in package

    cpack_add_component(helper GROUP utils)
endif()

is_symbol_defined(symdef _WIN32)
if(symdef)
    # Add the following code for each utility
    # This will search and copy all the dependencies of utilities to make it standalone.
    # Since all the three utilites have same dependencies, the fixup_bundle can be is executed only once.
    # Incase of utilites having different dependencies, the fixup_bundle has to be executed for each utility.
    install(CODE "
            include(BundleUtilities)
            fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/imgcompare.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            COMPONENT dependencies)
    # Uncomment the following lines to let the fixup_bundle search and copy the dependencies of following utilities
    # install(CODE "
            # include(BundleUtilities)
            # fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/imggeocrop.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            # COMPONENT dependencies)

    # install(CODE "
            # include(BundleUtilities)
            # fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/starfm.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            # COMPONENT dependencies)

    # install(CODE "
            # include(BundleUtilities)
            # fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/estarfm.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            # COMPONENT dependencies)

    # install(CODE "
            # include(BundleUtilities)
            # fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/spstfm.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            # COMPONENT dependencies)

    # install(CODE "
            # include(BundleUtilities)
            # fixup_bundle(\"${CMAKE_BINARY_DIR}/bin/fitfc.exe\" \"\" \"${CMAKE_BINARY_DIR}/bin\")"
            # COMPONENT dependencies)

    # installation
    install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
            DESTINATION bin
            COMPONENT dependencies
            FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "libimagefusion.dll" EXCLUDE)

    #adding the dependencies to the package
    cpack_add_component(dependencies
                        DISPLAY_NAME "Dependent Libraries"
                        DESCRIPTION "Includes all the external dependent libraries for the utils to work"
                        GROUP utils)
endif()

# creation of group under which the utilities can be grouped.
cpack_add_component_group(utils
                          DISPLAY_NAME "Utilities"
                          DESCRIPTION "Utilities made with the Imagefusion framework.")

