set(SRSRAN_XNAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library(srsran-fmt STATIC
    external/fmt/src/format.cc
    external/fmt/src/os.cc)
target_include_directories(srsran-fmt PUBLIC ${SRSRAN_XNAP_ROOT}/external/fmt/include)

add_library(srsran-log STATIC
    lib/srslog/backend_worker.cpp
    lib/srslog/event_trace.cpp
    lib/srslog/formatters/json_formatter.cpp
    lib/srslog/formatters/text_formatter.cpp
    lib/srslog/srslog.cpp
    lib/srslog/srslog_c.cpp)
target_include_directories(srsran-log PUBLIC
    ${SRSRAN_XNAP_ROOT}/include
    ${SRSRAN_XNAP_ROOT}/external
    ${SRSRAN_XNAP_ROOT}/external/fmt/include
    ${SRSRAN_XNAP_ROOT}/lib/srslog)
target_link_libraries(srsran-log PUBLIC srsran-fmt pthread)

add_library(srsran-xnap STATIC
    lib/asn1/asn1_utils.cpp
    lib/asn1/xnap/common.cpp
    lib/asn1/xnap/xnap.cpp
    lib/asn1/xnap/xnap_ies.cpp
    lib/asn1/xnap/xnap_pdu_contents.cpp
    lib/support/byte_buffer.cpp)
target_include_directories(srsran-xnap PUBLIC
    ${SRSRAN_XNAP_ROOT}/include
    ${SRSRAN_XNAP_ROOT}/external
    ${SRSRAN_XNAP_ROOT}/external/fmt/include)
target_compile_options(srsran-xnap PRIVATE -Os -fno-exceptions)
target_link_libraries(srsran-xnap PUBLIC srsran-log)
