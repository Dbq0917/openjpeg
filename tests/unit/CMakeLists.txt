# UNIT TESTS

include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2
)

set(unit_test
  testempty0
  testempty1
  testempty2
)
foreach(ut ${unit_test})
  add_executable(${ut} ${ut}.c)
  target_link_libraries(${ut} openjp2)
  add_test(NAME ${ut} COMMAND ${ut})
endforeach()

add_executable(testjp2 testjp2.c)
target_link_libraries(testjp2 openjp2)
add_test(NAME testjp2 COMMAND testjp2 ${OPJ_DATA_ROOT})

if(BUILD_JPIP)
  add_executable(testjpip_query_parser testjpip_query_parser.c)
  target_include_directories(testjpip_query_parser PRIVATE
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjpip
  )
  target_link_libraries(testjpip_query_parser openjpip)
  add_test(NAME testjpip_query_parser COMMAND testjpip_query_parser)

  add_executable(testjpip_sockline testjpip_sockline.c)
  target_include_directories(testjpip_sockline PRIVATE
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjpip
  )
  target_link_libraries(testjpip_sockline openjpip)
  add_test(NAME testjpip_sockline COMMAND testjpip_sockline)
endif()
