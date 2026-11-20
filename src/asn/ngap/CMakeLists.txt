file(GLOB HDR_FILES *.h)
file(GLOB SRC_FILES *.c)
filter_appledouble(HDR_FILES)
filter_appledouble(SRC_FILES)

add_library(asn-ngap ${HDR_FILES} ${SRC_FILES})
target_link_libraries(asn-ngap asn-asn1c)
