#include "Renderer.h"
#include "Utils.h"


std::vector<char> Renderer::loadSource(const std::string &name) {
	auto it = shaderSources.find(name);
	if (it != shaderSources.end()) {
		return it->second;
	} else {
		auto source = readTextFile(name);
		shaderSources.emplace(name, source);
		return source;
	}
}
