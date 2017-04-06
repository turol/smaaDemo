#ifndef RENDERER_H
#define RENDERER_H


#define ATTR_POS   0
#define ATTR_COLOR   1
#define ATTR_CUBEPOS 2
#define ATTR_ROT     3


#define TEXUNIT_TEMP 0
#define TEXUNIT_COLOR 1
#define TEXUNIT_AREATEX 2
#define TEXUNIT_SEARCHTEX 3
#define TEXUNIT_EDGES 4
#define TEXUNIT_BLEND 5


class FragmentShader;
class Framebuffer;
class Shader;
class ShaderBuilder;
class VertexShader;


#ifdef RENDERER_OPENGL


#include <GL/glew.h>


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */);


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


class ShaderBuilder {
	std::vector<char> source;

	ShaderBuilder &operator=(const ShaderBuilder &) = delete;

	ShaderBuilder(ShaderBuilder &&) = delete;
	ShaderBuilder &operator=(ShaderBuilder &&) = delete;

	friend class VertexShader;
	friend class FragmentShader;

public:
	ShaderBuilder();
	ShaderBuilder(const ShaderBuilder &other);

	VertexShader compileVertex();
	FragmentShader compileFragment();

	void pushLine(const std::string &line);
	void pushVertexAttr(const std::string &attr);
	void pushVertexVarying(const std::string &var);
	void pushFragmentVarying(const std::string &var);
	void pushFragmentOutputDecl();
	void pushFragmentOutput(const std::string &expr);
	void pushFile(const std::string &filename);
};


class VertexShader {
	GLuint shader;

	VertexShader() = delete;

	VertexShader(const VertexShader &) = delete;
	VertexShader &operator=(const VertexShader &) = delete;

	VertexShader(VertexShader &&) = delete;
	VertexShader &operator=(VertexShader &&) = delete;

	friend class Shader;

public:

	VertexShader(const std::string &name, const ShaderBuilder &builder);
	explicit VertexShader(const std::string &name);

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

	FragmentShader(const std::string &name, const ShaderBuilder &builder);
	explicit FragmentShader(const std::string &name);

	~FragmentShader();
};


class Shader {
    GLuint program;

	GLint screenSizeLoc;

	Shader() = delete;
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;

	Shader(Shader &&) = delete;
	Shader &operator=(Shader &&) = delete;

public:
	Shader(const VertexShader &vertexShader, const FragmentShader &fragmentShader);

	~Shader();

	GLint getUniformLocation(const char *name);

	GLint getScreenSizeLocation() const;

	void bind();
};


#elif defined(RENDERER_VULKAN)

#include <vulkan/vulkan.hpp>


#elif defined(RENDERER_NULL)

#else

#error "No renderer specififued"


#endif  // RENDERER



#endif  // RENDERER_H
