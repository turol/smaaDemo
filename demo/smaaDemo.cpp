#include <sys/stat.h>

#include <cassert>
#include <cfloat>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/utility.hpp>

#include <SDL.h>

#include <GL/glew.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#define ATTR_POS   0


// FIXME: should be ifdeffed out on compilers which already have it
// http://isocpp.org/files/papers/N3656.txt
namespace std {

    template<class T> struct _Unique_if {
        typedef unique_ptr<T> _Single_object;
    };

    template<class T> struct _Unique_if<T[]> {
        typedef unique_ptr<T[]> _Unknown_bound;
    };

    template<class T, size_t N> struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };

    template<class T, class... Args>
        typename _Unique_if<T>::_Single_object
        make_unique(Args&&... args) {
            return unique_ptr<T>(new T(std::forward<Args>(args)...));
        }

    template<class T>
        typename _Unique_if<T>::_Unknown_bound
        make_unique(size_t n) {
            typedef typename remove_extent<T>::type U;
            return unique_ptr<T>(new U[n]());
        }

    template<class T, class... Args>
        typename _Unique_if<T>::_Known_bound
        make_unique(Args&&...) = delete;


}  // namespace std


struct FILEDeleter {
	void operator()(FILE *f) { fclose(f); }
};


union Color {
	uint32_t val;
	struct {
		uint8_t r, g, b, a;
	};
};


std::vector<char> readTextFile(std::string filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (!file) {
		// TODO: better exception
		throw std::runtime_error("file not found " + filename);
	}

	int fd = fileno(file.get());
	if (fd < 0) {
		// TODO: better exception
		throw std::runtime_error("no fd");
	}

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	int retval = fstat(fd, &statbuf);
	if (retval < 0) {
		// TODO: better exception
		throw std::runtime_error("fstat failed");
	}

	uint64_t filesize = static_cast<uint64_t>(statbuf.st_size);
	// ensure NUL -termination
	std::vector<char> buf(filesize + 1, '\0');

	size_t ret = fread(&buf[0], 1, filesize, file.get());
	if (ret != filesize)
	{
		// TODO: better exception
		throw std::runtime_error("fread failed");
	}

	return buf;
}


class Shader : public boost::noncopyable {
    GLuint program;

public:
	Shader(std::string vertexShaderName, std::string fragmentShaderName);

	~Shader();

	GLint getUniformLocation(const char *name);

	void bind();
};


Shader::Shader(std::string vertexShaderName, std::string fragmentShaderName)
: program(0)
{
	auto vsSrc = readTextFile(vertexShaderName);
	auto fsSrc = readTextFile(fragmentShaderName);

	std::vector<const char *> sourcePointers;
	sourcePointers.push_back(&vsSrc[0]);

	// TODO: refactor to separate function
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &sourcePointers[0], NULL);
	glCompileShader(vertexShader);

	// TODO: defer checking to enable multithreaded shader compile
	GLint status = 0;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetShaderInfoLog(vertexShader, status, NULL, &infoLog[0]);
		printf("info log: %s\n", &infoLog[0]); fflush(stdout);
		glDeleteShader(vertexShader);
		throw std::runtime_error("VS compile failed");
	}

	sourcePointers[0] = &fsSrc[0];
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &sourcePointers[0], NULL);
	glCompileShader(fragmentShader);

	// TODO: defer checking to enable multithreaded shader compile
	status = 0;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetShaderInfoLog(fragmentShader, status, NULL, &infoLog[0]);
		printf("info log: %s\n", &infoLog[0]); fflush(stdout);
		glDeleteShader(fragmentShader);
		glDeleteShader(vertexShader);
		throw std::runtime_error("FS compile failed");
	}

	program = glCreateProgram();
	glBindAttribLocation(program, ATTR_POS, "position");
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &status);
		std::vector<char> infoLog(status + 1, '\0');
		// TODO: better logging
		glGetProgramInfoLog(program, status, NULL, &infoLog[0]);
		printf("info log: %s\n", &infoLog[0]); fflush(stdout);
		throw std::runtime_error("shader link failed");
	}
}


Shader::~Shader() {
	if (program != 0) {
		glDeleteProgram(program);
		program = 0;
	}
}


GLint Shader::getUniformLocation(const char *name) {
	assert(program != 0);
	assert(name != NULL);

	return glGetUniformLocation(program, name);
}


void Shader::bind() {
	assert(program != 0);

	glUseProgram(program);
}


class SMAADemo : public boost::noncopyable {
	unsigned int windowWidth, windowHeight;
	SDL_Window *window;
	SDL_GLContext context;

	std::unique_ptr<Shader> simpleShader;
	// TODO: these are shader properties
	// better yet use UBOs
	GLint mvpLoc, colorLoc;

	// TODO: create helper classes for these
	GLuint vbo, ibo;

	unsigned int cubePower;

	struct Cube {
		glm::vec3 pos;
		glm::quat orient;
		Color col;


		Cube(float x_, float y_, float z_, glm::quat orient_, Color col_)
		: pos(x_, y_, z_), orient(orient_), col(col_)
		{
		}
	};

	std::vector<Cube> cubes;

public:

	SMAADemo();

	~SMAADemo();

	void initRender();

	void createCubes();

	void mainLoop();

	void render();
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, window(NULL)
, context(NULL)
, mvpLoc(0)
, colorLoc(0)
, vbo(0)
, ibo(0)
, cubePower(3)
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


struct Vertex {
	float x, y, z;
};


const float coord = sqrtf(3.0f) / 2.0f;


Vertex vertices[] =
{
	  { -coord ,  coord, -coord }
	, {  coord ,  coord, -coord }
	, { -coord ,  coord,  coord }
	, {  coord ,  coord,  coord }
	, { -coord , -coord, -coord }
	, {  coord , -coord, -coord }
	, { -coord , -coord,  coord }
	, {  coord , -coord,  coord }
};


// FIXME: check vertex winding, some of these are probably wrong side out
uint32_t indices[] =
{
    // top
	  0, 1, 2
	, 1, 2, 3

	// front
	, 0, 4, 1
    , 4, 1, 5

	// back
	, 2, 3, 6
    , 3, 6, 7

	// left
	, 0, 2, 4
    , 2, 4, 6

	// right
	, 1, 3, 7
    , 3, 7, 8

	// bottom
	, 4, 5, 6
	, 5, 6, 7
};


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

    // enable vsync, using late swap tearing if possible
	int retval = SDL_GL_SetSwapInterval(-1);
	if (retval != 0) {
		SDL_GL_SetSwapInterval(1);
	}

	glewExperimental = true;
	glewInit();

	// swap once to get better traces
	SDL_GL_SwapWindow(window);

	simpleShader = std::make_unique<Shader>("simpleVS.vert", "simpleFS.frag");
	mvpLoc = simpleShader->getUniformLocation("modelViewProj");
	colorLoc = simpleShader->getUniformLocation("color");

	// TODO: DSA
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);
	glEnableVertexAttribArray(0);
}


void SMAADemo::createCubes() {
	// cubes on a side is some power of 2
	const unsigned int cubesSide = pow(2, cubePower);

	// cube of cubes, n^3 cubes total
	const unsigned int numCubes = pow(cubesSide, 3);

	const float cubeDiameter = sqrtf(3.0f);
	const float cubeDistance = cubeDiameter + 1.0f;

	const float bigCubeSide = cubeDistance * cubesSide;

	cubes.clear();
	cubes.reserve(numCubes);

	Color col;
	for (unsigned int x = 0; x < cubesSide; x++) {
		for (unsigned int y = 0; y < cubesSide; y++) {
			for (unsigned int z = 0; z < cubesSide; z++) {
				// random RGB, alpha = 1.0
				// TODO: use repeatable random generator and seed
				// FIXME: we're abusing little-endianness, make it portable
				col.val = (rand() & 0x00FFFFFF) | 0xFF000000;
				float qx = float(rand()) / RAND_MAX;
				float qy = float(rand()) / RAND_MAX;
				float qz = float(rand()) / RAND_MAX;
				float qw = float(rand()) / RAND_MAX;
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				cubes.emplace_back((x * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (z * cubeDistance) - (bigCubeSide / 2.0f)
				                 , glm::quat(qx, qy, qz, qw)
				                 , col);
			}
		}
	}
}


void SMAADemo::mainLoop() {
	bool keepGoing = true;
	while (keepGoing) {
		// TODO: timing
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
					keepGoing = false;
				}
				break;
			}
		}

		render();
	}
}


void SMAADemo::render() {
	// TODO: reset all relevant state in case some 3rd-party program fucked them up

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	simpleShader->bind();

	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f));
	glm::mat4 proj = glm::perspective(65.0f, float(windowWidth) / windowHeight, 0.1f, 100.0f);
	glm::mat4 viewProj = proj * view;

	// TODO: instancing
	for (const auto &cube : cubes) {
		glm::mat4 model = glm::translate(glm::mat4(1.0f), cube.pos) * glm::mat4_cast(cube.orient);
		glm::mat4 mvp = viewProj * model;
		glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
		glUniform4f(colorLoc, cube.col.r / 255.0f, cube.col.g / 255.0f, cube.col.b / 255.0f, cube.col.a / 255.0f);
		glDrawElements(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL);
	}

	SDL_GL_SwapWindow(window);
}


int main(int /*argc */, char * /*argv*/ []) {
	SMAADemo demo;

	// TODO: parse command line arguments and/or config file

	demo.initRender();
	demo.createCubes();
	demo.mainLoop();

	return 0;
}
