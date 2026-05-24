target_sources(
	${PROJECT_NAME}
	PRIVATE
	"main.cpp"
	"Demo/Renderer.cpp"
	"Demo/Renderer.hpp"
	"Demo/Viewer.cpp"
	"Demo/Viewer.hpp")

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR})