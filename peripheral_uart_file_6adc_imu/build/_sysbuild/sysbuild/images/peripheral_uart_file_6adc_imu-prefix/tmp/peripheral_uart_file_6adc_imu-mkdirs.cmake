# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu")
  file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu")
endif()
file(MAKE_DIRECTORY
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/peripheral_uart_file_6adc_imu"
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix"
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/tmp"
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/src/peripheral_uart_file_6adc_imu-stamp"
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/src"
  "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/src/peripheral_uart_file_6adc_imu-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/src/peripheral_uart_file_6adc_imu-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/peripheral_uart_file_6adc_imu/build/_sysbuild/sysbuild/images/peripheral_uart_file_6adc_imu-prefix/src/peripheral_uart_file_6adc_imu-stamp${cfgdir}") # cfgdir has leading slash
endif()
