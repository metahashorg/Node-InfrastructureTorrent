set(SNIPER_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/libsniper" CACHE INTERNAL "sniper_include_dirs")
set(SNIPER_LIBRARIES "" CACHE INTERNAL "sniper_libraries")

include_directories(BEFORE ${SNIPER_INCLUDE_DIRS})
add_subdirectory(libsniper)
