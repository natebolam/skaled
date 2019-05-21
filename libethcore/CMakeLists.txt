file(GLOB sources "*.cpp" "*.h")

add_library(ethcore ${sources})

target_include_directories(ethcore PRIVATE "${UTILS_INCLUDE_DIR}")
target_link_libraries(ethcore PUBLIC devcrypto devcore
        ${DEPS_INSTALL_ROOT}/lib/libcurl.a
        ${DEPS_INSTALL_ROOT}/lib/libssl.a
        ${DEPS_INSTALL_ROOT}/lib/libcrypto.a
        ${DEPS_INSTALL_ROOT}/lib/libz.a)
