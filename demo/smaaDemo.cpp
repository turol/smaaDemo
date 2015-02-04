#include <sys/stat.h>

#include <algorithm>
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
#define ATTR_COLOR   1
#define ATTR_CUBEPOS 2
#define ATTR_ROT     3


#define TEXUNIT_COLOR 0


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


std::vector<char> readFile(std::string filename) {
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
	std::vector<char> buf(filesize, '\0');

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


static std::vector<char> processShaderIncludes(std::vector<char> shaderSource) {
	std::vector<char> output(shaderSource);

	auto includePos = output.begin();

	while (true) {
		// find an #include
		while (includePos < output.end()) {
			// is it a comment?
			if (*includePos == '/') {
				includePos++;
				if (includePos == output.end()) {
					break;
				}

				if (*includePos == '/') {
					// until line end
					includePos = std::find(includePos, output.end(), '\n');
				} else if (*includePos == '*') {
					// until "*/"
					while (true) {
						includePos = std::find(includePos + 1, output.end(), '*');
						if (includePos == output.end()) {
							break;
						}

						includePos++;
						if (includePos == output.end()) {
							break;
						}

						if (*includePos == '/') {
							includePos++;
							break;
						} else if (*includePos == '*') {
							// handle "**/"
							includePos--;
						}
					}
				}
			} else if (*includePos == '#' ) {
				includePos++;
				std::string directive(includePos, std::min(includePos + 7, output.end()));
				if (directive == "include") {
					// we have an "#include"
					break;
				}
				includePos++;
			} else {
				includePos++;
			}
		}

		if (includePos == output.end()) {
			// not found, we're done
			break;
		}

		// find first of either " or <
		auto filenamePos = std::min(std::find(includePos, output.end(), '"'), std::find(includePos, output.end(), '<')) + 1;
		auto filenameEnd = std::min(std::find(filenamePos, output.end(), '"'), std::find(filenamePos, output.end(), '>'));
		std::string filename(filenamePos, filenameEnd);

		printf("Filename: %s\n", filename.c_str());

		// we don't want a terminating '\0'
		auto includeContents = readFile(filename);
		// TODO: strip other errant '\0's

		std::vector<char> newOutput;
		// TODO: could reduce this a bit
		newOutput.reserve(output.size() + includeContents.size());
		newOutput.insert(newOutput.end(), output.begin(), includePos);
		newOutput.insert(newOutput.end(), includeContents.begin(), includeContents.end());
		newOutput.insert(newOutput.end(), filenameEnd + 1, output.end());

		auto dist = std::distance(output.begin(), includePos);
		std::swap(output, newOutput);
		includePos = output.begin() + dist;
		// go again in case of recursive includes
	}

	return output;
}


Shader::Shader(std::string vertexShaderName, std::string fragmentShaderName)
: program(0)
{
	auto vsSrc = processShaderIncludes(readTextFile(vertexShaderName));
	auto fsSrc = processShaderIncludes(readTextFile(fragmentShaderName));

	std::vector<const char *> sourcePointers;
	sourcePointers.push_back(&vsSrc[0]);

	// TODO: refactor to separate function
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &sourcePointers[0], NULL);
	glCompileShader(vertexShader);

	// TODO: defer checking to enable multithreaded shader compile
	GLint status = 0;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

	{
		GLint infoLogLen = 0;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLogLen);
		std::vector<char> infoLog(infoLogLen + 1, '\0');
		// TODO: better logging
		glGetShaderInfoLog(vertexShader, infoLogLen, NULL, &infoLog[0]);
		printf("vert shader \"%s\" info log: %s\n", vertexShaderName.c_str(), &infoLog[0]); fflush(stdout);
	}

	if (status != GL_TRUE) {
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

	{
		GLint infoLogLen = 0;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLogLen);
		std::vector<char> infoLog(infoLogLen + 1, '\0');
		// TODO: better logging
		glGetShaderInfoLog(fragmentShader, infoLogLen, NULL, &infoLog[0]);
		printf("frag shader \"%s\" info log: %s\n", fragmentShaderName.c_str(), &infoLog[0]); fflush(stdout);
	}

	if (status != GL_TRUE) {
		glDeleteShader(fragmentShader);
		glDeleteShader(vertexShader);
		throw std::runtime_error("FS compile failed");
	}

	program = glCreateProgram();
	glBindAttribLocation(program, ATTR_POS, "position");
	glBindAttribLocation(program, ATTR_COLOR, "color");
	glBindAttribLocation(program, ATTR_CUBEPOS, "cubePos");
	glBindAttribLocation(program, ATTR_ROT, "rotationQuat");

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
	glUseProgram(program);

	GLint colorLoc = getUniformLocation("color");
	glUniform1i(colorLoc, TEXUNIT_COLOR);
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


class SMAADemo;


class Framebuffer : public boost::noncopyable {
	// TODO: need a proper Render object to control the others
	friend class SMAADemo;

	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;

public:

	explicit Framebuffer(GLuint fbo_)
	: fbo(fbo_)
	, colorTex(0)
	, depthTex(0)
	, width(0)
	, height(0)
	{
	}

	~Framebuffer();

	void bind();

	void blitTo(Framebuffer &target);
};


Framebuffer::~Framebuffer() {
	if (colorTex != 0) {
		glDeleteTextures(1, &colorTex);
		colorTex = 0;
	}

	if (depthTex != 0) {
		glDeleteTextures(1, &depthTex);
		depthTex = 0;
	}

	if (fbo != 0) {
		glDeleteFramebuffers(1, &fbo);
		fbo = 0;
	}
}


void Framebuffer::bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}


void Framebuffer::blitTo(Framebuffer &target) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.fbo);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}


class SMAADemo : public boost::noncopyable {
	unsigned int windowWidth, windowHeight;
	SDL_Window *window;
	SDL_GLContext context;

	std::unique_ptr<Shader> simpleShader;
	// TODO: these are shader properties
	// better yet use UBOs
	GLint viewProjLoc;

	// TODO: create helper classes for these
	GLuint vbo, ibo;
	GLuint instanceVBO;

	unsigned int cubePower;

	std::unique_ptr<Framebuffer> builtinFBO;
	std::unique_ptr<Framebuffer> renderFBO;

	bool antialiasing;
	std::unique_ptr<Shader> fxaaShader;
	std::unique_ptr<Shader> smaaEdgeShader;

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


	struct InstanceData {
		float x, y, z;
		float qx, qy, qz;
		Color col;


		InstanceData(glm::quat rot, glm::vec3 pos, Color col_)
			: x(pos.x), y(pos.y), z(pos.z)
			, qx(rot.x), qy(rot.y), qz(rot.z)
			, col(col_)
		{
			// shader assumes this and uses it to calculate w from other components
			assert(rot.w >= 0.0);
		}
	};

	std::vector<InstanceData> instances;


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
, viewProjLoc(-1)
, vbo(0)
, ibo(0)
, instanceVBO(0)
, cubePower(3)
, antialiasing(true)
{
	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

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


#define VBO_OFFSETOF(st, member) reinterpret_cast<GLvoid *>(offsetof(st, member))


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

	// TODO: check extensions
	// at least direct state access, texture storage

	// swap once to get better traces
	SDL_GL_SwapWindow(window);

	simpleShader = std::make_unique<Shader>("simpleVS.vert", "simpleFS.frag");
	viewProjLoc = simpleShader->getUniformLocation("viewProj");

	{
	smaaEdgeShader = std::make_unique<Shader>("smaaEdge.vert", "smaaEdge.frag");
	GLint screenSizeLoc = smaaEdgeShader->getUniformLocation("screenSize");
	glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
	glUniform4fv(screenSizeLoc, 1, glm::value_ptr(screenSize));
	}

	{
	fxaaShader = std::make_unique<Shader>("fxaa.vert", "fxaa.frag");
	GLint screenSizeLoc = fxaaShader->getUniformLocation("screenSize");
	glm::vec4 screenSize(windowWidth, windowHeight, 1.0f / float(windowWidth), 1.0f / float(windowHeight));
	glUniform4fv(screenSizeLoc, 1, glm::value_ptr(screenSize));
	}

	// TODO: DSA
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), NULL, GL_STREAM_DRAW);

	glVertexAttribPointer(ATTR_CUBEPOS, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, x));
	glVertexAttribDivisor(ATTR_CUBEPOS, 1);

	glVertexAttribPointer(ATTR_ROT, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, qx));
	glVertexAttribDivisor(ATTR_ROT, 1);

	glVertexAttribPointer(ATTR_COLOR, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, col));
	glVertexAttribDivisor(ATTR_COLOR, 1);

	builtinFBO = std::make_unique<Framebuffer>(0);
	builtinFBO->width = windowWidth;
	builtinFBO->height = windowHeight;

	// TODO: move to a helper method
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	renderFBO = std::make_unique<Framebuffer>(fbo);
	renderFBO->width = windowWidth;
	renderFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	renderFBO->colorTex = tex;
	glTextureStorage2DEXT(tex, GL_TEXTURE_2D, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glNamedFramebufferTextureEXT(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	tex = 0;
	glGenTextures(1, &tex);
	renderFBO->depthTex = tex;
	glTextureStorage2DEXT(tex, GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);
	glNamedFramebufferTextureEXT(fbo, GL_DEPTH_ATTACHMENT, tex, 0);

	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_COLOR, GL_TEXTURE_2D, renderFBO->colorTex);
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

	// reallocate instance data buffer
	glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData) * numCubes, NULL, GL_STREAM_DRAW);

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
				// TODO: switch
				if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
					keepGoing = false;
				} else if (event.key.keysym.scancode == SDL_SCANCODE_A) {
					antialiasing = !antialiasing;
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

	renderFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);

	simpleShader->bind();

	uint32_t ticks = SDL_GetTicks();
	const uint32_t rotationPeriod = 30 * 1000;
	ticks = ticks % rotationPeriod;
	glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f)), float(M_PI * 2.0f * ticks) / rotationPeriod, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, 0.1f, 100.0f);
	glm::mat4 viewProj = proj * view;
	glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

	instances.clear();
	instances.reserve(cubes.size());
	for (const auto &cube : cubes) {
		instances.emplace_back(cube.orient, cube.pos, cube.col);
	}

	// FIXME: depends on instance data vbo remaining bound
	// use dsa instead
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(InstanceData) * instances.size(), &instances[0]);

	glEnableVertexAttribArray(ATTR_POS);
	glEnableVertexAttribArray(ATTR_CUBEPOS);
	glEnableVertexAttribArray(ATTR_ROT);
	glEnableVertexAttribArray(ATTR_COLOR);
	glDrawElementsInstanced(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL, cubes.size());

	glDisableVertexAttribArray(ATTR_POS);
	glDisableVertexAttribArray(ATTR_CUBEPOS);
	glDisableVertexAttribArray(ATTR_ROT);
	glDisableVertexAttribArray(ATTR_COLOR);

	if (antialiasing) {
		glDisable(GL_DEPTH_TEST);
		builtinFBO->bind();
		fxaaShader->bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
	} else {
	renderFBO->blitTo(*builtinFBO);
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
