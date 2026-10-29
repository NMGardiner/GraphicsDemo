#ifndef GRAPHICSDEMO_VIEWER_HPP
#define GRAPHICSDEMO_VIEWER_HPP

// Project
#include "Demo/Renderer.hpp"

// CoreLib
#include "CoreLib/UtilMacros.hpp"

// Forward Declarations

struct SDL_Window;
union SDL_Event;



namespace Demo
{

class Viewer
{

public:
	Viewer();
	~Viewer();

	CORE_CLASS_CONSTRUCTORS_DELETE(Viewer);

	void EnterRunLoop();

private:
	void HandleEvent(const SDL_Event& event);

private:
	SDL_Window* m_pWindow;
	Renderer* m_pRenderer;

	bool m_shutdownFlag;

}; // class Viewer

} // namespace Demo

#endif // GRAPHICSDEMO_VIEWER_HPP