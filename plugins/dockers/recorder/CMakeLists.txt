set(KRITA_RECORDERDOCKER_SOURCES recorderdocker.cpp recorderdocker_dock.cpp )
add_library(kritarecorderdocker MODULE ${KRITA_RECORDERDOCKER_SOURCES})
target_link_libraries(kritarecorderdocker kritaui)
install(TARGETS kritarecorderdocker  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
