# build target
add_executable(fitfc main.cpp ${SOURCES_HELPERS})
target_link_libraries(fitfc imagefusion)

# installation
install(TARGETS fitfc RUNTIME DESTINATION bin COMPONENT fitfc)

# adding component to the group
cpack_add_component(fitfc
                    DISPLAY_NAME Fit-FC
                    DESCRIPTION "Utility that implements the 'Regression model fitting (RM fitting), spatial filtering (SF) and residual compensation (RC)'"
                    GROUP utils
                    DEPENDS lib)
