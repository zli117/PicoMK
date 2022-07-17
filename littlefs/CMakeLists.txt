add_library(littlefs STATIC
        littlefs/lfs.c
        littlefs/lfs_util.c)
target_include_directories(littlefs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(littlefs hardware_flash)
