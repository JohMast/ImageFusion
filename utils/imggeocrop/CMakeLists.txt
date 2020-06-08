# build target
add_executable(imggeocrop main.cpp ${SOURCES_HELPERS})
target_link_libraries(imggeocrop imagefusion)

# installation
install(TARGETS imggeocrop RUNTIME DESTINATION bin COMPONENT imggeocrop)

# adding component to the group
cpack_add_component(imggeocrop
                    DISPLAY_NAME "image geoinfo crop"
                    DESCRIPTION "Utility to crop two images to the same geo extents."
                    GROUP utils
                    DEPENDS lib)
