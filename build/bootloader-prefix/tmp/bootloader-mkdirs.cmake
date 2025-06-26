# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/jin/esp/esp-idf/components/bootloader/subproject"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/tmp"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/src/bootloader-stamp"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/src"
  "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jin/workspaces/espWorkspace/esp32_desktop_controller/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
