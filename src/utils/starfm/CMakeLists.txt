# build target
add_executable(starfm main.cpp ${SOURCES_HELPERS})
target_link_libraries(starfm imagefusion)

# installation
install(TARGETS starfm RUNTIME DESTINATION bin COMPONENT starfm)

# adding component to the group
cpack_add_component(starfm
                    DISPLAY_NAME STARFM
                    DESCRIPTION "Utility that implements the 'Spatial and temporal adaptive reflectance fusion model algorithm'"
                    GROUP utils
                    DEPENDS lib)
