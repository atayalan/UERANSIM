file(GLOB HDR_FILES *.hpp)
file(GLOB SRC_FILES *.cpp)
filter_appledouble(HDR_FILES)
filter_appledouble(SRC_FILES)

add_library(app ${HDR_FILES} ${SRC_FILES})

target_compile_options(app PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)

target_link_libraries(app utils)
target_link_libraries(app udp)
