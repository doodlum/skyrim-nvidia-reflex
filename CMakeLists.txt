cmake_minimum_required(VERSION 3.20)

project(
	NVIDIA_Reflex
	VERSION 1.0.0
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XSEPlugin)
find_package(binary_io CONFIG REQUIRED)
