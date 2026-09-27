# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save")
  file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save")
endif()
file(MAKE_DIRECTORY
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/BLE_6adc_imu_power_save"
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix"
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/tmp"
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/src/BLE_6adc_imu_power_save-stamp"
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/src"
  "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/src/BLE_6adc_imu_power_save-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/src/BLE_6adc_imu_power_save-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/LocalWorkspace/Insole_sensor_workspace/BLE_6adc_imu_power_save/build_no_ble_serial/_sysbuild/sysbuild/images/BLE_6adc_imu_power_save-prefix/src/BLE_6adc_imu_power_save-stamp${cfgdir}") # cfgdir has leading slash
endif()
