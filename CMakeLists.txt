cmake_minimum_required(VERSION 3.5)
project (libjson20  VERSION 1.0.0)

if (MSVC)
	add_compile_options(/W4 /EHsc /DNOMINMAX /std:c++20)
	set(STANDARD_LIBRARIES "")
else()
	add_compile_options(-Wall -Wextra -Wpedantic  -std=c++20)
	set(STANDARD_LIBRARIES "pthread")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Default path to install" FORCE)
endif()

include(library.cmake)
add_subdirectory("src/tests")
add_subdirectory("src/examples")

# Install the header file to the include directory
install(FILES ${CMAKE_HEADER_OUTPUT_DIRECTORY}/json20.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libjson20)


# Generate and install the Config.cmake file
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/libjson20Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/libjson20
)

# Install the Config.cmake file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libjson20Config.cmake DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/libjson20)

# Install the version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/libjson20ConfigVersion.cmake
  COMPATIBILITY SameMajorVersion
)

# Install the version file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libjson20ConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/cmake/libjson20)

