# SDL
add_subdirectory(SDL)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3::Headers)