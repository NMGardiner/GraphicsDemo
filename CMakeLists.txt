cmake_minimum_required(VERSION 3.26)

project(GraphicsDemo VERSION 0.0.0 LANGUAGES CXX)

add_executable(${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

add_subdirectory(Source)

# Internal libraries.
add_subdirectory(Modules)

# External libraries.
add_subdirectory(External)

# Symlink the data dir to the binary output dir.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink
		${CMAKE_CURRENT_LIST_DIR}/Data
		${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/Data)

# Compile HLSL shaders.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
	COMMAND cmd /C ${CMAKE_CURRENT_LIST_DIR}/Tools/CompileShaders.bat)