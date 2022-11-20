cmake_minimum_required(VERSION 3.20)

project(
	NVIDIA_Reflex
	VERSION 1.1.1
	LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XSEPlugin)
