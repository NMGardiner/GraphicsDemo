#ifndef GRAPHICSDEMO_VIEWER_HPP
#define GRAPHICSDEMO_VIEWER_HPP

// Forward Declarations

typedef struct SDL_Window SDL_Window;



namespace Demo
{

class Viewer
{

public:
	Viewer();
	~Viewer();

	void EnterRunLoop();

private:
	SDL_Window* m_pWindow;

};

} // namespace Demo

#endif // GRAPHICSDEMO_VIEWER_HPP