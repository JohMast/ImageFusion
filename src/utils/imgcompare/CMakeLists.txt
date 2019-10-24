# build target
add_executable(imgcompare main.cpp ${SOURCES_HELPERS})
target_link_libraries(imgcompare imagefusion)

# installation
install(TARGETS imgcompare RUNTIME DESTINATION bin COMPONENT imgcompare)

# adding component to the group
cpack_add_component(imgcompare
                    DISPLAY_NAME "image compare"
                    DESCRIPTION "Utility to compare images (diff image, scatter image, etc.)"
                    GROUP utils
                    DEPENDS lib)
