cmake_minimum_required(VERSION 3.0)
project(double-conversion VERSION 2.0.1)

set(headers
    double-conversion/bignum.h
    double-conversion/cached-powers.h
    double-conversion/diy-fp.h
    double-conversion/double-conversion.h
    double-conversion/fast-dtoa.h
    double-conversion/fixed-dtoa.h
    double-conversion/ieee.h
    double-conversion/strtod.h
    double-conversion/utils.h)

add_library(double-conversion
            double-conversion/bignum.cc
            double-conversion/bignum-dtoa.cc
            double-conversion/cached-powers.cc
            double-conversion/diy-fp.cc
            double-conversion/double-conversion.cc
            double-conversion/fast-dtoa.cc
            double-conversion/fixed-dtoa.cc
            double-conversion/strtod.cc
            ${headers})
target_include_directories(
    double-conversion PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# pick a version #
set(double-conversion_SOVERSION_MAJOR 1)
set(double-conversion_SOVERSION_MINOR 0)
set(double-conversion_SOVERSION_PATCH 0)
set(double-conversion_SOVERSION ${double-conversion_SOVERSION_MAJOR}.${double-conversion_SOVERSION_MINOR}.${double-conversion_SOVERSION_PATCH})
set_property(TARGET double-conversion PROPERTY SOVERSION ${double-conversion_SOVERSION})

# set up testing if requested
option(BUILD_TESTING "Build test programs" OFF)
if(BUILD_TESTING)
  enable_testing()
  include(CTest)
  add_subdirectory(test)
endif()

include(GNUInstallDirs)
install(TARGETS double-conversion EXPORT double-conversionTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${headers}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/double-conversion)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/double-conversionConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/double-conversion/double-conversionConfig.cmake"
    INSTALL_DESTINATION "${LIB_INSTALL_DIR}/double-conversion/cmake")
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/double-conversion/double-conversionConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

export(EXPORT double-conversionTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/double-conversion/double-conversionTargets.cmake"
       NAMESPACE double-conversion::)

set(ConfigPackageLocation lib/cmake/double-conversion)
install(EXPORT double-conversionTargets
        FILE double-conversionTargets.cmake
        NAMESPACE double-conversion::
        DESTINATION ${ConfigPackageLocation})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/double-conversion/double-conversionConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/double-conversion/double-conversionConfigVersion.cmake"
        DESTINATION ${ConfigPackageLocation}
        COMPONENT Devel)
