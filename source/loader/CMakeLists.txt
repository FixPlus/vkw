file(GLOB LOADER_SOURCE *.cpp *.h *.hpp)
add_library(LOADER_LIB_OBJ OBJECT ${LOADER_SOURCE})
target_include_directories(LOADER_LIB_OBJ PRIVATE $ENV{Vulkan_INCLUDE_DIR})

if(UNIX)
	target_compile_options(LOADER_LIB_OBJ PUBLIC -fPIC)
endif()

add_library(LOADER_LIB INTERFACE)

target_sources(LOADER_LIB INTERFACE
	$<TARGET_OBJECTS:LOADER_LIB_OBJ>
)
