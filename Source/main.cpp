// Project Headers
#include "Demo/Viewer.hpp"

// System Headers
#include <memory>



int main(int /*argc*/, char** /*argv*/)
{
	std::unique_ptr<Demo::Viewer> viewer = std::make_unique<Demo::Viewer>();

	viewer->EnterRunLoop();

	return 0;
}