
########################################################
# Files

SET (reporting_SRCS
  qgsreporting.cpp
  qgsserverreporter.cpp
)

SET (reporting_HDRS
  qgsserverreporter.h
)

########################################################
# Build

ADD_LIBRARY (reporting MODULE ${reporting_SRCS} ${reporting_HDRS})


INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/external
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/core/symbology
  ${CMAKE_SOURCE_DIR}/src/core/expression
  ${CMAKE_SOURCE_DIR}/src/core/geometry
  ${CMAKE_SOURCE_DIR}/src/core/metadata
  ${CMAKE_SOURCE_DIR}/src/core/raster
  ${CMAKE_SOURCE_DIR}/src/server
  ${CMAKE_SOURCE_DIR}/src/server/services
  ${CMAKE_SOURCE_DIR}/src/server/services/config

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_BINARY_DIR}/src/python
  ${CMAKE_BINARY_DIR}/src/analysis
  ${CMAKE_BINARY_DIR}/src/server
  ${CMAKE_CURRENT_BINARY_DIR}
)

TARGET_LINK_LIBRARIES(reporting
  qgis_core
  qgis_server
)


########################################################
# Install

INSTALL(TARGETS reporting
    RUNTIME DESTINATION ${QGIS_SERVER_MODULE_DIR}
    LIBRARY DESTINATION ${QGIS_SERVER_MODULE_DIR}
)
