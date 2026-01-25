cmake_minimum_required(VERSION 3.26)

project(CoreLib VERSION 0.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC "Source/CoreLib/UtilMacros.hpp")

# CMake fails to determine the linker language when there's only a .hpp
# file in the target sources, so help it out.
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# No split headers, internal includes can skip specifying the subdir.
target_include_directories(
	${PROJECT_NAME}
	PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Source
	PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Source/CoreLib)