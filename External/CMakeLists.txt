# D3D12
target_link_libraries(${PROJECT_NAME} PRIVATE d3d12.lib dxgi.lib)

# D3D12MemoryAllocator
add_subdirectory(D3D12MemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE D3D12MemoryAllocator)

# Dear ImGui
target_sources(
	${PROJECT_NAME}
	PRIVATE
	imgui/imconfig.h
	imgui/imgui.cpp
	imgui/imgui.h
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_internal.h
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/imstb_rectpack.h
	imgui/imstb_textedit.h
	imgui/imstb_truetype.h
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/backends/imgui_impl_sdl3.h
	imgui/misc/debuggers/imgui.natvis
	imgui/misc/debuggers/imgui.natstepfilter)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	imgui/
	imgui/backends/)

# SDL
add_subdirectory(SDL)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3::Headers)