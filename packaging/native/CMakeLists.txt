cmake_minimum_required(VERSION 3.24)
project(ConnectionTraceStudioSDK VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This SDK requires Windows x64. No driver is installed by this build.")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(TitanConnectionTraceStudio STATIC
  "sdk/SDK/ConnectionTrace.cpp"
)
add_library(ConnectionTraceStudioSDK::sdk ALIAS TitanConnectionTraceStudio)
set_target_properties(TitanConnectionTraceStudio PROPERTIES EXPORT_NAME sdk)
target_compile_features(TitanConnectionTraceStudio PUBLIC cxx_std_17)
target_compile_definitions(TitanConnectionTraceStudio PUBLIC )
target_compile_definitions(TitanConnectionTraceStudio PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
if(MSVC)
  target_compile_options(TitanConnectionTraceStudio PRIVATE /utf-8 /EHsc /W4)
endif()
target_include_directories(TitanConnectionTraceStudio PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk>"
  "$<INSTALL_INTERFACE:include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/SDK>"
  "$<INSTALL_INTERFACE:include/SDK>"
)
target_link_libraries(TitanConnectionTraceStudio PUBLIC advapi32 comctl32 crypt32 fwpuclnt gdi32 newdev ole32 rpcrt4 setupapi shell32 user32 wintrust ws2_32)
install(TARGETS TitanConnectionTraceStudio EXPORT ConnectionTraceStudioSDKTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")
install(FILES LICENSE DESTINATION share/connection-trace-studio)
install(EXPORT ConnectionTraceStudioSDKTargets NAMESPACE ConnectionTraceStudioSDK:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConnectionTraceStudioSDK)
configure_file("ConnectionTraceStudioSDKConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/ConnectionTraceStudioSDKConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ConnectionTraceStudioSDKConfigVersion.cmake" VERSION 0.1.0 COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ConnectionTraceStudioSDKConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/ConnectionTraceStudioSDKConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConnectionTraceStudioSDK)
