cmake_minimum_required(VERSION 3.21)

project(
	NVIDIA_Reflex
	VERSION 1.1.2
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XSEPlugin)

find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")

target_include_directories(
	"${PROJECT_NAME}"
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/cmake
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${SIMPLEINI_INCLUDE_DIRS}
)

find_package(magic_enum CONFIG REQUIRED)

target_link_libraries(
	${PROJECT_NAME} 
	PUBLIC 
	${CommonLibName}::${CommonLibName}
	PRIVATE
		magic_enum::magic_enum
		debug ${CMAKE_CURRENT_SOURCE_DIR}/extern/detours/x64/Debug/detours.lib
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/extern/detours/x64/Release/detours.lib
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/extern/nvapi/nvapi64.lib
)