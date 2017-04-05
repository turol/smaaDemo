#ifdef RENDERER_OPENGL


#include <cassert>

#include <algorithm>
#include <string>
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


#endif //  RENDERER_OPENGL
