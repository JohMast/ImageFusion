# build target
add_executable(imginterp main.cpp customopts.cpp ${SOURCES_HELPERS})
target_link_libraries(imginterp imagefusion)

# installation
install(TARGETS imginterp RUNTIME DESTINATION bin COMPONENT imginterp)

# adding component to the group
cpack_add_component(imginterp
                    DISPLAY_NAME "Linear Interpolator"
                    DESCRIPTION "Utility to fill the gaps caused by removing the cloud pixels from the images using linear interpolation"
                    GROUP utils
                    DEPENDS lib)
