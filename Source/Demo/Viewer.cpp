#include "Demo/Viewer.hpp"

// SDL
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"

// System Headers
#include <cassert>
#include <cstdint>



namespace Demo
{

Viewer::Viewer() :
	m_pWindow(nullptr)
{
	constexpr uint32_t kWindowWidth  = 2560u;
	constexpr uint32_t kWindowHeight = 1440u;

	bool result = SDL_Init(SDL_INIT_VIDEO);
	assert(result);

	m_pWindow = SDL_CreateWindow(
		"Graphics Demo",
		static_cast<int>(kWindowWidth),
		static_cast<int>(kWindowHeight),
		SDL_WINDOW_VULKAN);
	assert(m_pWindow != nullptr);
}



Viewer::~Viewer()
{
	assert(m_pWindow != nullptr);
	SDL_DestroyWindow(m_pWindow);
	SDL_Quit();
}



void Viewer::EnterRunLoop()
{
	for (uint64_t frameNumber = 0llu; ; ++frameNumber)
	{
		bool recievedExitEvent = false;

		SDL_Event event = {};

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				recievedExitEvent = true;
				break;
			}

			// Further event processing.
		}

		if (recievedExitEvent)
		{
			break;
		}

		// Update logic.
	}
}

} // namespace Demo