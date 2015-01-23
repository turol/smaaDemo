#include <cassert>

#include <boost/utility.hpp>

#include <SDL.h>


class SMAADemo : public boost::noncopyable {
	unsigned int windowWidth, windowHeight;
	SDL_Window *window;
	SDL_GLContext context;

public:

	SMAADemo();

	~SMAADemo();

	void initRender();
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, window(NULL)
, context(NULL)
{
	// TODO: check return value
	SDL_Init(SDL_INIT_VIDEO);

	// TODO: detect screens, log interesting display parameters etc
}


SMAADemo::~SMAADemo() {
	if (context != NULL) {
		SDL_GL_DeleteContext(context);
		context = NULL;
	}

	if (window != NULL) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	SDL_Quit();
}


void SMAADemo::initRender() {
	assert(window == NULL);
	assert(context == NULL);

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_OPENGL);

	context = SDL_GL_CreateContext(window);

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.
}


int main(int /*argc */, char * /*argv*/ []) {
	SMAADemo demo;

	// TODO: parse command line arguments and/or config file

	demo.initRender();
}
