PROJECT ( CIP_PYTHON )

IF ( WIN32 )
  SET ( CIP_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE} )
ELSE ( WIN32 )
  SET ( CIP_LIBRARY_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
ENDIF ( WIN32 )

CONFIGURE_FILE ( ${CMAKE_CURRENT_SOURCE_DIR}/setup_ChestConventions.py.in 
  ${CMAKE_CURRENT_BINARY_DIR}/setup_ChestConventions.py @ONLY )

ADD_CUSTOM_TARGET ( ${PROJECT_NAME} ALL
  COMMAND python ${CMAKE_CURRENT_BINARY_DIR}/setup_ChestConventions.py build_ext -b ${CMAKE_CURRENT_SOURCE_DIR}/
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
ADD_DEPENDENCIES ( ${PROJECT_NAME} ChestConventions )

IF ( CIP_BUILD_TESTING )
 SUBDIRS ( particles )
 SUBDIRS ( phenotypes )
 SUBDIRS ( utils )
ENDIF( CIP_BUILD_TESTING ) 
