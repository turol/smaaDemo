#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL.h>


#ifdef USE_GLEW

#include <GL/glew.h>

#else  // USE_GLEW

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#endif  // USE_GLEW


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "AreaTex.h"
#include "SearchTex.h"


#define ATTR_POS   0
#define ATTR_COLOR   1
#define ATTR_CUBEPOS 2
#define ATTR_ROT     3


#define TEXUNIT_COLOR 0
#define TEXUNIT_AREATEX 1
#define TEXUNIT_SEARCHTEX 2
#define TEXUNIT_EDGES 3
#define TEXUNIT_BLEND 4


#ifndef USE_GLEW


void glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
	glBindTexture(target, texture);
	GLenum format;
	switch (internalformat) {
	case GL_RG8:
		format = GL_RG;
		break;

	case GL_R8:
		format = GL_RED;
		break;

	case GL_RGBA8:
		format = GL_RGBA;
		break;

	case GL_DEPTH_COMPONENT16:
		format =  GL_DEPTH_COMPONENT;
		break;

	default:
		assert(false);  // No matching internalformat
		break;
	}
	for (GLsizei i = 0; i < levels; i++) {
		glTexImage2D(target, i, internalformat, width, height, 0, format, GL_BYTE, NULL);
		width = std::max(1.0, floor(width / 2));
		height = std::max(1.0, floor(height / 2));
	}
}


#endif  // USE_GLEW


#ifdef EMSCRIPTEN


#include <emscripten.h>


// a collection of emscripten hacks to avoid polluting main code with them


#define SDL_WINDOW_FULLSCREEN_DESKTOP SDL_WINDOW_FULLSCREEN


Uint64 SDL_GetPerformanceFrequency() {
	return 1000;
}


Uint64 SDL_GetPerformanceCounter() {
	return SDL_GetTicks();
}


struct SDL_Window {
};


SDL_Window* SDL_CreateWindow(const char* /* title */,
                             int         /* x */,
                             int         /* y */,
                             int         w,
                             int         h,
                             Uint32      flags_)
{
	Uint32 flags = SDL_DOUBLEBUF;

	if ((flags_ & SDL_WINDOW_RESIZABLE) != 0) {
		flags |= SDL_RESIZABLE;
	}
	if ((flags_ & SDL_WINDOW_FULLSCREEN) != 0) {
		flags |= SDL_FULLSCREEN;
	}
	if ((flags_ & SDL_WINDOW_OPENGL) != 0) {
		flags |= SDL_OPENGL;
	}

	SDL_SetVideoMode(w, h, 32, flags);

	return new SDL_Window();
}


SDL_GLContext SDL_GL_CreateContext(SDL_Window* window) {
	return reinterpret_cast<SDL_GLContext>(window);
}


int SDL_GetNumVideoDisplays() {
	return 0;
}


int SDL_GetNumDisplayModes(int /* displayIndex */) {
	return 0;
}


int SDL_GetDisplayMode(int /* displayIndex */, int /* modeIndex */, SDL_DisplayMode* /* mode */) {
	return 0;
}


#endif  // EMSCRIPTEN


#ifdef _MSC_VER
#define fileno _fileno
#define __builtin_unreachable() assert(false)
#endif


// should be ifdeffed out on compilers which already have it (eg. VS2013)
// http://isocpp.org/files/papers/N3656.txt
#ifndef _MSC_VER
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
#endif


struct FILEDeleter {
	void operator()(FILE *f) { fclose(f); }
};


union Color {
	uint32_t val;
	struct {
		uint8_t r, g, b, a;
	};
};


static const Color white = { 0xFFFFFFFF };


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
				std::string directive(includePos + 1, std::min(includePos + 8, output.end()));
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


static GLuint createShader(GLenum type, const std::string &name, const std::vector<char> &rawSrc) {
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
	auto src = processShaderIncludes(rawSrc);

	const char *sourcePointer = &src[0];
	GLint sourceLen = src.size();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &sourcePointer, &sourceLen);
	glCompileShader(shader);

	// TODO: defer checking to enable multithreaded shader compile
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	{
		GLint infoLogLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
		if (infoLogLen != 0) {
			std::vector<char> infoLog(infoLogLen + 1, '\0');
			// TODO: better logging
			glGetShaderInfoLog(shader, infoLogLen, NULL, &infoLog[0]);
			if (infoLog[0] != '\0') {
				printf("shader \"%s\" info log:\n%s\ninfo log end\n", name.c_str(), &infoLog[0]); fflush(stdout);
			}
		}
	}

	if (status != GL_TRUE) {
		glDeleteShader(shader);
		throw std::runtime_error("shader compile failed");
	}

	return shader;
}


static GLuint createShader(GLenum type, const std::string &filename) {
	auto src = readTextFile(filename);
	return createShader(type, filename, src);
}


class Shader;
class ShaderBuilder;


class VertexShader {
	GLuint shader;

	VertexShader() = delete;

	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	friend class Shader;

public:

	VertexShader(const std::string &filename);

	VertexShader(const std::string &name, const ShaderBuilder &builder);

	~VertexShader();
};


class FragmentShader {
	GLuint shader;

	FragmentShader() = delete;

	FragmentShader(const FragmentShader &) = delete;
	FragmentShader &operator=(const FragmentShader &) = delete;

	FragmentShader(FragmentShader &&) = delete;
	FragmentShader &operator=(FragmentShader &&) = delete;

	friend class Shader;

public:

	FragmentShader(const std::string &filename);

	FragmentShader(const std::string &name, const ShaderBuilder &builder);

	~FragmentShader();
};


class ShaderBuilder {
	std::vector<char> source;

	ShaderBuilder &operator=(const ShaderBuilder &) = delete;

	ShaderBuilder(ShaderBuilder &&) = delete;
	ShaderBuilder &operator=(ShaderBuilder &&) = delete;

	friend class VertexShader;
	friend class FragmentShader;

	bool glES;

public:
	explicit ShaderBuilder(bool glES_);
	ShaderBuilder(const ShaderBuilder &other);

	VertexShader compileVertex();
	FragmentShader compileFragment();

	void pushLine(const std::string &line);
	void pushVertexAttr(const std::string &attr);
	void pushVertexVarying(const std::string &var);
	void pushFragmentVarying(const std::string &var);
	void pushFile(const std::string &filename);
};


ShaderBuilder::ShaderBuilder(bool glES_)
: glES(glES_)
{
	source.reserve(512);

	if (glES) {
		pushLine("#version 100");

		// FIXME: is this universally available on WebGL implementations?
		pushLine("#extension GL_EXT_shader_texture_lod : enable");

		pushLine("precision highp float;");
	} else {
		pushLine("#version 330");

#ifdef USE_GLEW

		if (GLEW_ARB_gpu_shader5) {
			pushLine("#extension GL_ARB_gpu_shader5 : enable");
		}
		if (GLEW_ARB_texture_gather) {
			pushLine("#extension GL_ARB_texture_gather : enable");
		}

#endif  // USE_GLEW

	}
}


ShaderBuilder::ShaderBuilder(const ShaderBuilder &other)
: glES(other.glES)
{
	source = other.source;
}


void ShaderBuilder::pushLine(const std::string &line) {
	source.reserve(source.size() + line.size() + 1);
	source.insert(source.end(), line.begin(), line.end());
	source.push_back('\n');
}


void ShaderBuilder::pushVertexAttr(const std::string &attr) {
	if (glES) {
		pushLine("attribute " + attr);
	} else {
		pushLine("in " + attr);
	}

}


void ShaderBuilder::pushVertexVarying(const std::string &var) {
	if (glES) {
		pushLine("varying " + var);
	} else {
		pushLine("out " + var);
	}
}


void ShaderBuilder::pushFragmentVarying(const std::string &var) {
	if (glES) {
		pushLine("varying " + var);
	} else {
		pushLine("in " + var);
	}
}


void ShaderBuilder::pushFile(const std::string &filename) {
	// TODO: grab file here, don't use #include
	// which we'll just end up parsing back later
	pushLine("#include \"" + filename + "\"");
}


class Shader {
    GLuint program;

	GLint screenSizeLoc;

	Shader() = delete;
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&) = delete;
	Shader &operator=(Shader &&) = delete;

public:
	Shader(std::string vertexShaderName, std::string fragmentShaderName);
	Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader);

	~Shader();

	GLint getUniformLocation(const char *name);

	GLint getScreenSizeLocation();

	void bind();
};


VertexShader::VertexShader(const std::string &filename)
: shader(0)
{
	shader = createShader(GL_VERTEX_SHADER, filename);
}


VertexShader::VertexShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
{
	shader = createShader(GL_VERTEX_SHADER, name, builder.source);
}


VertexShader::~VertexShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


FragmentShader::FragmentShader(const std::string &filename)
: shader(0)
{
	shader = createShader(GL_FRAGMENT_SHADER, filename);
}


FragmentShader::FragmentShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
{
	shader = createShader(GL_FRAGMENT_SHADER, name, builder.source);
}


FragmentShader::~FragmentShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


Shader::Shader(std::string vertexShaderName, std::string fragmentShaderName)
: Shader(
         VertexShader(vertexShaderName)
       , FragmentShader(fragmentShaderName)
        )
{
}



Shader::Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader)
: program(0)
, screenSizeLoc(0)
{
	program = glCreateProgram();
	glBindAttribLocation(program, ATTR_POS, "position");
	glBindAttribLocation(program, ATTR_COLOR, "color");
	glBindAttribLocation(program, ATTR_CUBEPOS, "cubePos");
	glBindAttribLocation(program, ATTR_ROT, "rotationQuat");

	glAttachShader(program, vertexShader.shader);
	glAttachShader(program, fragmentShader.shader);
	glLinkProgram(program);

	GLint status = 0;
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

	GLint areaTexLoc = getUniformLocation("areaTex");
	if (areaTexLoc >= 0) {
		glUniform1i(areaTexLoc, TEXUNIT_AREATEX);
	}

	GLint searchTexLoc = getUniformLocation("searchTex");
	if (searchTexLoc >= 0) {
		glUniform1i(searchTexLoc, TEXUNIT_SEARCHTEX);
	}

	GLint edgesTexLoc = getUniformLocation("edgesTex");
	if (edgesTexLoc >= 0) {
		glUniform1i(edgesTexLoc, TEXUNIT_EDGES);
	}

	GLint blendTexLoc = getUniformLocation("blendTex");
	if (blendTexLoc >= 0) {
		glUniform1i(blendTexLoc, TEXUNIT_BLEND);
	}

	screenSizeLoc = getUniformLocation("screenSize");
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


GLint Shader::getScreenSizeLocation() {
	return screenSizeLoc;
}


void Shader::bind() {
	assert(program != 0);

	glUseProgram(program);
}


class SMAADemo;


class Framebuffer {
	// TODO: need a proper Render object to control the others
	friend class SMAADemo;

	GLuint fbo;
	GLuint colorTex;
	GLuint depthTex;

	unsigned int width, height;

	Framebuffer() = delete;
	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&) = delete;
	Framebuffer &operator=(Framebuffer &&) = delete;

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


namespace AAMethod {

enum AAMethod {
	  FXAA
	, SMAA
	, LAST = SMAA
};


const char *name(AAMethod m) {
	switch (m) {
	case FXAA:
		return "FXAA";
		break;

	case SMAA:
		return "SMAA";
		break;
	}

	__builtin_unreachable();
}


}  // namespace AAMethod


static const char *smaaDebugModeStr(unsigned int mode) {
	switch (mode) {
	case 0:
		return "none";

	case 1:
		return "edges";

	case 2:
		return "blend";
	}

	__builtin_unreachable();
}


// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

static uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}


class RandomGen {
	pcg32_random_t rng;

	RandomGen(const RandomGen &) = delete;
	RandomGen &operator=(const RandomGen &) = delete;
	RandomGen(RandomGen &&) = delete;
	RandomGen &operator=(RandomGen &&) = delete;

public:

	RandomGen(uint64_t seed)
	{
		rng.state = seed;
		rng.inc = 1;
		// spin it once for proper initialization
		pcg32_random_r(&rng);
	}


	float randFloat() {
		uint32_t u = randU32();
		// because 24 bits mantissa
		u &= 0x00FFFFFFU;
		return float(u) / 0x00FFFFFFU;
	}


	uint32_t randU32() {
		return pcg32_random_r(&rng);
	}
};


static const char *fxaaQualityLevels[] =
{ "10", "15", "20", "29", "39" };


static const unsigned int maxFXAAQuality = sizeof(fxaaQualityLevels) / sizeof(fxaaQualityLevels[0]);


static const char *smaaQualityLevels[] =
{ "LOW", "MEDIUM", "HIGH", "ULTRA" };


static const unsigned int maxSMAAQuality = sizeof(smaaQualityLevels) / sizeof(smaaQualityLevels[0]);


class SMAADemo {
	unsigned int windowWidth, windowHeight;
	unsigned int resizeWidth, resizeHeight;
	bool vsync;
	bool fullscreen;
	SDL_Window *window;
	SDL_GLContext context;
	bool glES;

	std::unique_ptr<Shader> cubeShader;
	// TODO: these are shader properties
	// better yet use UBOs
	GLint viewProjLoc;

	// TODO: create helper classes for these
	GLuint cubeVBO, cubeIBO;
	GLuint fullscreenVBO;
	GLuint instanceVBO;

	unsigned int cubePower;

	std::unique_ptr<Framebuffer> builtinFBO;
	std::unique_ptr<Framebuffer> renderFBO;
	std::unique_ptr<Framebuffer> edgesFBO;
	std::unique_ptr<Framebuffer> blendFBO;

	bool antialiasing;
	AAMethod::AAMethod aaMethod;
	std::unique_ptr<Shader> fxaaShader;
	std::unique_ptr<Shader> smaaEdgeShader;
	std::unique_ptr<Shader> smaaBlendWeightShader;
	std::unique_ptr<Shader> smaaNeighborShader;
	GLuint areaTex;
	GLuint searchTex;

	bool rotateCamera;
	float cameraRotation;
	uint64_t lastTime;
	uint64_t freq;
	uint64_t rotationTime;
	unsigned int debugMode;
	unsigned int colorMode;
	bool rightShift, leftShift;
	RandomGen random;
	unsigned int fxaaQuality;
	unsigned int smaaQuality;
	bool keepGoing;


	struct Image {
		std::string filename;
		GLuint tex;
	};

	std::vector<Image> images;


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

	SMAADemo(const SMAADemo &) = delete;
	SMAADemo &operator=(const SMAADemo &) = delete;
	SMAADemo(SMAADemo &&) = delete;
	SMAADemo &operator=(SMAADemo &&) = delete;

public:

	SMAADemo();

	~SMAADemo();

	void initRender();

	void createFramebuffers();

	void applyVSync();

	void applyFullscreen();

	void buildCubeShader();

	void buildFXAAShader();

	void buildSMAAShaders();

	void createCubes();

	void colorCubes();

	void setCubeVBO();

	void setFullscreenVBO();

	void mainLoopIteration();

	bool shouldKeepGoing() const {
		return keepGoing;
	}

	void addImage(const char *filename);

	void render();
};


SMAADemo::SMAADemo()
: windowWidth(1280)
, windowHeight(720)
, vsync(true)
, fullscreen(false)
, window(NULL)
, context(NULL)
#ifdef EMSCRIPTEN
, glES(true)
#else  // EMSCRIPTEN
, glES(false)
#endif  // EMSCRIPTEN
, viewProjLoc(-1)
, cubeVBO(0)
, cubeIBO(0)
, fullscreenVBO(0)
, instanceVBO(0)
, cubePower(3)
, antialiasing(true)
, aaMethod(AAMethod::SMAA)
, areaTex(0)
, searchTex(0)
, rotateCamera(false)
, cameraRotation(0.0f)
, lastTime(0)
, freq(0)
, rotationTime(0)
, debugMode(0)
, colorMode(0)
, rightShift(false)
, leftShift(false)
, random(1)
, fxaaQuality(maxFXAAQuality - 1)
, smaaQuality(maxSMAAQuality - 1)
, keepGoing(true)
{
	resizeWidth = windowWidth;
	resizeHeight = windowHeight;

	// TODO: check return value
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	freq = SDL_GetPerformanceFrequency();
	lastTime = SDL_GetPerformanceCounter();

	// TODO: detect screens, log interesting display parameters etc
	// TODO: initialize random using external source
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


static const Vertex vertices[] =
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


static const float fullscreenVertices[] =
{
	  -1.0f, -1.0f
	,  3.0f, -1.0f
	, -1.0f,  3.0f
};


// FIXME: check vertex winding, some of these are probably wrong side out
static const uint32_t indices[] =
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


void SMAADemo::buildCubeShader() {
	ShaderBuilder s(glES);

	ShaderBuilder vert(s);
	vert.pushLine("uniform mat4 viewProj;");
	vert.pushVertexAttr("vec3 rotationQuat;");
	vert.pushVertexAttr("vec3 cubePos;");
	vert.pushVertexAttr("vec3 color;");
	vert.pushVertexAttr("vec3 position;");
	vert.pushVertexVarying("vec3 colorFrag;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    // our quaternions are normalized and have w > 0.0");
	vert.pushLine("    float qw = sqrt(1.0 - dot(rotationQuat, rotationQuat));");
	vert.pushLine("    // rotate");
	vert.pushLine("    // this is quaternion multiplication from glm");
	vert.pushLine("    vec3 v = position;");
	vert.pushLine("    vec3 uv = cross(rotationQuat, v);");
	vert.pushLine("    vec3 uuv = cross(rotationQuat, uv);");
	vert.pushLine("    uv *= (2.0 * qw);");
	vert.pushLine("    uuv *= 2.0;");
	vert.pushLine("    vec3 rotatedPos = v + uv + uuv;");
	vert.pushLine("");
	vert.pushLine("    gl_Position = viewProj * vec4(rotatedPos + cubePos, 1.0);");
	vert.pushLine("    colorFrag = color;");
	vert.pushLine("}");

	VertexShader vShader("cube.vert", vert);

	// fragment
	ShaderBuilder frag(s);

	frag.pushFragmentVarying("vec3 colorFrag;");
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushLine("    gl_FragColor.xyz = colorFrag;");
	frag.pushLine("    gl_FragColor.w = dot(colorFrag, vec3(0.299, 0.587, 0.114));");
	frag.pushLine("}");

	FragmentShader fShader("cube.frag", frag);

	cubeShader = std::make_unique<Shader>(vShader, fShader);
	viewProjLoc = cubeShader->getUniformLocation("viewProj");
}


void SMAADemo::buildFXAAShader() {
	glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

	ShaderBuilder s(glES);

	s.pushLine("#define FXAA_PC 1");

#ifdef EMSCRIPTEN

	s.pushLine("#define FXAA_GLSL_120 1");

#else  // EMSCRIPTEN

	s.pushLine("#define FXAA_GLSL_130 1");

#endif  // EMSCRIPTEN

	// TODO: cache shader based on quality level
	s.pushLine("#define FXAA_QUALITY_PRESET " + std::string(fxaaQualityLevels[fxaaQuality]));

	ShaderBuilder vert(s);
	vert.pushVertexAttr("vec2 pos;");
	vert.pushVertexVarying("vec2 texcoord;");
	vert.pushLine("void main(void)");
	vert.pushLine("{");
	vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
	vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
	vert.pushLine("}");

	VertexShader vShader("fxaa.vert", vert);

	// fragment
	ShaderBuilder frag(s);
	frag.pushFile("fxaa3_11.h");
	frag.pushLine("uniform sampler2D color;");
	frag.pushLine("uniform vec4 screenSize;");
	frag.pushFragmentVarying("vec2 texcoord;");
	frag.pushLine("void main(void)");
	frag.pushLine("{");
	frag.pushLine("    vec4 zero = vec4(0.0, 0.0, 0.0, 0.0);");
	frag.pushLine("    gl_FragColor = FxaaPixelShader(texcoord, zero, color, color, color, screenSize.xy, zero, zero, zero, 0.75, 0.166, 0.0833, 8.0, 0.125, 0.05, zero);");
	frag.pushLine("}");

	FragmentShader fShader("fxaa.frag", frag);

	fxaaShader = std::make_unique<Shader>(vShader, fShader);
	glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
}


void SMAADemo::buildSMAAShaders() {
	ShaderBuilder s(glES);

	s.pushLine("#define SMAA_RT_METRICS screenSize");
	s.pushLine("#define SMAA_GLSL_3 1");
	// TODO: cache shader based on quality level
	s.pushLine("#define SMAA_PRESET_"  + std::string(smaaQualityLevels[smaaQuality]) + " 1");

	s.pushLine("uniform vec4 screenSize;");

	ShaderBuilder commonVert(s);

	commonVert.pushLine("#define SMAA_INCLUDE_PS 0");
	commonVert.pushLine("#define SMAA_INCLUDE_VS 1");

	commonVert.pushFile("smaa.h");

	ShaderBuilder commonFrag(s);

	commonFrag.pushLine("#define SMAA_INCLUDE_PS 1");
	commonFrag.pushLine("#define SMAA_INCLUDE_VS 0");

	commonFrag.pushFile("smaa.h");

	glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);
	{
		ShaderBuilder vert(commonVert);

		vert.pushVertexAttr("vec2 pos;");
		vert.pushVertexVarying("vec2 texcoord;");
		vert.pushVertexVarying("vec4 offset0;");
		vert.pushVertexVarying("vec4 offset1;");
		vert.pushVertexVarying("vec4 offset2;");
		vert.pushLine("void main(void)");
		vert.pushLine("{");
		vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
		vert.pushLine("    vec4 offsets[3];");
		vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    SMAAEdgeDetectionVS(texcoord, offsets);");
		vert.pushLine("    offset0 = offsets[0];");
		vert.pushLine("    offset1 = offsets[1];");
		vert.pushLine("    offset2 = offsets[2];");
		vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
		vert.pushLine("}");

		VertexShader vShader("smaaEdge.vert", vert);

		ShaderBuilder frag(commonFrag);

		frag.pushLine("uniform sampler2D color;");
		frag.pushFragmentVarying("vec2 texcoord;");
		frag.pushFragmentVarying("vec4 offset0;");
		frag.pushFragmentVarying("vec4 offset1;");
		frag.pushFragmentVarying("vec4 offset2;");
		frag.pushLine("void main(void)");
		frag.pushLine("{");
		frag.pushLine("    vec4 offsets[3];");
		frag.pushLine("    offsets[0] = offset0;");
		frag.pushLine("    offsets[1] = offset1;");
		frag.pushLine("    offsets[2] = offset2;");
		frag.pushLine("    gl_FragColor = vec4(SMAAColorEdgeDetectionPS(texcoord, offsets, color), 0.0, 0.0);");
		frag.pushLine("}");

		FragmentShader fShader("smaaEdge.frag", frag);

		smaaEdgeShader = std::make_unique<Shader>(vShader, fShader);
		glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}

	{
		ShaderBuilder vert(commonVert);

		vert.pushVertexAttr("vec2 pos;");
		vert.pushVertexVarying("vec2 texcoord;");
		vert.pushVertexVarying("vec2 pixcoord;");
		vert.pushVertexVarying("vec4 offset0;");
		vert.pushVertexVarying("vec4 offset1;");
		vert.pushVertexVarying("vec4 offset2;");
		vert.pushLine("void main(void)");
		vert.pushLine("{");
		vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
		vert.pushLine("    vec4 offsets[3];");
		vert.pushLine("    offsets[0] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    offsets[1] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    offsets[2] = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    pixcoord = vec2(0.0, 0.0);");
		vert.pushLine("    SMAABlendingWeightCalculationVS(texcoord, pixcoord, offsets);");
		vert.pushLine("    offset0 = offsets[0];");
		vert.pushLine("    offset1 = offsets[1];");
		vert.pushLine("    offset2 = offsets[2];");
		vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
		vert.pushLine("}");

		VertexShader vShader("smaaBlendWeight.vert", vert);

		ShaderBuilder frag(commonFrag);

		frag.pushLine("uniform sampler2D edgesTex;");
		frag.pushLine("uniform sampler2D areaTex;");
		frag.pushLine("uniform sampler2D searchTex;");
		frag.pushFragmentVarying("vec2 texcoord;");
		frag.pushFragmentVarying("vec2 pixcoord;");
		frag.pushFragmentVarying("vec4 offset0;");
		frag.pushFragmentVarying("vec4 offset1;");
		frag.pushFragmentVarying("vec4 offset2;");
		frag.pushLine("void main(void)");
		frag.pushLine("{");
		frag.pushLine("    vec4 offsets[3];");
		frag.pushLine("    offsets[0] = offset0;");
		frag.pushLine("    offsets[1] = offset1;");
		frag.pushLine("    offsets[2] = offset2;");
		frag.pushLine("    gl_FragColor = SMAABlendingWeightCalculationPS(texcoord, pixcoord, offsets, edgesTex, areaTex, searchTex, vec4(0.0, 0.0, 0.0, 0.0));");
		frag.pushLine("}");

		FragmentShader fShader("smaaBlendWeight.frag", frag);

		smaaBlendWeightShader = std::make_unique<Shader>(vShader, fShader);
		glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}

	{
		ShaderBuilder vert(commonVert);

		vert.pushVertexAttr("vec2 pos;");
		vert.pushVertexVarying("vec2 texcoord;");
		vert.pushVertexVarying("vec4 offset;");
		vert.pushLine("void main(void)");
		vert.pushLine("{");
		vert.pushLine("    texcoord = pos * 0.5 + 0.5;");
		vert.pushLine("    offset = vec4(0.0, 0.0, 0.0, 0.0);");
		vert.pushLine("    SMAANeighborhoodBlendingVS(texcoord, offset);");
		vert.pushLine("    gl_Position = vec4(pos, 1.0, 1.0);");
		vert.pushLine("}");

		VertexShader vShader("smaaNeighbor.vert", vert);

		ShaderBuilder frag(commonFrag);

		frag.pushLine("uniform sampler2D blendTex;");
		frag.pushLine("uniform sampler2D colorTex;");
		frag.pushFragmentVarying("vec2 texcoord;");
		frag.pushFragmentVarying("vec4 offset;");
		frag.pushLine("void main(void)");
		frag.pushLine("{");
		frag.pushLine("    gl_FragColor = SMAANeighborhoodBlendingPS(texcoord, offset, colorTex, blendTex);");
		frag.pushLine("}");

		FragmentShader fShader("smaaNeighbor.frag", frag);

		smaaNeighborShader = std::make_unique<Shader>(vShader, fShader);
		glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}
}


void SMAADemo::initRender() {
	assert(window == NULL);
	assert(context == NULL);

	// TODO: fullscreen, resizable, highdpi etc. as necessary
	// TODO: check errors
	// TODO: other GL attributes as necessary
	// TODO: use core context (and maybe debug as necessary)
#ifndef EMSCRIPTEN

	if (glES) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	} else {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	}

#endif  // EMSCRIPTEN

	SDL_DisplayMode mode;
	memset(&mode, 0, sizeof(mode));
	int numDisplays = SDL_GetNumVideoDisplays();
	printf("Number of displays detected: %i\n", numDisplays);

	for (int i = 0; i < numDisplays; i++) {
		int numModes = SDL_GetNumDisplayModes(i);
		printf("Number of display modes for display %i : %i\n", i, numModes);

		for (int j = 0; j < numModes; j++) {
			SDL_GetDisplayMode(i, j, &mode);
			printf("Display mode %i : width %i, height %i, BPP %i\n", j, mode.w, mode.h, SDL_BITSPERPIXEL(mode.format));
		}
	}

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	window = SDL_CreateWindow("SMAA Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, flags);

	context = SDL_GL_CreateContext(window);

	// TODO: call SDL_GL_GetDrawableSize, log GL attributes etc.

	applyVSync();

#ifdef USE_GLEW
	glewExperimental = true;
	glewInit();
#endif  // USE_GLEW

	// TODO: check extensions
	// at least direct state access, texture storage

	// swap once to get better traces
	SDL_GL_SwapWindow(window);

	buildCubeShader();
	buildSMAAShaders();
	buildFXAAShader();

	// TODO: DSA
	glGenBuffers(1, &cubeVBO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &cubeIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &fullscreenVBO);
	glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVertices), &fullscreenVertices[0], GL_STATIC_DRAW);

	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), NULL, GL_STREAM_DRAW);

	glGenTextures(1, &areaTex);
	glTextureStorage2DEXT(areaTex, GL_TEXTURE_2D, 1, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT);
	std::vector<unsigned char> tempBuffer(std::max(AREATEX_SIZE, SEARCHTEX_SIZE), 0);
	for (unsigned int y = 0; y < AREATEX_HEIGHT; y++) {
		unsigned int srcY = AREATEX_HEIGHT - 1 - y;
		//unsigned int srcY = y;
		memcpy(&tempBuffer[y * AREATEX_PITCH], areaTexBytes + srcY * AREATEX_PITCH, AREATEX_PITCH);
	}
	glTextureSubImage2DEXT(areaTex, GL_TEXTURE_2D, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	glTextureParameteriEXT(areaTex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(areaTex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(areaTex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(areaTex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_AREATEX, GL_TEXTURE_2D, areaTex);

	glGenTextures(1, &searchTex);
	glTextureStorage2DEXT(searchTex, GL_TEXTURE_2D, 1, GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT);
	for (unsigned int y = 0; y < SEARCHTEX_HEIGHT; y++) {
		unsigned int srcY = SEARCHTEX_HEIGHT - 1 - y;
		//unsigned int srcY = y;
		memcpy(&tempBuffer[y * SEARCHTEX_PITCH], searchTexBytes + srcY * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
	}
	glTextureSubImage2DEXT(searchTex, GL_TEXTURE_2D, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, &tempBuffer[0]);
	glTextureParameteriEXT(searchTex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(searchTex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(searchTex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(searchTex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_SEARCHTEX, GL_TEXTURE_2D, searchTex);

	builtinFBO = std::make_unique<Framebuffer>(0);
	builtinFBO->width = windowWidth;
	builtinFBO->height = windowHeight;

	createFramebuffers();
}


void SMAADemo::setCubeVBO() {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glVertexAttribPointer(ATTR_POS, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glVertexAttribPointer(ATTR_CUBEPOS, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, x));
	glVertexAttribDivisor(ATTR_CUBEPOS, 1);

	glVertexAttribPointer(ATTR_ROT, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, qx));
	glVertexAttribDivisor(ATTR_ROT, 1);

	glVertexAttribPointer(ATTR_COLOR, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(InstanceData), VBO_OFFSETOF(InstanceData, col));
	glVertexAttribDivisor(ATTR_COLOR, 1);

	glEnableVertexAttribArray(ATTR_POS);
	glEnableVertexAttribArray(ATTR_CUBEPOS);
	glEnableVertexAttribArray(ATTR_ROT);
	glEnableVertexAttribArray(ATTR_COLOR);
}


void SMAADemo::setFullscreenVBO() {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
	glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, NULL);

	glEnableVertexAttribArray(ATTR_POS);
	glDisableVertexAttribArray(ATTR_CUBEPOS);
	glDisableVertexAttribArray(ATTR_ROT);
	glDisableVertexAttribArray(ATTR_COLOR);
}


void SMAADemo::createFramebuffers()	{
	renderFBO.reset();
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
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glNamedFramebufferTextureEXT(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	tex = 0;
	glGenTextures(1, &tex);
	renderFBO->depthTex = tex;
	glTextureStorage2DEXT(tex, GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glNamedFramebufferTextureEXT(fbo, GL_DEPTH_ATTACHMENT, tex, 0);

	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_COLOR, GL_TEXTURE_2D, renderFBO->colorTex);

	// SMAA edges texture and FBO
	edgesFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	edgesFBO = std::make_unique<Framebuffer>(fbo);
	edgesFBO->width = windowWidth;
	edgesFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glGenTextures(1, &tex);
	edgesFBO->colorTex = tex;
	glTextureStorage2DEXT(tex, GL_TEXTURE_2D, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glNamedFramebufferTextureEXT(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_EDGES, GL_TEXTURE_2D, edgesFBO->colorTex);

	// SMAA blending weights texture and FBO
	blendFBO.reset();
	fbo = 0;
	glGenFramebuffers(1, &fbo);
	blendFBO = std::make_unique<Framebuffer>(fbo);
	blendFBO->width = windowWidth;
	blendFBO->height = windowHeight;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	tex = 0;
	glGenTextures(1, &tex);
	blendFBO->colorTex = tex;
	glTextureStorage2DEXT(tex, GL_TEXTURE_2D, 1, GL_RGBA8, windowWidth, windowHeight);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glNamedFramebufferTextureEXT(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

	glBindMultiTextureEXT(GL_TEXTURE0 + TEXUNIT_BLEND, GL_TEXTURE_2D, blendFBO->colorTex);
}


void SMAADemo::applyVSync() {
	if (vsync) {
		// enable vsync, using late swap tearing if possible
		int retval = SDL_GL_SetSwapInterval(-1);
		if (retval != 0) {
			// TODO: check return val
			SDL_GL_SetSwapInterval(1);
		}
		printf("VSync is on\n");
	} else {
		// TODO: check return val
		SDL_GL_SetSwapInterval(0);
		printf("VSync is off\n");
	}
}


void SMAADemo::applyFullscreen() {
#ifndef EMSCRIPTEN
	// emscripten doesn't allow program-initiated fullscreen without exterme trickery

	if (fullscreen) {
		// TODO: check return val?
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		printf("Fullscreen\n");
	} else {
		SDL_SetWindowFullscreen(window, 0);
		printf("Windowed\n");
	}
#endif  // EMSCRIPTEN
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

	for (unsigned int x = 0; x < cubesSide; x++) {
		for (unsigned int y = 0; y < cubesSide; y++) {
			for (unsigned int z = 0; z < cubesSide; z++) {
				float qx = random.randFloat();
				float qy = random.randFloat();
				float qz = random.randFloat();
				float qw = random.randFloat();
				float reciprocLen = 1.0f / sqrtf(qx*qx + qy*qy + qz*qz + qw*qw);
				qx *= reciprocLen;
				qy *= reciprocLen;
				qz *= reciprocLen;
				qw *= reciprocLen;

				cubes.emplace_back((x * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (y * cubeDistance) - (bigCubeSide / 2.0f)
				                 , (z * cubeDistance) - (bigCubeSide / 2.0f)
				                 , glm::quat(qx, qy, qz, qw)
				                 , white);
			}
		}
	}

	// reallocate instance data buffer
	glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData) * numCubes, NULL, GL_STREAM_DRAW);

	colorCubes();
}


void SMAADemo::colorCubes() {
	if (colorMode == 0) {
		for (auto &cube : cubes) {
			Color col;
			// random RGB, alpha = 1.0
			// FIXME: we're abusing little-endianness, make it portable
			col.val = random.randU32() | 0xFF000000;
			cube.col = col;
		}
	} else {
		for (auto &cube : cubes) {
			Color col;
			// YCbCr, fixed luma, random chroma, alpha = 1.0
			// worst case scenario for luma edge detection
			// TODO: use the same luma as shader

			float y = 0.5f;
			const float c_red = 0.299
				, c_green = 0.587
			, c_blue = 0.114;
			float cb = random.randFloat();
			float cr = random.randFloat();

			float r = cr * (2 - 2 * c_red) + y;
			float g = (y - c_blue * cb - c_red * cr) / c_green;
			float b = cb * (2 - 2 * c_blue) + y;

			col.r = 255 * r;
			col.g = 255 * g;
			col.b = 255 * b;
			col.a = 0xFF;
			cube.col = col;
		}
	}
}


static void printHelp() {
	printf(" a     - toggle antialiasing on/off\n");
	printf(" c     - re-color cubes\n");
	printf(" h     - print help\n");
	printf(" m     - change antialiasing method\n");
	printf(" q     - toggle quality\n");
	printf(" v     - toggle vsync\n");
	printf(" SPACE - toggle camera rotation\n");
	printf(" ESC   - quit\n");
}


void SMAADemo::mainLoopIteration() {
		// TODO: timing
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				keepGoing = false;
				break;

			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_ESCAPE:
					keepGoing = false;
					break;

				case SDL_SCANCODE_LSHIFT:
					leftShift = true;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = true;
					break;

				case SDL_SCANCODE_SPACE:
					rotateCamera = !rotateCamera;
					printf("camera rotation is %s\n", rotateCamera ? "on" : "off");
					break;

				case SDL_SCANCODE_A:
					antialiasing = !antialiasing;
					printf("antialiasing set to %s\n", antialiasing ? "on" : "off");
					break;

				case SDL_SCANCODE_C:
					if (rightShift || leftShift) {
						colorMode = (colorMode + 1) % 2;
						printf("color mode set to %s\n", colorMode ? "YCbCr" : "RGB");
					}
					colorCubes();
					break;

				case SDL_SCANCODE_D:
					if (antialiasing && aaMethod == AAMethod::SMAA) {
						if (leftShift || rightShift) {
							debugMode = (debugMode + 3 - 1) % 3;
						} else {
							debugMode = (debugMode + 1) % 3;
						}
						printf("Debug mode set to %s\n", smaaDebugModeStr(debugMode));
					}
					break;

				case SDL_SCANCODE_H:
					printHelp();
					break;

				case SDL_SCANCODE_M:
					aaMethod = AAMethod::AAMethod((int(aaMethod) + 1) % (int(AAMethod::LAST) + 1));
					printf("aa method set to %s\n", AAMethod::name(aaMethod));
					break;

				case SDL_SCANCODE_Q:
					switch (aaMethod) {
					case AAMethod::FXAA:
						if (leftShift || rightShift) {
							fxaaQuality = fxaaQuality + maxFXAAQuality - 1;
						} else {
							fxaaQuality = fxaaQuality + 1;
						}
						fxaaQuality = fxaaQuality % maxFXAAQuality;
						buildFXAAShader();
						printf("FXAA quality set to %s (%u)\n", fxaaQualityLevels[fxaaQuality], fxaaQuality);
						break;

					case AAMethod::SMAA:
						if (leftShift || rightShift) {
							smaaQuality = smaaQuality + maxSMAAQuality - 1;
						} else {
							smaaQuality = smaaQuality + 1;
						}
						smaaQuality = smaaQuality % maxSMAAQuality;
						buildSMAAShaders();
						printf("SMAA quality set to %s (%u)\n", smaaQualityLevels[smaaQuality], smaaQuality);
						break;

					}
					break;

				case SDL_SCANCODE_V:
					vsync = !vsync;
					applyVSync();
					break;

				case SDL_SCANCODE_F:
					fullscreen = !fullscreen;
					applyFullscreen();
					break;

				default:
					break;
				}
				break;

			case SDL_KEYUP:
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_LSHIFT:
					leftShift = false;
					break;

				case SDL_SCANCODE_RSHIFT:
					rightShift = false;
					break;

				default:
					break;
				}
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					resizeWidth = event.window.data1;
					resizeHeight = event.window.data2;
					break;
				default:
					break;
				}
			}
		}

		render();
}


void SMAADemo::addImage(const char *filename) {
	int width = 0, height = 0;
	unsigned char *imageData = stbi_load(filename, &width, &height, NULL, 3);
	printf(" %p  %dx%d\n", imageData, width, height);

	images.push_back(Image());
	Image &img = images.back();
	img.filename = filename;
	glGenTextures(1, &img.tex);

	// flip it
	std::vector<unsigned char> temp(3 * width * height, 0);
	for (int i = 0; i < height; i++) {
		memcpy(&temp[i * width * 3], &imageData[(height - 1 - i) * width * 3], width * 3);
	}

	glTextureStorage2DEXT(img.tex, GL_TEXTURE_2D, 1, GL_RGB8, width, height);
	glTextureSubImage2DEXT(img.tex, GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, &temp[0]);
	glTextureParameteriEXT(img.tex, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteriEXT(img.tex, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteriEXT(img.tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteriEXT(img.tex, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	stbi_image_free(imageData);
}


void SMAADemo::render() {
	if (resizeWidth != windowWidth || resizeHeight != windowHeight) {
		windowWidth = resizeWidth;
		windowHeight = resizeHeight;
		createFramebuffers();

		glm::vec4 screenSize = glm::vec4(1.0f / float(windowWidth), 1.0f / float(windowHeight), windowWidth, windowHeight);

		glUniform4fv(fxaaShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaEdgeShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaBlendWeightShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
		glUniform4fv(smaaNeighborShader->getScreenSizeLocation(), 1, glm::value_ptr(screenSize));
	}

	uint64_t ticks = SDL_GetPerformanceCounter();
	uint64_t elapsed = ticks - lastTime;

	lastTime = ticks;

	// TODO: reset all relevant state in case some 3rd-party program fucked them up

	glViewport(0, 0, windowWidth, windowHeight);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	builtinFBO->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	renderFBO->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	cubeShader->bind();

	if (rotateCamera) {
		rotationTime += elapsed;

		const uint64_t rotationPeriod = 30 * freq;
		rotationTime = rotationTime % rotationPeriod;
		cameraRotation = float(M_PI * 2.0f * rotationTime) / rotationPeriod;
	}
	glm::mat4 view = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -25.0f)), cameraRotation, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(float(65.0f * M_PI * 2.0f / 360.0f), float(windowWidth) / windowHeight, 0.1f, 100.0f);
	glm::mat4 viewProj = proj * view;
	glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

	instances.clear();
	instances.reserve(cubes.size());
	for (const auto &cube : cubes) {
		instances.emplace_back(cube.orient, cube.pos, cube.col);
	}

	setCubeVBO();
	// FIXME: depends on instance data vbo remaining bound
	// use dsa instead
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(InstanceData) * instances.size(), &instances[0]);

	glDrawElementsInstanced(GL_TRIANGLES, 3 * 2 * 6, GL_UNSIGNED_INT, NULL, cubes.size());

	setFullscreenVBO();

	if (antialiasing) {
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		switch (aaMethod) {
		case AAMethod::FXAA:
			builtinFBO->bind();
			fxaaShader->bind();
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;

		case AAMethod::SMAA:
			smaaEdgeShader->bind();

			if (debugMode == 1) {
				// detect edges only
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				edgesFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			smaaBlendWeightShader->bind();
			if (debugMode == 2) {
				// show blending weights
				builtinFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				break;
			} else {
				blendFBO->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			// full effect
			smaaNeighborShader->bind();
			builtinFBO->bind();
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			break;
		}

	} else {
		renderFBO->blitTo(*builtinFBO);
	}

	SDL_GL_SwapWindow(window);
}


#ifdef EMSCRIPTEN


static void mainLoopWrapper(void *smaaDemo_) {
	SMAADemo *demo = reinterpret_cast<SMAADemo *>(smaaDemo_);
	demo->mainLoopIteration();
}


#endif  // EMSCRIPTEN


int main(int argc, char *argv[]) {
	try {
	auto demo = std::make_unique<SMAADemo>();

	demo->initRender();
	demo->createCubes();
	printHelp();

	// TODO: better command line parsing
	for (int i = 1; i < argc; i++) {
		printf("image \"%s\"\n", argv[i]);
		demo->addImage(argv[i]);
	}

#ifdef EMSCRIPTEN

	emscripten_set_main_loop_arg(mainLoopWrapper, &demo, 0, 1);

#else  // EMSCRIPTEN

	while (demo->shouldKeepGoing()) {
		demo->mainLoopIteration();
	}

#endif  // EMSCRIPTEN

	} catch (std::exception &e) {
		printf("caught std::exception \"%s\"\n", e.what());
		// so native dumps core
		throw;
	} catch (...) {
		printf("unknown exception\n");
		// so native dumps core
		throw;
	}
	return 0;
}
