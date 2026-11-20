file(GLOB_RECURSE HDR_FILES *.hpp)
file(GLOB_RECURSE SRC_FILES *.cpp)
filter_appledouble(HDR_FILES)
filter_appledouble(SRC_FILES)

add_library(common-lib ${HDR_FILES} ${SRC_FILES})

target_compile_options(common-lib PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)

target_include_directories(common-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(common-lib utils)
target_link_libraries(common-lib asn-ngap)
target_link_libraries(common-lib asn-rrc)

target_include_directories(common-lib PUBLIC ${SCTP_INCLUDE_DIR})
target_link_libraries(common-lib ${SCTP_LIBRARY}) # Linux SCTP
