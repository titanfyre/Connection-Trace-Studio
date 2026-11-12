vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/titanfyre/Connection-Trace-Studio/releases/download/sdk-v0.1.0/connection-trace-studio-sdk-0.1.0.tar.gz"
    FILENAME "connection-trace-studio-sdk-0.1.0.tar.gz"
    SHA512 7f26b7e6c527134426d4612089f6b6567464566def44c8fee87e156949484892227b6abf7e6ef818efd9f10793d656f71b81b449bf947d71dfd568076c0a3618
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}" NO_REMOVE_ONE_LEVEL)
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ConnectionTraceStudioSDK CONFIG_PATH lib/cmake/ConnectionTraceStudioSDK)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
