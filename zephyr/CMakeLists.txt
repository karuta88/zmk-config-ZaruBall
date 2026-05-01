# Ensure ZMK public headers (e.g. <zmk/hid.h>) are visible to external modules
# when building against forks whose exported include paths differ.
zephyr_include_directories(${CMAKE_SOURCE_DIR}/include)
