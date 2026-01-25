#ifndef GRAPHICSDEMO_VIEWER_HPP
#define GRAPHICSDEMO_VIEWER_HPP

// CoreLib
#include "CoreLib/UtilMacros.hpp"

// Forward Declarations

typedef struct SDL_Window SDL_Window;



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
	SDL_Window* m_pWindow;

};

} // namespace Demo

#endif // GRAPHICSDEMO_VIEWER_HPP