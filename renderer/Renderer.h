/*
Copyright (c) 2015-2023 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#ifndef RENDERER_H
#define RENDERER_H


#include <algorithm>
#include <array>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 1
#include <glm/glm.hpp>

#include <SDL.h>

#include <magic_enum.hpp>
#include <magic_enum_containers.hpp>

#include "utils/Hash.h"
#include "utils/Utils.h"  // for isPow2


namespace renderer {


#define MAX_COLOR_RENDERTARGETS 2
#define MAX_VERTEX_ATTRIBS      4
#define MAX_VERTEX_BUFFERS      1
#define MAX_DESCRIPTOR_SETS     2  // per pipeline
#define MAX_TEXTURE_MIPLEVELS   14
#define MAX_TEXTURE_SIZE        (1 << (MAX_TEXTURE_MIPLEVELS - 1))


struct Buffer;
struct DescriptorSetLayout;
struct Framebuffer;
struct GraphicsPipeline;
struct RenderPass;
struct RenderTarget;
struct Sampler;
struct Texture;


// owned is whether the container owns the resources or not
// when false they're owned by the caller
template <class T, typename HandleBaseType = uint32_t, bool owned = false> class ResourceContainer;


// #define HANDLE_OWNERSHIP_DEBUG 1


#ifdef HANDLE_OWNERSHIP_DEBUG


template <class T, typename BaseType = uint32_t>
class Handle {
	using HandleType = BaseType;

	friend class ResourceContainer<T, BaseType, true>;
	friend class ResourceContainer<T, BaseType, false>;

	HandleType  handle = 0;
	bool        owned  = false;


	Handle(HandleType handle_, bool owned_ = true)
	: handle(handle_)
	, owned(owned_)
	{
	}

public:


	Handle() {
	}


	~Handle() {
		assert(!owned);
	}


	// copying a Handle, new copy is not owned no matter what
	Handle(const Handle<T, HandleType> &other)
	: handle(other.handle)
	{
	}


	Handle &operator=(const Handle<T, HandleType> &other) noexcept {
		if (this != &other) {
			assert(!this->owned && "Overwriting an owned handle when copying");
			handle = other.handle;
		}

		return *this;
	}


	// Moving an owned handle takes ownership
	Handle(Handle<T, HandleType> &&other) noexcept {
		handle       = other.handle;
		owned        = other.owned;

		other.handle = 0;
		other.owned  = false;
	}


	Handle<T, HandleType> &operator=(Handle<T, HandleType> &&other) noexcept {
		if (this != &other) {
			assert(!this->owned && "Overwriting an owned handle when moving");
			handle       = other.handle;
			owned        = other.owned;

			other.handle = 0;
            other.owned  = false;
		}

		return *this;
	}


	bool operator==(const Handle<T, HandleType> &other) const {
		return handle == other.handle;
	}


	bool operator!=(const Handle<T, HandleType> &other) const {
		return handle != other.handle;
	}


	explicit operator bool() const {
		return handle != 0;
	}


	void reset() {
		assert(!owned);
		handle = 0;
	}


	size_t hashValue() const {
		return std::hash<HandleType>()(handle);
	}
};


// this is different from other Handles because it's not backed by a Resource
class DebugGroupHandle {
	friend class Renderer;

	unsigned int  count = 0;
	bool          owned = false;


	explicit DebugGroupHandle(unsigned int count_)
	: count(count_)
	, owned(true)
	{
	}

public:


	DebugGroupHandle() {
	}


	~DebugGroupHandle() {
		assert(count == 0);
		assert(!owned);
	}


	// copying a Handle, new copy is not owned no matter what
	DebugGroupHandle(const DebugGroupHandle &other)
	: count(other.count)
	{
	}


	DebugGroupHandle &operator=(const DebugGroupHandle &other) noexcept {
		if (this != &other) {
			assert(!this->owned && "Overwriting an owned handle when copying");
			count = other.count;
		}

		return *this;
	}


	// Moving an owned handle takes ownership
	DebugGroupHandle(DebugGroupHandle &&other) noexcept {
		count        = other.count;
		owned        = other.owned;

		other.count  = 0;
		other.owned  = false;
	}


	DebugGroupHandle &operator=(DebugGroupHandle &&other) noexcept {
		if (this != &other) {
			assert(!this->owned && "Overwriting an owned handle when moving");
			count        = other.count;
			owned        = other.owned;

			other.count  = 0;
			other.owned  = false;
		}

		return *this;
	}
};


#else  // HANDLE_OWNERSHIP_DEBUG


template <class T, typename BaseType = uint32_t>
class Handle {
	using HandleType = BaseType;

	friend class ResourceContainer<T, BaseType, true>;
	friend class ResourceContainer<T, BaseType, false>;

	HandleType handle = 0;


	explicit Handle(HandleType handle_)
	: handle(handle_)
	{
	}

public:


	Handle() {
	}


	~Handle() {
	}


	Handle(const Handle<T, HandleType> &other)                     = default;
	Handle &operator=(const Handle<T, HandleType> &other) noexcept = default;

	Handle(Handle<T, HandleType> &&other) noexcept                 = default;
	Handle<T, HandleType> &operator=(Handle<T, HandleType> &&other) noexcept   = default;


	bool operator==(const Handle<T, HandleType> &other) const {
		return handle == other.handle;
	}


	bool operator!=(const Handle<T, HandleType> &other) const {
		return handle != other.handle;
	}


	explicit operator bool() const {
		return handle != 0;
	}


	void reset() {
		handle = 0;
	}


	size_t hashValue() const {
		return std::hash<HandleType>()(handle);
	}
};


// this is different from other Handles because it's not backed by a Resource
class DebugGroupHandle {
	friend class Renderer;

	unsigned int  count = 0;


	explicit DebugGroupHandle(unsigned int count_)
	: count(count_)
	{
	}

public:


	DebugGroupHandle() {
	}


	~DebugGroupHandle() {
		assert(count == 0);
	}


	DebugGroupHandle(const DebugGroupHandle &other)            noexcept = default;
	DebugGroupHandle &operator=(const DebugGroupHandle &other) noexcept = default;

	DebugGroupHandle(DebugGroupHandle &&other)                 noexcept = default;
	DebugGroupHandle &operator=(DebugGroupHandle &&other)      noexcept = default;
};


#endif  // HANDLE_OWNERSHIP_DEBUG


using BufferHandle            = Handle<Buffer>;
using DSLayoutHandle          = Handle<DescriptorSetLayout>;
using FramebufferHandle       = Handle<Framebuffer>;
using GraphicsPipelineHandle  = Handle<GraphicsPipeline>;
using RenderPassHandle        = Handle<RenderPass>;
using RenderTargetHandle      = Handle<RenderTarget>;
using SamplerHandle           = Handle<Sampler>;
using TextureHandle           = Handle<Texture>;


enum class BlendFunc : uint8_t {
	  Zero
	, One
	, Constant
	, SrcAlpha
	, OneMinusSrcAlpha
};


enum class BufferType : uint8_t {
	  Invalid
	, Index
	, Uniform
	, Storage
	, Vertex
	, Everything
};


enum class DescriptorType : uint8_t {
	  UniformBuffer
	, StorageBuffer
	, Sampler
	, Texture
	, CombinedSampler
	, End
};


enum class FilterMode : uint8_t {
	  Nearest
	, Linear
};


enum class Format : uint8_t {
	  Invalid
	, R8
	, RG8
	, RGB8
	, RGBA8
	, sRGBA8
	, BGRA8
	, sBGRA8
	, RG16Float
	, RGBA16Float
	, RGBA32Float
	, Depth16
	, Depth16S8
	, Depth24S8
	, Depth24X8
	, Depth32Float
};


enum class IndexFormat : uint8_t {
	  b32
	, b16
};


enum class Layout : uint8_t {
	  Undefined
	, ShaderRead
	, TransferSrc
	, TransferDst
	, RenderAttachment
	, Present
};


// rendertarget behavior when RenderPass begins
enum class PassBegin : uint8_t {
	  DontCare
	, Keep
	, Clear
};


enum class ShaderLanguage : uint8_t {
	  GLSL
	, HLSL
};


enum class TextureUsage : uint8_t {
	  BlitDestination
	, BlitSource
	, Present
	, RenderTarget
	, ResolveDestination
	, ResolveSource
	, Sampling
};


using TextureUsageSet = magic_enum::containers::bitset<TextureUsage>;


enum class VSync : uint8_t {
	  Off
	, On
	, LateSwapTear
};


enum class VtxFormat : uint8_t {
	  Float
	, UNorm8
};


enum class WrapMode : uint8_t {
	  Clamp
	, Wrap
};


bool isColorFormat(Format format);
bool isDepthFormat(Format format);
bool issRGBFormat(Format format);


// CombinedSampler helper
struct CSampler {
	TextureHandle tex;
	SamplerHandle sampler;


	CSampler()
	{
	}

	CSampler(const CSampler &other)                = default;
	CSampler &operator=(const CSampler &other)     = default;

	CSampler(CSampler &&other) noexcept            = default;
	CSampler &operator=(CSampler &&other) noexcept = default;

	~CSampler() {
	}
};


struct DescriptorLayout {
	DescriptorType  type;
	unsigned int    offset;
	// TODO: stage flags
};


struct ShaderMacro {
	std::string key;
	std::string value;


	ShaderMacro() {}

	template <typename K, typename V>
	ShaderMacro(K &&key_, V &&value_)
	: key(std::forward<K>(key_))
	, value(std::forward<V>(value_))
	{}

	ShaderMacro(const ShaderMacro &other)            = default;
	ShaderMacro& operator=(const ShaderMacro &other) = default;

	ShaderMacro(ShaderMacro &&other) noexcept            = default;
	ShaderMacro& operator=(ShaderMacro &&other) noexcept = default;

	~ShaderMacro() {}

	bool operator<(const ShaderMacro &other) const {
		return this->key < other.key;
	}

	bool operator==(const ShaderMacro &other) const {
		return this->key == other.key;
	}

	size_t hashValue() const {
		size_t h = 0;
		hashCombine(h, key);
		hashCombine(h, value);
		return h;
	}
};


class ShaderMacros {
	// sorted according to key, no duplicates
	std::vector<ShaderMacro> impl;

	friend class Renderer;
	friend struct RendererBase;
	friend struct RendererImpl;

public:

	ShaderMacros() {}

	ShaderMacros(const ShaderMacros &other)            = default;
	ShaderMacros& operator=(const ShaderMacros &other) = default;

	ShaderMacros(ShaderMacros &&other) noexcept            = default;
	ShaderMacros& operator=(ShaderMacros &&other) noexcept = default;

	~ShaderMacros() {}

	void set(std::string_view key, std::string_view value) {
		for (auto &macro : impl) {
			if (macro.key == key) {
				macro.value = std::string(value);
				return;
			}
		}
		assert(std::is_sorted(impl.begin(), impl.end()));
		impl.emplace_back(key, value);
		std::sort(impl.begin(), impl.end());
	}

	bool operator!=(const ShaderMacros &other) const {
		return this->impl != other.impl;
	}

	bool empty() const {
		return impl.empty();
	}

	size_t hashValue() const {
		size_t h = 0;

		for (const ShaderMacro &m : impl) {
			hashCombine(h, m.hashValue());
		}

		return h;
	}
};


uint32_t formatSize(Format format);


template <class Derived>
struct DescBase {

protected:
	std::string  name_;

public:

	DescBase() {}

	DescBase(const DescBase &other)                = default;
	DescBase &operator=(const DescBase &other)     = default;

	DescBase(DescBase &&other) noexcept            = default;
	DescBase &operator=(DescBase &&other) noexcept = default;

	~DescBase() {}

	Derived &name(const std::string &str) {
		assert(!str.empty());
		name_ = str;

		return *static_cast<Derived *>(this);
	}
};


struct FramebufferDesc : public DescBase<FramebufferDesc> {
	FramebufferDesc()
	{
	}

	~FramebufferDesc() { }

	FramebufferDesc(const FramebufferDesc &)                = default;
	FramebufferDesc &operator=(const FramebufferDesc &)     = default;

	FramebufferDesc(FramebufferDesc &&) noexcept            = default;
	FramebufferDesc &operator=(FramebufferDesc &&) noexcept = default;


	FramebufferDesc &renderPass(RenderPassHandle rp) {
		renderPass_ = rp;
		return *this;
	}

	FramebufferDesc &depthStencil(RenderTargetHandle ds) {
		depthStencil_ = ds;
		return *this;
	}

	FramebufferDesc &color(unsigned int index, RenderTargetHandle c) {
		assert(index < MAX_COLOR_RENDERTARGETS);
		colors_[index] = c;
		return *this;
	}


private:

	RenderPassHandle                                         renderPass_;
	RenderTargetHandle                                       depthStencil_;
	std::array<RenderTargetHandle, MAX_COLOR_RENDERTARGETS>  colors_;

	friend class Renderer;
	friend struct RendererImpl;
};


template <class Derived>
class PipelineDescBase : public DescBase<Derived> {

protected:

	ShaderMacros                                     shaderMacros_;
	ShaderLanguage                                   shaderLanguage_   = ShaderLanguage::GLSL;
	std::array<DSLayoutHandle, MAX_DESCRIPTOR_SETS>  descriptorSetLayouts;

public:

	PipelineDescBase() {}

	PipelineDescBase(const PipelineDescBase &other)                = default;
	PipelineDescBase &operator=(const PipelineDescBase &other)     = default;

	PipelineDescBase(PipelineDescBase &&other) noexcept            = default;
	PipelineDescBase &operator=(PipelineDescBase &&other) noexcept = default;

	~PipelineDescBase() {}

	Derived &shaderMacros(const ShaderMacros &m) {
		shaderMacros_ = m;
		return *static_cast<Derived *>(this);
	}

	Derived &shaderLanguage(ShaderLanguage lang) {
		shaderLanguage_ = lang;
		return *static_cast<Derived *>(this);
	}

	Derived &descriptorSetLayout(unsigned int index, DSLayoutHandle handle) {
		assert(index < MAX_DESCRIPTOR_SETS);
		descriptorSetLayouts[index] = handle;
		return *static_cast<Derived *>(this);
	}

	template <typename T> Derived &descriptorSetLayout(unsigned int index) {
		assert(index < MAX_DESCRIPTOR_SETS);
		descriptorSetLayouts[index] = T::layoutHandle;
		return *static_cast<Derived *>(this);
	}
};


class GraphicsPipelineDesc : public PipelineDescBase<GraphicsPipelineDesc> {
	std::string           vertexShaderName;
	std::string           fragmentShaderName;
	RenderPassHandle      renderPass_;
	uint32_t              vertexAttribMask  = 0;
	unsigned int          numSamples_       = 1;
	bool                  depthWrite_       = false;
	bool                  depthTest_        = false;
	bool                  cullFaces_        = false;
	bool                  scissorTest_      = false;
	bool                  blending_         = false;
	BlendFunc             sourceBlend_      = BlendFunc::One;
	BlendFunc             destinationBlend_ = BlendFunc::Zero;
	// TODO: blend equation
	// TODO: per-MRT blending

	struct VertexAttr {
		uint8_t    bufBinding = 0;
		uint8_t    count      = 0;
		VtxFormat  format     = VtxFormat::Float;
		uint8_t    offset     = 0;

		bool operator==(const VertexAttr &other) const;
		bool operator!=(const VertexAttr &other) const;

		size_t hashValue() const {
			size_t h = 0;
			hashCombine(h, bufBinding);
			hashCombine(h, count);
			hashCombine(h, format);
			hashCombine(h, offset);
			return h;
		}
	};

	struct VertexBuf {
		uint32_t stride = 0;

		bool operator==(const VertexBuf &other) const;
		bool operator!=(const VertexBuf &other) const;

		size_t hashValue() const {
			return std::hash<uint32_t>()(stride);
		}
	};

	std::array<VertexAttr,     MAX_VERTEX_ATTRIBS>   vertexAttribs;
	std::array<VertexBuf,      MAX_VERTEX_BUFFERS>   vertexBuffers;


public:

	GraphicsPipelineDesc &vertexShader(const std::string &name) {
		assert(!name.empty());
		vertexShaderName = name;
		return *this;
	}

	GraphicsPipelineDesc &fragmentShader(const std::string &name) {
		assert(!name.empty());
		fragmentShaderName = name;
		return *this;
	}

	GraphicsPipelineDesc &renderPass(RenderPassHandle h) {
		renderPass_ = h;
		return *this;
	}

	GraphicsPipelineDesc &vertexAttrib(uint32_t attrib, uint8_t bufBinding, uint8_t count, VtxFormat format, uint8_t offset) {
		assert(attrib < MAX_VERTEX_ATTRIBS);

		vertexAttribs[attrib].bufBinding = bufBinding;
		vertexAttribs[attrib].count      = count;
		vertexAttribs[attrib].format     = format;
		vertexAttribs[attrib].offset     = offset;


		vertexAttribMask |= (1 << attrib);
		return *this;
	}

	GraphicsPipelineDesc &vertexBufferStride(uint8_t buf, uint32_t stride) {
		assert(buf < MAX_VERTEX_BUFFERS);
		vertexBuffers[buf].stride = stride;

		return *this;
	}

	GraphicsPipelineDesc &blending(bool b) {
		blending_ = b;
		return *this;
	}

	GraphicsPipelineDesc &sourceBlend(BlendFunc b) {
		assert(blending_);
		sourceBlend_ = b;
		return *this;
	}

	GraphicsPipelineDesc &destinationBlend(BlendFunc b) {
		assert(blending_);
		destinationBlend_ = b;
		return *this;
	}

	GraphicsPipelineDesc &depthWrite(bool d) {
		depthWrite_ = d;
		return *this;
	}

	GraphicsPipelineDesc &depthTest(bool d) {
		depthTest_ = d;
		return *this;
	}

	GraphicsPipelineDesc &cullFaces(bool c) {
		cullFaces_ = c;
		return *this;
	}

	GraphicsPipelineDesc &scissorTest(bool s) {
		scissorTest_ = s;
		return *this;
	}

	GraphicsPipelineDesc &numSamples(unsigned int n) {
		assert(n != 0);
		assert(isPow2(n));
		numSamples_ = n;
		return *this;
	}

	GraphicsPipelineDesc() {
	}

	~GraphicsPipelineDesc() {}

	GraphicsPipelineDesc(const GraphicsPipelineDesc &desc)                = default;
	GraphicsPipelineDesc &operator=(const GraphicsPipelineDesc &desc)     = default;

	GraphicsPipelineDesc(GraphicsPipelineDesc &&desc) noexcept            = default;
	GraphicsPipelineDesc &operator=(GraphicsPipelineDesc &&desc) noexcept = default;

	bool operator==(const GraphicsPipelineDesc &other) const;

	size_t hashValue() const;

	friend class Renderer;
	friend struct RendererImpl;
};


struct RenderPassDesc : public DescBase<RenderPassDesc> {
	RenderPassDesc() {
		depthStencil_.passBegin = PassBegin::DontCare;
	}

	~RenderPassDesc() { }

	RenderPassDesc(const RenderPassDesc &)                = default;
	RenderPassDesc &operator=(const RenderPassDesc &)     = default;

	RenderPassDesc(RenderPassDesc &&) noexcept            = default;
	RenderPassDesc &operator=(RenderPassDesc &&) noexcept = default;

	RenderPassDesc &depthStencil(Format ds, PassBegin pb) {
		assert(isDepthFormat(ds));
		depthStencil_.format    = ds;
		depthStencil_.passBegin = pb;
		return *this;
	}

	RenderPassDesc &color(unsigned int index, Format c, PassBegin pb, Layout initial, Layout final, glm::vec4 clear = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)) {
		assert(index < MAX_COLOR_RENDERTARGETS);
		colorRTs_[index].format         = c;
		colorRTs_[index].passBegin      = pb;
		colorRTs_[index].initialLayout  = initial;
		colorRTs_[index].finalLayout    = final;
		if (pb == PassBegin::Clear) {
			colorRTs_[index].clearValue = clear;
		}
		return *this;
	}

	RenderPassDesc &clearDepth(float v) {
		depthStencil_.passBegin    = PassBegin::Clear;
		depthStencil_.clearValue.x = v;
		return *this;
	}

	RenderPassDesc &numSamples(unsigned int n) {
		numSamples_ = n;
		return *this;
	}

	bool operator==(const RenderPassDesc &other) const;

	size_t hashValue() const;

	std::string colorRTDebug(unsigned int i) const {
		assert(i < MAX_COLOR_RENDERTARGETS);
		const auto &rt = colorRTs_[i];

		return fmt::format("{}\t{}\t{}", magic_enum::enum_name(rt.passBegin), magic_enum::enum_name(rt.initialLayout), magic_enum::enum_name(rt.finalLayout));
	}


private:

	struct RTInfo {
		Format     format         = Format::Invalid;
		PassBegin  passBegin      = PassBegin::DontCare;
		Layout     initialLayout  = Layout::Undefined;
		Layout     finalLayout    = Layout::Undefined;
		glm::vec4  clearValue     = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		size_t hashValue() const;
	};


	const RTInfo &color(unsigned int index) const {
		assert(index < MAX_COLOR_RENDERTARGETS);
		return colorRTs_[index];
	}


	RTInfo                                       depthStencil_;
	std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorRTs_;
	unsigned int                                 numSamples_          = 1;


	friend class Renderer;
	friend struct RendererImpl;
};


struct RenderTargetDesc : public DescBase<RenderTargetDesc> {
	RenderTargetDesc() {
	}

	~RenderTargetDesc() { }

	RenderTargetDesc(const RenderTargetDesc &)                = default;
	RenderTargetDesc &operator=(const RenderTargetDesc &)     = default;

	RenderTargetDesc(RenderTargetDesc &&) noexcept            = default;
	RenderTargetDesc &operator=(RenderTargetDesc &&) noexcept = default;


	RenderTargetDesc &width(unsigned int w) {
		assert(w < MAX_TEXTURE_SIZE);
		width_ = w;
		return *this;
	}

	RenderTargetDesc &height(unsigned int h) {
		assert(h < MAX_TEXTURE_SIZE);
		height_ = h;
		return *this;
	}

	RenderTargetDesc &numSamples(unsigned int s) {
		assert(s > 0);
		numSamples_ = s;
		return *this;
	}

	RenderTargetDesc &format(Format f) {
		format_ = f;
		return *this;
	}

	RenderTargetDesc &additionalViewFormat(Format f) {
		additionalViewFormat_ = f;
		return *this;
	}

	RenderTargetDesc &usage(const TextureUsageSet &u) {
		assert(u.any());
		assert(!u.test(TextureUsage::RenderTarget));  // set implicitly

		if (numSamples_ == 1) {
			// resolve source only valid on multisampled
			assert(!u.test(TextureUsage::ResolveSource));
		} else {
			// resolve destination only valid on non-multisampled
			assert(!u.test(TextureUsage::ResolveDestination));
			// can't blit to or from multisampled, must resolve
			assert(!u.test(TextureUsage::BlitSource));
			assert(!u.test(TextureUsage::BlitDestination));
		}

		usage_ = u;
		usage_.set(TextureUsage::RenderTarget);
		return *this;
	}

	const TextureUsageSet &usage() const {
		return usage_;
	}


	unsigned int width()                const { return width_; }
	unsigned int height()               const { return height_; }
	unsigned int numSamples()           const { return numSamples_; }
	Format       format()               const { return format_; }
	Format       additionalViewFormat() const { return additionalViewFormat_; }

private:

	unsigned int     width_                = 0;
	unsigned int     height_               = 0;
	unsigned int     numSamples_           = 1;
	Format           format_               = Format::Invalid;
	Format           additionalViewFormat_ = Format::Invalid;
	TextureUsageSet  usage_                = { TextureUsage::RenderTarget };


	friend class Renderer;
	friend struct RendererImpl;
};


struct SamplerDesc : public DescBase<SamplerDesc> {
	SamplerDesc() {
	}

	SamplerDesc(const SamplerDesc &desc)                = default;
	SamplerDesc &operator=(const SamplerDesc &desc)     = default;

	SamplerDesc(SamplerDesc &&desc) noexcept            = default;
	SamplerDesc &operator=(SamplerDesc &&desc) noexcept = default;

	~SamplerDesc() {}

	SamplerDesc &minFilter(FilterMode m) {
		min = m;
		return *this;
	}

	SamplerDesc &magFilter(FilterMode m) {
		mag = m;
		return *this;
	}

private:

	FilterMode  min      = FilterMode::Nearest;
	FilterMode  mag      = FilterMode::Nearest;
	WrapMode    wrapMode = WrapMode::Clamp;


	friend class Renderer;
	friend struct RendererImpl;
};


struct SwapchainDesc {
	unsigned int  width      = 0;
	unsigned int  height     = 0;
	unsigned int  numFrames  = 3;
	VSync         vsync      = VSync::On;
	bool          fullscreen = false;


	SwapchainDesc() {
	}
};


struct TextureDesc : public DescBase<TextureDesc> {
	TextureDesc() {
	}

	~TextureDesc() { }

	TextureDesc(const TextureDesc &)                = default;
	TextureDesc &operator=(const TextureDesc &)     = default;

	TextureDesc(TextureDesc &&) noexcept            = default;
	TextureDesc &operator=(TextureDesc &&) noexcept = default;


	TextureDesc &width(unsigned int w) {
		assert(w < MAX_TEXTURE_SIZE);
		width_ = w;
		return *this;
	}

	TextureDesc &height(unsigned int h) {
		assert(h < MAX_TEXTURE_SIZE);
		height_ = h;
		return *this;
	}

	TextureDesc &format(Format f) {
		format_ = f;
		return *this;
	}

	TextureDesc &mipLevelData(unsigned int level, const void *data, unsigned int size) {
		assert(level < numMips_);
		mipData_[level].data = data;
		mipData_[level].size = size;
		return *this;
	}


	TextureDesc &usage(const TextureUsageSet &u) {
		assert(u.any());
		assert(!u.test(TextureUsage::Present));        // only valid on rendertargets
		assert(!u.test(TextureUsage::RenderTarget));   // rendertargets are not created with this
		assert(!u.test(TextureUsage::ResolveSource));  // only valid on multisampled RTs
		usage_ = u;
		return *this;
	}

private:

	struct MipLevel {
		const void   *data  = nullptr;
		unsigned int  size  = 0;
	};

	unsigned int                                 width_    = 0;
	unsigned int                                 height_   = 0;
	unsigned int                                 numMips_  = 1;
	Format                                       format_   = Format::Invalid;
	std::array<MipLevel, MAX_TEXTURE_MIPLEVELS>  mipData_;
	TextureUsageSet                              usage_;


	friend class Renderer;
	friend struct RendererImpl;
};


struct Version {
	unsigned int  major = 0;
	unsigned int  minor = 0;
	unsigned int  patch = 0;

	Version() {
	}

	Version(const Version &)                = default;
	Version &operator=(const Version &)     = default;

	Version(Version &&) noexcept            = default;
	Version &operator=(Version &&) noexcept = default;

	~Version() {}
};


struct RendererDesc {
	bool           debug                = false;
	bool           robustness           = false;
	bool           tracing              = false;
	bool           skipShaderCache      = false;
	bool           optimizeShaders      = true;
	bool           validateShaders      = false;
	bool           transferQueue        = true;
	bool           synchronizationDebug = false;
	unsigned int   ephemeralRingBufSize = 1 * 1048576;
	SwapchainDesc  swapchain;
	std::string    applicationName;
	Version        applicationVersion;
	std::string    engineName;
	Version        engineVersion;
	std::string    vulkanDeviceFilter;


	RendererDesc() {
	}
};


struct RendererFeatures {
	uint32_t  maxMSAASamples   = 1;
	bool      sRGBFramebuffer  = false;
	bool      SSBOSupported    = false;


	RendererFeatures() {
	}
};


struct RendererImpl;


class Renderer {
	RendererImpl *impl;

	explicit Renderer(RendererImpl *impl_)
	: impl(impl_)
	{
		assert(impl != nullptr);
	}


public:

	static Renderer createRenderer(const RendererDesc &desc);

	Renderer(const Renderer &)            = delete;
	Renderer &operator=(const Renderer &) = delete;

	Renderer &operator=(Renderer &&other)  noexcept
	{
		if (this == &other) {
			return *this;
		}

		assert(!impl);

		impl       = other.impl;
		other.impl = nullptr;

		return *this;
	}

	Renderer(Renderer &&other) noexcept
	: impl(other.impl)
	{
		other.impl = nullptr;
	}

	Renderer()
	: impl(nullptr)
	{
	}

	~Renderer();


	operator bool() const {
		return bool(impl);
	}


	bool isRenderTargetFormatSupported(Format format) const;
	unsigned int getCurrentRefreshRate() const;
	unsigned int getMaxRefreshRate() const;

	bool getSynchronizationDebugMode() const;
	void setSynchronizationDebugMode(bool mode);

	const RendererFeatures &getFeatures() const;
	Format getSwapchainFormat() const;

	// TODO: add buffer usage flags
	BufferHandle            createBuffer(BufferType type, uint32_t size, const void *contents);
	BufferHandle            createEphemeralBuffer(BufferType type, uint32_t size, const void *contents);
	FramebufferHandle       createFramebuffer(const FramebufferDesc &desc);
	GraphicsPipelineHandle  createGraphicsPipeline(const GraphicsPipelineDesc &desc);
	RenderPassHandle        createRenderPass(const RenderPassDesc &desc);
	RenderTargetHandle      createRenderTarget(const RenderTargetDesc &desc);
	SamplerHandle           createSampler(const SamplerDesc &desc);
	TextureHandle           createTexture(const TextureDesc &desc);
	// TODO: non-ephemeral descriptor set

	DSLayoutHandle createDescriptorSetLayout(const DescriptorLayout *layout);
	template <typename T> void registerDescriptorSetLayout() {
		T::layoutHandle = createDescriptorSetLayout(T::layout);
	}

	// gets the textures of a rendertarget to be used for sampling
	// might be ephemeral, don't store
	TextureHandle        getRenderTargetView(RenderTargetHandle handle, Format f);

	void deleteBuffer(BufferHandle &&handle);
	void deleteFramebuffer(FramebufferHandle &&handle);
	void deleteGraphicsPipeline(GraphicsPipelineHandle &&handle);
	void deleteRenderPass(RenderPassHandle &&handle);
	void deleteRenderTarget(RenderTargetHandle &&handle);
	void deleteSampler(SamplerHandle &&handle);
	void deleteTexture(TextureHandle &&handle);

	void setSwapchainDesc(const SwapchainDesc &desc);
	bool isSwapchainDirty() const;
	glm::uvec2 getDrawableSize() const;

	void waitForDeviceIdle();

	// rendering
	void beginFrame();
	void presentFrame();

	void beginRenderPass(RenderPassHandle rpHandle, FramebufferHandle fbHandle);
	void beginRenderPassSwapchain(RenderPassHandle rpHandle);
	void endRenderPass();

	void layoutTransition(RenderTargetHandle image, Layout src, Layout dest);

	void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height);

	void bindGraphicsPipeline(GraphicsPipelineHandle pipeline);
	void bindDescriptorSet(unsigned int index, const DSLayoutHandle layout, const void *data);
	template <typename T> void bindDescriptorSet(unsigned int index, const T &data) {
		bindDescriptorSet(index, T::layoutHandle, &data);
	}

	void bindIndexBuffer(BufferHandle buffer, IndexFormat indexFormat);
	void bindVertexBuffer(unsigned int binding, BufferHandle buffer);

	void blit(RenderTargetHandle source, RenderTargetHandle target);
	void resolveMSAA(RenderTargetHandle source, RenderTargetHandle target);
	void resolveMSAAToSwapchain(RenderTargetHandle source, Layout finalLayout);

	void draw(unsigned int firstVertex, unsigned int vertexCount);
	void drawIndexed(unsigned int vertexCount, unsigned int firstIndex);
	void drawIndexedInstanced(unsigned int vertexCount, unsigned int instanceCount);
	void drawIndexedVertexOffset(unsigned int vertexCount, unsigned int firstIndex, unsigned int vertexOffset);

	DebugGroupHandle beginDebugGroup(const std::string &name);
	void endDebugGroup(DebugGroupHandle &&g);
};


class ScopedDebugGroup {
	Renderer          &renderer;
	DebugGroupHandle  handle;


public:

	ScopedDebugGroup(Renderer &renderer_, const std::string &name)
	: renderer(renderer_)
	, handle(renderer.beginDebugGroup(name))
	{
	}

	~ScopedDebugGroup() {
		renderer.endDebugGroup(std::move(handle));
	}

	// not copyable
	ScopedDebugGroup(const ScopedDebugGroup &)            noexcept = delete;
	ScopedDebugGroup &operator=(const ScopedDebugGroup &) noexcept = delete;

	// not movable
	ScopedDebugGroup(ScopedDebugGroup &&)                 noexcept = delete;
	ScopedDebugGroup &operator=(ScopedDebugGroup &&)      noexcept = delete;
};


}  // namespace renderer


namespace std {


template <> struct hash<renderer::BlendFunc> {
	size_t operator()(const renderer::BlendFunc &f) const {
		return std::hash<size_t>()(magic_enum::enum_integer(f));
	}
};


template <typename T>
struct hash<renderer::Handle<T> > {
	size_t operator()(const renderer::Handle<T> &handle) const {
		return handle.hashValue();
	}
};


template <> struct hash<renderer::GraphicsPipelineDesc> {
	size_t operator()(const renderer::GraphicsPipelineDesc &desc) const {
		return desc.hashValue();
	}
};


template <> struct hash<renderer::RenderPassDesc> {
	size_t operator()(const renderer::RenderPassDesc &desc) const {
		return desc.hashValue();
	}
};


template <> struct hash<renderer::ShaderMacros> {
	size_t operator()(const renderer::ShaderMacros &macros) const {
		return macros.hashValue();
	}
};


}  // namespace std


#endif  // RENDERER_H
