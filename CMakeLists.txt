zephyr_library()
zephyr_library_sources(device/src/manikin_ble_serializer.c
                       device/src/manikin_ble.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/device/src)
