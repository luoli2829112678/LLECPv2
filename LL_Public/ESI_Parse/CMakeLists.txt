
add_library(ESI_Parse SHARED
    ESI_Parse.cpp
    pugixml.cpp
)

target_include_directories(ESI_Parse PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(ESI_Parse PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
