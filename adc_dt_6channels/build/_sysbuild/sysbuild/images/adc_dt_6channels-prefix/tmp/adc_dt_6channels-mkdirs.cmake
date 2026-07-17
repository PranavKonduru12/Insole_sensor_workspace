# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels")
  file(MAKE_DIRECTORY "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels")
endif()
file(MAKE_DIRECTORY
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/adc_dt_6channels"
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix"
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/tmp"
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/src/adc_dt_6channels-stamp"
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/src"
  "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/src/adc_dt_6channels-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/src/adc_dt_6channels-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/LocalWorkspace/embedded_projects_fromSDK_local/adc_dt_6channels/build/_sysbuild/sysbuild/images/adc_dt_6channels-prefix/src/adc_dt_6channels-stamp${cfgdir}") # cfgdir has leading slash
endif()
