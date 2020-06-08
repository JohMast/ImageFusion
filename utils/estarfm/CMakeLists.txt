# build target
add_executable(estarfm main.cpp ${SOURCES_HELPERS})
target_link_libraries(estarfm imagefusion)

# installation
install(TARGETS estarfm RUNTIME DESTINATION bin COMPONENT estarfm)

# adding component to the group
cpack_add_component(estarfm
                    DISPLAY_NAME ESTARFM
                    DESCRIPTION "Utility that implements the 'Enhanced spatial and temporal adaptive reflectance fusion model algorithm'"
                    GROUP utils
                    DEPENDS lib)
