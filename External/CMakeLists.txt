# D3D12
target_link_libraries(${PROJECT_NAME} PRIVATE d3d12.lib dxgi.lib)

# SDL
add_subdirectory(SDL)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3::Headers)