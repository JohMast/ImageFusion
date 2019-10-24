# build target
add_executable(spstfm main.cpp ${SOURCES_HELPERS})
target_link_libraries(spstfm imagefusion)

# installation
install(TARGETS spstfm RUNTIME DESTINATION bin COMPONENT spstfm)

# adding component to the group
cpack_add_component(spstfm
                    DISPLAY_NAME SPSTFM
                    DESCRIPTION "Utility that implements the 'Spatiotemporal Reflectance Fusion via Sparse Representation' algorithm"
                    GROUP utils
                    DEPENDS lib)
