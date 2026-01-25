cmake_minimum_required(VERSION 3.26)

project(GraphicsDemo VERSION 0.0.0 LANGUAGES CXX)

add_executable(${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

add_subdirectory(Source)

# Internal libraries.
add_subdirectory(Modules)

# External libraries.
add_subdirectory(External)