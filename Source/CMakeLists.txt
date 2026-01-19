target_sources(
	${PROJECT_NAME}
	PRIVATE
	main.cpp
	Demo/Viewer.hpp
	Demo/Viewer.cpp)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR})