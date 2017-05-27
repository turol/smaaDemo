#ifndef RENDERERINTERNAL_H
#define RENDERERINTERNAL_H


class Includer final : public shaderc::CompileOptions::IncluderInterface {
	std::unordered_map<std::string, std::vector<char> > cache;


	virtual shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type /* type */, const char* /* requesting_source */, size_t /* include_depth */) {
		std::string filename(requested_source);

		// std::unordered_map<std::string, std::vector<char> >::iterator it = cache.find(filename);
		auto it = cache.find(filename);
		if (it == cache.end()) {
			auto contents = readFile(requested_source);
			bool inserted = false;
			std::tie(it, inserted) = cache.emplace(std::move(filename), std::move(contents));
			// since we just checked it's not there this must succeed
			assert(inserted);
		}

		auto data = new shaderc_include_result;
		data->source_name         = it->first.c_str();
		data->source_name_length  = it->first.size();
		data->content             = it->second.data();
		data->content_length      = it->second.size();
		data->user_data           = nullptr;

		return data;
	}

	virtual void ReleaseInclude(shaderc_include_result* data) {
		// no need to delete any of data's contents, they're owned by this class
		delete data;
	}
};


#endif  // RENDERERINTERNAL_H
