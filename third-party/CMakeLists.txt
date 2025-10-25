set(BUILD_SHARED_LIBS OFF)
set(ENABLE_CAT OFF)
set(ENABLE_CPIO OFF)
set(ENABLE_TAR OFF)
set(ENABLE_TEST OFF)

add_subdirectory(libarchive)

include_directories(libarchive/libarchive)
