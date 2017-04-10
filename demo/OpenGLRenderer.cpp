#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <vector>

#include "Renderer.h"
#include "Utils.h"


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


ShaderBuilder::ShaderBuilder()
{
	source.reserve(512);

		pushLine("#version 450");

		if (GLEW_ARB_gpu_shader5) {
			pushLine("#extension GL_ARB_gpu_shader5 : enable");
		}
		if (GLEW_ARB_texture_gather) {
			pushLine("#extension GL_ARB_texture_gather : enable");
		}
}


ShaderBuilder::ShaderBuilder(const ShaderBuilder &other)
{
	source = other.source;
}


void ShaderBuilder::pushLine(const std::string &line) {
	source.reserve(source.size() + line.size() + 1);
	source.insert(source.end(), line.begin(), line.end());
	source.push_back('\n');
}


void ShaderBuilder::pushVertexAttr(const std::string &attr) {
		pushLine("in " + attr);
}


void ShaderBuilder::pushVertexVarying(const std::string &var) {
		pushLine("out " + var);
}


void ShaderBuilder::pushFragmentVarying(const std::string &var) {
		pushLine("in " + var);
}


void ShaderBuilder::pushFragmentOutput(const std::string &expr) {
		pushLine("    outColor = " + expr);
}


void ShaderBuilder::pushFragmentOutputDecl() {
		pushLine("out vec4 outColor;");
}


void ShaderBuilder::pushFile(const std::string &filename) {
	// TODO: grab file here, don't use #include
	// which we'll just end up parsing back later
	pushLine("#include \"" + filename + "\"");
}


VertexShader::VertexShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
{
	shader = createShader(GL_VERTEX_SHADER, name, builder.source);
}


VertexShader::VertexShader(const std::string &name)
: shader(0)
{
	auto source = readTextFile(name);
	shader = createShader(GL_VERTEX_SHADER, name, source);
}


VertexShader::~VertexShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
}


FragmentShader::FragmentShader(const std::string &name, const ShaderBuilder &builder)
: shader(0)
{
	shader = createShader(GL_FRAGMENT_SHADER, name, builder.source);
}


FragmentShader::FragmentShader(const std::string &name)
: shader(0)
{
	auto source = readTextFile(name);
	shader = createShader(GL_FRAGMENT_SHADER, name, source);
}


FragmentShader::~FragmentShader() {
	assert(shader != 0);

	glDeleteShader(shader);
	shader = 0;
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

		glBindFragDataLocation(program, 0, "outColor");

	GLint colorLoc = getUniformLocation("colorTex");
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


GLint Shader::getScreenSizeLocation() const {
	return screenSizeLoc;
}


void Shader::bind() {
	assert(program != 0);

	glUseProgram(program);
}


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


static const char *errorSource(GLenum source)
{
	switch (source)
	{
	case GL_DEBUG_SOURCE_API:
		return "API";
		break;

	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return "window system";
		break;

	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return "shader compiler";
		break;

	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return "third party";
		break;

	case GL_DEBUG_SOURCE_APPLICATION:
		return "application";
		break;

	case GL_DEBUG_SOURCE_OTHER:
		return "other";
		break;

	default:
		break;
	}

	return "unknown source";
}


static const char *errorType(GLenum type)
{
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
	case GL_DEBUG_CATEGORY_API_ERROR_AMD:
		return "error";
		break;

	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
		return "deprecated behavior";
		break;

	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
	case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
		return "undefined behavior";
		break;

	case GL_DEBUG_TYPE_PORTABILITY:
		return "portability";
		break;

	case GL_DEBUG_TYPE_PERFORMANCE:
	case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
		return "performance";
		break;

	case GL_DEBUG_TYPE_OTHER:
	case GL_DEBUG_CATEGORY_OTHER_AMD:
		return "other";
		break;

	case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
		return "window system error";
		break;

	case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
		return "shader compiler error";
		break;

	case GL_DEBUG_CATEGORY_APPLICATION_AMD:
		return "application error";
		break;

	default:
		break;

	}

	return "unknown type";
}


void GLAPIENTRY glDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /* length */, const GLchar *message, const void * /* userParam */)
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH_ARB:
		printf("GL error from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		printf("GL warning from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	case GL_DEBUG_SEVERITY_LOW_ARB:
		printf("GL debug from %s type %s: (%d) %s\n", errorSource(source), errorType(type), id, message);
		break;

	default:
		printf("GL error of unknown severity %x from %s type %s: (%d) %s\n", severity, errorSource(source), errorType(type), id, message);
		break;
	}
}


#endif //  RENDERER_OPENGL
