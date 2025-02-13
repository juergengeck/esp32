cmake_minimum_required(VERSION 3.10)
project(OneCrypto)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find mbedtls installed by Homebrew
set(MBEDTLS_ROOT_DIR "/opt/homebrew/opt/mbedtls")
set(MBEDTLS_INCLUDE_DIRS "${MBEDTLS_ROOT_DIR}/include")
set(MBEDTLS_LIBRARIES "${MBEDTLS_ROOT_DIR}/lib/libmbedtls.dylib"
                      "${MBEDTLS_ROOT_DIR}/lib/libmbedcrypto.dylib"
                      "${MBEDTLS_ROOT_DIR}/lib/libmbedx509.dylib")

# Find nlohmann/json installed by Homebrew
find_package(nlohmann_json REQUIRED)

# Library target
add_library(one_crypto
    src/one_crypto.cpp
)

target_include_directories(one_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MBEDTLS_INCLUDE_DIRS}
)

target_link_libraries(one_crypto PUBLIC
    ${MBEDTLS_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Mac tool target
add_executable(mac_tool
    examples/mac_tool.cpp
)

target_link_libraries(mac_tool PRIVATE
    one_crypto
) 