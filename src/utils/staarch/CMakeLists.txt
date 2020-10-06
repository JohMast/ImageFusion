# build target
add_executable(staarch main.cpp ${SOURCES_HELPERS})
target_link_libraries(staarch imagefusion)

# installation
install(TARGETS staarch RUNTIME DESTINATION bin COMPONENT staarch)

# adding component to the group
cpack_add_component(staarch
                    DISPLAY_NAME STAARCH
                    DESCRIPTION "Utility that implements the 'Spatial Temporal Adaptive Algorithm for mapping Reflectance Change' including modified STARFM prediction"
                    GROUP utils
                    DEPENDS lib)
