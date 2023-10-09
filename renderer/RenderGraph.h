#ifndef RENDERGRAPH_H
#define RENDERGRAPH_H


#include "utils/Hash.h"

#include <algorithm>
#include <functional>
#include <variant>

#include "renderer/Renderer.h"


namespace renderer {


template <typename T>
struct Default {
};


/*
 Requirements for types RT and RP:
  - must be equality comparable (implement operators == and !=)
  - must be hashable (have specialization of std::hash)
  - must have a default value (specialization of Default)
 */

template <typename RT, typename RP>
class RenderGraph {

public:

	class PassResources {

		HashMap<std::pair<RT, Format>, TextureHandle>  rendertargets;

		// TODO: buffers

	public:

		TextureHandle get(RT rt, Format fmt = Format::Invalid) const {
			std::pair<RT, Format> p;
			p.first  = rt;
			p.second = fmt;

			auto it = rendertargets.find(p);
			// failing this means the rendertarget was not correctly declared as input
			assert(it != rendertargets.end());

			return it->second;
		}

		friend class RenderGraph;
	};


	struct PassDesc {
		PassDesc()
		{
		}

		~PassDesc() { }

		PassDesc(const PassDesc &)                = default;
		PassDesc(PassDesc &&) noexcept            = default;

		PassDesc &operator=(const PassDesc &)     = default;
		PassDesc &operator=(PassDesc &&) noexcept = default;

		PassDesc &depthStencil(RT ds, PassBegin) {
			depthStencil_ = ds;
			return *this;
		}

		PassDesc &color(unsigned int index, RT id, PassBegin pb, glm::vec4 clear = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)) {
			assert(index < MAX_COLOR_RENDERTARGETS);
			assert(id != Default<RT>::value);
			colorRTs_[index].id             = id;
			colorRTs_[index].passBegin      = pb;
			if (pb == PassBegin::Clear) {
				colorRTs_[index].clearValue = clear;
			}
			return *this;
		}

		PassDesc &clearDepth(float v) {
			clearDepthAttachment  = true;
			depthClearValue       = v;
			return *this;
		}

		PassDesc &name(const std::string &str) {
			name_ = str;
			return *this;
		}

		PassDesc &numSamples(unsigned int n) {
			numSamples_ = n;
			return *this;
		}

		PassDesc &inputRendertarget(RT id) {
			auto success DEBUG_ASSERTED = inputRendertargets.emplace(id);
			assert(success.second);
			return *this;
		}

		struct RTInfo {
			RT             id          = Default<RT>::value;
			PassBegin      passBegin   = PassBegin::DontCare;
			glm::vec4      clearValue  = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		};

		RT                                           depthStencil_         = Default<RT>::value;
		std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorRTs_;
		HashSet<RT>                                  inputRendertargets;
		unsigned int                                 numSamples_           = 1;
		std::string                                  name_;
		bool                                         clearDepthAttachment  = false;
		float                                        depthClearValue       = 1.0f;
	};

	using RenderPassFunc = std::function<void(RP, PassResources &)>;


private:

	enum class RGState : uint8_t {
		  Invalid
		, Building
		, Ready
		, Rendering
	};


	struct RenderPass {
		RP                           id;
		std::string                  name;
		RenderPassHandle             handle;
		FramebufferHandle            fb;
		std::vector<RenderPassFunc>  renderFunctions;
		PassDesc                     desc;
		RenderPassDesc               rpDesc;
	};


	struct InternalRT {
		RenderTargetHandle  handle;
		RenderTargetDesc    desc;
	};


	struct ExternalRT {
		Format              format         = Format::Invalid;
		Layout              initialLayout  = Layout::Undefined;
		Layout              finalLayout    = Layout::Undefined;

		// not owned by us
		// only valid during frame
		RenderTargetHandle  handle;

		// TODO: map of renderpasses which use this
	};


	using Rendertarget = std::variant<ExternalRT, InternalRT>;


	template <typename FE, typename FI>
	static void visitRendertarget(Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final {
			FE fe;
			FI fi;

			explicit FuncVisitor(FE &&fe_, FI &&fi_)
			: fe(std::move(fe_))
			, fi(std::move(fi_))
			{
			}

			void operator()(ExternalRT &e) const {
				fe(e);
			}

			void operator()(InternalRT &i) const {
				fi(i);
			}
		};

		return std::visit(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename FE, typename FI>
	static void visitRendertarget(const Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final {
			FE fe;
			FI fi;

			explicit FuncVisitor(FE &&fe_, FI &&fi_)
			: fe(std::move(fe_))
			, fi(std::move(fi_))
			{
			}

			void operator()(const ExternalRT &e) const {
				fe(e);
			}

			void operator()(const InternalRT &i) const {
				fi(i);
			}
		};

		return std::visit(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename T, typename FE, typename FI>
	static T visitRendertargetValue(Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final {
			FE fe;
			FI fi;

			explicit FuncVisitor(FE &&fe_, FI &&fi_)
			: fe(std::move(fe_))
			, fi(std::move(fi_))
			{
			}

			T operator()(ExternalRT &e) const {
				return fe(e);
			}

			T operator()(InternalRT &i) const {
				return fi(i);
			}
		};

		return std::visit(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename T, typename FE, typename FI>
	static T visitRendertargetValue(const Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final {
			FE fe;
			FI fi;

			explicit FuncVisitor(FE &&fe_, FI &&fi_)
			: fe(std::move(fe_))
			, fi(std::move(fi_))
			{
			}

			T operator()(const ExternalRT &e) const {
				return fe(e);
			}

			T operator()(const InternalRT &i) const {
				return fi(i);
			}
		};

		return std::visit(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	static void nopExternal(const ExternalRT & /* e */) { }
	static void nopInternal(const InternalRT & /* i */) { }


	static Format getFormat(const Rendertarget &rt) {
		return visitRendertargetValue<Format>(rt
		                       , [] (const ExternalRT &e) { return e.format;        }
		                       , [] (const InternalRT &i) { return i.desc.format(); }
		                        );
	}


	static Format getAdditionalViewFormat(const Rendertarget &rt) {
		return visitRendertargetValue<Format>(rt
		                       , [] (const ExternalRT & /* e */) { return Format::Invalid;               }
		                       , [] (const InternalRT &i)        { return i.desc.additionalViewFormat(); }
		                        );
	}


	static RenderTargetHandle getHandle(const Rendertarget &rt) {
		return visitRendertargetValue<RenderTargetHandle>(rt
		                       , [] (const ExternalRT &e) { return e.handle; }
		                       , [] (const InternalRT &i) { return i.handle; }
		                        );
	}


	static bool isExternal(const Rendertarget &rt) {
		return visitRendertargetValue<bool>(rt
		                       , [] (const ExternalRT & /* e */) { return true;  }
		                       , [] (const InternalRT & /* i */) { return false; }
		                        );
	}


	static bool canMergeRenderPasses(const RenderPass &first, const RenderPass &second) {
		LOG("Checking for merge of render passes \"{}\" and \"{}\"", first.name, second.name);

		PassDesc firstDesc  = first.desc;
		PassDesc secondDesc = second.desc;
		// that use the same render targets
		// and the second renderpass passbegin is Keep
		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			auto &firstRT  = firstDesc.colorRTs_[i];
			auto &secondRT = secondDesc.colorRTs_[i];

			if (firstRT.id != secondRT.id) {
				LOG(" color rendertargets {} don't match", i);
				return false;
			}

			if (secondRT.id != Default<RT>::value) {
				// we checked above that these match
				assert(firstRT.id != Default<RT>::value);
				if (secondRT.passBegin != PassBegin::Keep) {
					LOG(" color rendertarget {} passBegin is not keep", i);
					return false;
				}

				if (secondDesc.inputRendertargets.find(firstRT.id) != secondDesc.inputRendertargets.end()) {
					LOG(" color rendertarget {} is input of second pass", i);
					return false;
				}
			}
		}

		if (firstDesc.numSamples_ != secondDesc.numSamples_) {
			LOG(" numSamples don't match");
			return false;
		}

		if (secondDesc.clearDepthAttachment) {
			LOG(" second pass clears depth");
			return false;
		}

		if (firstDesc.depthStencil_ != secondDesc.depthStencil_
		    && secondDesc.depthStencil_ != Default<RT>::value) {
			LOG(" depthStencils don't match");
			return false;
		}

		LOG(" could merge passes");
		return true;
	}


	void buildRenderPassFramebuffer(Renderer &renderer, RenderPass &rp) {
		const auto &desc = rp.desc;

		FramebufferDesc fbDesc;
		fbDesc.renderPass(rp.handle)
			  .name(rp.name);

		if (desc.depthStencil_ != Default<RT>::value) {
			auto it = rendertargets.find(desc.depthStencil_);
			assert(it != rendertargets.end());
			fbDesc.depthStencil(getHandle(it->second));
		}

		for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
			const auto &rt = desc.colorRTs_[i];
			if (rt.id != Default<RT>::value) {
				auto it = rendertargets.find(rt.id);
				assert(it != rendertargets.end());
				fbDesc.color(i, getHandle(it->second));
			}
		}

		LOG_TODO("cache framebuffers")
		rp.fb = renderer.createFramebuffer(fbDesc);
		assert(rp.fb);
	}


	struct Blit {
		RT             source       = Default<RT>::value;
		RT             dest         = Default<RT>::value;
		Layout         finalLayout  = Layout::Undefined;
	};


	struct ResolveMSAA {
		RT             source       = Default<RT>::value;
		RT             dest         = Default<RT>::value;
		Layout         finalLayout  = Layout::Undefined;
	};


	using Operation = std::variant<Blit, RenderPass, ResolveMSAA>;


	struct DebugLogVisitor final {
		void operator()(const Blit &b) const {
			LOG("Blit {} {} {}", to_string(b.source), to_string(b.dest), b.finalLayout);
		}

		void operator()(const RP &rpId) const {
			LOG("Renderpass {}", to_string(rpId));
		}

		void operator()(const ResolveMSAA &resolve) const {
			LOG("ResolveMSAA {} {} {}", to_string(resolve.source), to_string(resolve.dest), resolve.finalLayout);
		}
	};


	struct LayoutVisitor final {
		HashMap<RT, Layout>  &nextLayouts;
		RenderGraph          &rg;


		LayoutVisitor(HashMap<RT, Layout> &nextLayouts_, RenderGraph &rg_)
		: nextLayouts(nextLayouts_)
		, rg(rg_)
		{
		}

		void operator()(Blit &b) const {
			b.finalLayout         = nextLayouts[b.dest];
			nextLayouts[b.source] = Layout::TransferSrc;
		}

		void operator()(RenderPass &rp) const {
			RenderPassDesc &rpDesc = rp.rpDesc;
			PassDesc       &desc   = rp.desc;

			rpDesc.name(rp.name)
				  .numSamples(desc.numSamples_);

			if (desc.depthStencil_ != Default<RT>::value) {
				auto rtIt = rg.rendertargets.find(desc.depthStencil_);
				assert(rtIt != rg.rendertargets.end());

				Format fmt = getFormat(rtIt->second);
				assert(fmt != Format::Invalid);

				rpDesc.depthStencil(fmt, PassBegin::DontCare);
				if (desc.clearDepthAttachment) {
					rpDesc.clearDepth(desc.depthClearValue);
				}
			}

			for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
				auto rtId = desc.colorRTs_[i].id;
				if (rtId != Default<RT>::value) {
					auto rtIt = rg.rendertargets.find(rtId);
					assert(rtIt != rg.rendertargets.end());

					// get format
					Format fmt = getFormat(rtIt->second);
					assert(fmt != Format::Invalid);

					auto pb = desc.colorRTs_[i].passBegin;
					LOG_TODO("check this, might need a forward pass over operations")
					Layout initial = Layout::Undefined;
					if (pb == PassBegin::Keep) {
						initial = Layout::RenderAttachment;
					}

					Layout final = Layout::RenderAttachment;
					auto layoutIt = nextLayouts.find(rtId);
					if (layoutIt == nextLayouts.end()) {
						// unused
						LOG_TODO("remove it entirely")
						LOG("Removed unused rendertarget \"{}\" in renderpass \"{}\"", to_string(rtId), to_string(rp.id));
						desc.colorRTs_[i].id        = Default<RT>::value;
						desc.colorRTs_[i].passBegin = PassBegin::DontCare;
					} else {
						final = layoutIt->second;
						assert(final != Layout::Undefined);
						assert(final != Layout::TransferDst);

						rpDesc.color(i, fmt, pb, initial, final, desc.colorRTs_[i].clearValue);
						nextLayouts[rtId] = initial;
					}
				}
			}

			// mark input rt current layout as shader read
			for (RT inputRT : desc.inputRendertargets) {
				nextLayouts[inputRT] = Layout::ShaderRead;
			}
		}

		void operator()(ResolveMSAA &resolve) const {
			resolve.finalLayout         = nextLayouts[resolve.dest];
			nextLayouts[resolve.source] = Layout::TransferSrc;
		}
	};


	RGState                                          state            = RGState::Invalid;
	std::exception_ptr                               storedException;
	bool                                             hasExternalRTs   = false;
	RP                                               currentRP        = Default<RP>::value;
	std::vector<Operation>                           operations;
	RT                                               finalTarget      = Default<RT>::value;

	HashMap<RenderPassDesc, RenderPassHandle>        renderPassCache;

	HashMap<RT, Rendertarget>                        rendertargets;

	HashMap<GraphicsPipelineDesc, GraphicsPipelineHandle>  graphicsPipelines;

	HashSet<RP>                                      renderpassesWithExternalRTs;


	RenderGraph(const RenderGraph &)                = delete;
	RenderGraph(RenderGraph &&) noexcept            = delete;

	RenderGraph &operator=(const RenderGraph &)     = delete;
	RenderGraph &operator=(RenderGraph &&) noexcept = delete;


public:


	RenderGraph()
	{
	}


	~RenderGraph() {
		assert(state != RGState::Ready);
		assert(state != RGState::Rendering);
		assert(!storedException);
		assert(!hasExternalRTs);
		assert(operations.empty());
		assert(rendertargets.empty());
		assert(graphicsPipelines.empty());
		assert(renderpassesWithExternalRTs.empty());
	}


	void clearCaches(Renderer &renderer) {
		for (auto &p : graphicsPipelines) {
			renderer.deleteGraphicsPipeline(std::move(p.second));
		}
		graphicsPipelines.clear();

		for (auto &rp : renderPassCache) {
			renderer.deleteRenderPass(std::move(rp.second));
		}
		renderPassCache.clear();
	}


	void reset(Renderer &renderer) {
		assert(state == RGState::Invalid || state == RGState::Ready);
		state = RGState::Building;

		renderpassesWithExternalRTs.clear();
		hasExternalRTs = false;

		for (auto &rt : rendertargets) {
			assert(rt.first != Default<RT>::value);
			if (rt.first == finalTarget) {
				// not created so don't try to delete either
				continue;
			}

			visitRendertarget(rt.second
							  , nopExternal
							  , [&] (InternalRT &i) {
								  assert(i.handle);
								  renderer.deleteRenderTarget(std::move(i.handle));
							  }
							 );
		}
		rendertargets.clear();

		for (auto &op : operations) {
			auto *rp_ = std::get_if<RenderPass>(&op);
			if (!rp_) {
				continue;
			}

			RenderPass &rp = *rp_;
			rp.handle.reset();

			if (rp.fb) {
				renderer.deleteFramebuffer(std::move(rp.fb));
			}
		}
		operations.clear();

		renderer.waitForDeviceIdle();
	}


	void renderTarget(RT rt, const RenderTargetDesc &desc) {
		assert(state == RGState::Building);
		assert(rt != Default<RT>::value);
		assert(desc.usage().test(TextureUsage::RenderTarget));

		InternalRT temp1;
		temp1.desc   = desc;
		auto DEBUG_ASSERTED temp2 = rendertargets.emplace(rt, temp1);
		assert(temp2.second);
	}


	void externalRenderTarget(RT rt, Format format, Layout initialLayout, Layout finalLayout) {
		assert(state == RGState::Building);
		assert(rt != Default<RT>::value);
		assert(rendertargets.find(rt) == rendertargets.end());

		hasExternalRTs = true;

		ExternalRT e;
		e.format = format;
		e.initialLayout = initialLayout;
		e.finalLayout   = finalLayout;
		// leave handle undefined, it's set later by bindExternalRT
		auto temp DEBUG_ASSERTED = rendertargets.emplace(rt, e);
		assert(temp.second);
	}


	void renderPass(RP rp, const PassDesc &desc, RenderPassFunc f) {
		assert(state == RGState::Building);

		// check usage of input rendertargets
		for (RT rt : desc.inputRendertargets) {
			auto it = rendertargets.find(rt);
			assert(it != rendertargets.end());
			if (std::holds_alternative<InternalRT>(it->second)) {
				assert(std::get<InternalRT>(it->second).desc.usage().test(TextureUsage::Sampling));
			}
		}

		RenderPass rpData;
		rpData.id   = rp;
		rpData.name = desc.name_;
		rpData.desc = desc;
		rpData.renderFunctions.emplace_back(std::move(f));

		operations.emplace_back(std::move(rpData));
	}


	void resolveMSAA(RP /* rp */, RT source, RT dest) {
		assert(state == RGState::Building);

		auto DEBUG_ASSERTED srcIt = rendertargets.find(source);
		assert(srcIt != rendertargets.end());
		if (std::holds_alternative<InternalRT>(srcIt->second)) {
			assert(std::get<InternalRT>(srcIt->second).desc.usage().test(TextureUsage::ResolveSource));
		}

		auto DEBUG_ASSERTED destIt = rendertargets.find(dest);
		assert(destIt != rendertargets.end());
		if (std::holds_alternative<InternalRT>(destIt->second)) {
			assert(std::get<InternalRT>(destIt->second).desc.usage().test(TextureUsage::ResolveDestination));
		}

		assert(getFormat(srcIt->second) == getFormat(destIt->second));

		ResolveMSAA op;
		op.source = source;
		op.dest   = dest;
		operations.push_back(op);
	}


	void presentRenderTarget(RT rt) {
		assert(state == RGState::Building);
		assert(rt != Default<RT>::value);

		auto it = rendertargets.find(rt);
		assert(it != rendertargets.end());
		if (std::holds_alternative<InternalRT>(it->second)) {
			assert(std::get<InternalRT>(it->second).desc.usage().test(TextureUsage::Present));
		}

		finalTarget = rt;
	}


	void build(Renderer &renderer) {
		assert(state == RGState::Building);
		state = RGState::Ready;

		assert(finalTarget != Default<RT>::value);

		LOG("RenderGraph::build start");

		LOG_TODO("sort operations so they don't have to be added in order")

		// need to iterate these since removing rendertargets can lead to merging passes
		// and merging passes leads to having to recalculate layouts
		LOG_TODO("find a single-pass algorithm for this")
		bool keepGoing = false;

		do {
			keepGoing = false;
			// automatically decide layouts
			HashMap<RT, Layout> nextLayouts;

			// initialize final render target to transfer src
			nextLayouts[finalTarget] = Layout::Present;

			// initialize external rendertargets final layouts
			for (const auto &rt : rendertargets) {
				visitRendertarget(rt.second
								  , [&rt, &nextLayouts] (const ExternalRT &e) {
									  nextLayouts[rt.first] = e.finalLayout;
								  }
								  , nopInternal
								 );
			}

			LayoutVisitor lv(nextLayouts, *this);
			for (auto it = operations.rbegin(); it != operations.rend(); it++) {
				std::visit(lv, *it);
			}

			// merge operations if possible
			auto curr = operations.begin();
			auto next = curr + 1;
			while (next != operations.end()) {
				// the silly &* is because the thing is an iterator and get_if requires a pointer
				auto *currRP_ = std::get_if<RenderPass>(&*curr);
				auto *nextRP_ = std::get_if<RenderPass>(&*next);

				// if we have two renderpasses check if we can merge them
				if (currRP_ && nextRP_) {
					if (canMergeRenderPasses(*currRP_, *nextRP_)) {
						// add name of next to current
						currRP_->name += " / " + nextRP_->name;

						// add next function(s) to curr
						assert(!currRP_->renderFunctions.empty());
						assert(!nextRP_->renderFunctions.empty());
						for (auto &f : nextRP_->renderFunctions) {
							currRP_->renderFunctions.emplace_back(std::move(f));
						}
						nextRP_->renderFunctions.clear();

						// remove the second render pass
						// std::vector erase says documentation:
						//  "Invalidates iterators and references at or after the point of the erase, including the end() iterator."
						// so we trust that this doesn't invalidate the curr iterator
						next = operations.erase(next);

						// do the outer loop again to recalculate layouts
						keepGoing = true;

						// don't advance the iterators so we check the newly merged pass against next
						continue;
					}
				}
				LOG_TODO("if second operation is resolve, check if that can be merged as well")

				curr = next;
				next++;
			}
		} while (keepGoing);

		// write description to debug log before creating resources in case we trigger a validation error during that
		{
			struct DebugVisitor final {
				RenderGraph &rg;


				DebugVisitor(RenderGraph &rg_)
				: rg(rg_)
				{
				}


				void operator()(const Blit &b) const {
					LOG("Blit {} -> {}\t{}", to_string(b.source), to_string(b.dest), magic_enum::enum_name(b.finalLayout));
				}

				void operator()(const RenderPass &rpData) const {
					LOG("RenderPass {} \"{}\"", to_string(rpData.id), rpData.name);
					const auto &desc   = rpData.desc;
					const auto &rpDesc = rpData.rpDesc;

					if (desc.depthStencil_ != Default<RT>::value) {
						LOG(" depthStencil {}", to_string(desc.depthStencil_));
					}

					for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
						if (desc.colorRTs_[i].id != Default<RT>::value) {
							LOG(" color {}: {}\t{}", i, to_string(desc.colorRTs_[i].id), rpDesc.colorRTDebug(i));
						}
					}

					if (!desc.inputRendertargets.empty()) {
						LOG(" inputs:");
						std::vector<RT> inputs;
						inputs.reserve(desc.inputRendertargets.size());
						for (auto i : desc.inputRendertargets) {
							inputs.push_back(i);
						}

						std::sort(inputs.begin(), inputs.end());
						for (auto i : inputs) {
							LOG("  {}", to_string(i));
						}
					}
				}

				void operator()(const ResolveMSAA &r) const {
					LOG("ResolveMSAA {} -> {}\t{}", to_string(r.source), to_string(r.dest), magic_enum::enum_name(r.finalLayout));
				}
			};

			DebugVisitor d(*this);
			for (const auto &op : operations) {
				std::visit(d, op);
			}

			logFlush();
		}

		// create rendertargets
		for (auto &p : rendertargets) {
			assert(p.first != Default<RT>::value);
			if (p.first == finalTarget) {
				// directly to swapchain, don't create
				continue;
			}

			visitRendertarget(p.second
							  , nopExternal
							  , [&] (InternalRT &i) {
								  i.handle = renderer.createRenderTarget(i.desc);
							  }
							 );
		}

		// create low-level renderpass objects and framebuffers
		for (auto &op : operations) {
			auto *rp_ = std::get_if<RenderPass>(&op);
			if (!rp_) {
				continue;
			}

			RenderPass &rp = *rp_;
			rp.rpDesc.name(rp.name);
			const auto &desc = rp.desc;

			// if this is the final renderpass (which renders to swapchain)
			// we don't create its framebuffer here
			// Renderer will do that internally
			// we do have to make sure the format matches swapchain
			bool isFinal     = false;
			if (desc.colorRTs_[0].id == finalTarget) {
				isFinal = true;
			}

			assert(!rp.handle);
			{
				auto it = renderPassCache.find(rp.rpDesc);
				if (it != renderPassCache.end()) {
					rp.handle = it->second;
				} else {
					auto handle = renderer.createRenderPass(rp.rpDesc);
					// store owning handle in cache and return a non-owning copy
					rp.handle = handle;
					renderPassCache.emplace(rp.rpDesc, std::move(handle));
				}
			}
			assert(rp.handle);

			assert(!rp.fb);

			// if this renderpass has external RTs we defer its creation
			bool hasExternal = false;
			for (const auto &rt : desc.colorRTs_) {
				if (rt.id != Default<RT>::value) {
					auto it = rendertargets.find(rt.id);
					assert(it != rendertargets.end());
					if (isExternal(it->second)) {
						hasExternal = true;
						break;
					}
				}
			}
			LOG_TODO("check depthStencil too")

			if (!hasExternal) {
				if (!isFinal) {
					buildRenderPassFramebuffer(renderer, rp);
				}
			} else {
				auto result DEBUG_ASSERTED = renderpassesWithExternalRTs.insert(rp.id);
				assert(result.second);
			}
		}

		LOG("RenderGraph::build end");
		logFlush();
	}


	void bindExternalRT(RT rt, RenderTargetHandle handle) {
		assert(state == RGState::Ready);
		assert(handle);

		auto it = rendertargets.find(rt);
		assert(it != rendertargets.end());
		visitRendertarget(it->second
						  , [&] (ExternalRT &e) {
							  assert(!e.handle);
							  e.handle = handle;
						  }
						  , nopInternal
						 );
	}


	void render(Renderer &renderer) {
		assert(state == RGState::Ready);
		state = RGState::Rendering;

		if (hasExternalRTs) {
			bool hasExternal = false;
			for (const auto &p : rendertargets) {
				// if we have external RTs they must be bound by now
				visitRendertarget(p.second
								  , [&] (const ExternalRT &e DEBUG_ASSERTED) { assert(e.handle); hasExternal = true; }
								  , nopInternal
								 );
			}
			assert(hasExternal);

			// build framebuffers
			for (auto &op : operations) {
				auto *rp_ = std::get_if<RenderPass>(&op);
				if (!rp_) {
					continue;
				}

				RenderPass &rp = *rp_;
				if (renderpassesWithExternalRTs.find(rp.id) == renderpassesWithExternalRTs.end()) {
					continue;
				}

				assert(!rp.fb);
				buildRenderPassFramebuffer(renderer, rp);
				assert(rp.fb);
			}
		}

		renderer.beginFrame();

		struct OpVisitor final {
			Renderer    &r;
			RenderGraph &rg;


			OpVisitor(Renderer &r_, RenderGraph &rg_)
			: r(r_)
			, rg(rg_)
			{
			}


			void operator()(const Blit &b) const {
				assert(b.source != rg.finalTarget);
				assert(b.dest   != rg.finalTarget);

				auto srcIt = rg.rendertargets.find(b.source);
				assert(srcIt != rg.rendertargets.end());
				RenderTargetHandle sourceHandle = getHandle(srcIt->second);

				auto destIt = rg.rendertargets.find(b.dest);
				assert(destIt != rg.rendertargets.end());
				RenderTargetHandle targetHandle = getHandle(destIt->second);

				r.layoutTransition(targetHandle, Layout::Undefined, Layout::TransferDst);
				r.blit(sourceHandle, targetHandle);
				r.layoutTransition(targetHandle, Layout::TransferDst, b.finalLayout);
			}

			void operator()(const RenderPass &rp) const {
				assert(rg.currentRP == Default<RP>::value);
				rg.currentRP = rp.id;
				assert(rp.handle);

				renderer::ScopedDebugGroup g(r, rp.name);

				if (rp.fb) {
					r.beginRenderPass(rp.handle, rp.fb);
				} else {
					// must be final pass
					LOG_TODO("check that")

					r.beginRenderPassSwapchain(rp.handle);
				}

				PassResources res;
				LOG_TODO("build ahead of time, fill here?")
				for (RT inputRT : rp.desc.inputRendertargets) {
					// get rendertarget desc
					auto rtIt = rg.rendertargets.find(inputRT);
					assert(rtIt != rg.rendertargets.end());

					// get format
					Format fmt = getFormat(rtIt->second);
					assert(fmt != Format::Invalid);

					{
						// get view from renderer, add to res
						TextureHandle view = r.getRenderTargetView(getHandle(rtIt->second), fmt);
						res.rendertargets.emplace(std::make_pair(inputRT, fmt), view);
						// also add it with Format::Invalid so default works easier
						res.rendertargets.emplace(std::make_pair(inputRT, Format::Invalid), view);
					}

					// do the same for additional view format if there is one
					Format additionalFmt = getAdditionalViewFormat(rtIt->second);
					if (additionalFmt != Format::Invalid) {
						assert(additionalFmt != fmt);
						TextureHandle view = r.getRenderTargetView(getHandle(rtIt->second), additionalFmt);
						res.rendertargets.emplace(std::make_pair(inputRT, additionalFmt), view);
					}
				}

				try {
					assert(!rp.renderFunctions.empty());
					for (auto &f : rp.renderFunctions) {
						f(rp.id, res);
					}
				} catch (std::exception &e) {
					LOG_TODO("log renderpass")
					LOG("Exception \"{}\" during renderpass", e.what());
					if (rg.storedException) {
						LOG("Already have an exception, not stored");
					} else {
						rg.storedException = std::current_exception();
					}

				}
				r.endRenderPass();

				assert(rg.currentRP == rp.id);
				rg.currentRP = Default<RP>::value;
			}

			void operator()(const ResolveMSAA &resolve) const {
				assert(resolve.source != rg.finalTarget);

				auto srcIt = rg.rendertargets.find(resolve.source);
				assert(srcIt != rg.rendertargets.end());
				RenderTargetHandle sourceHandle = getHandle(srcIt->second);

				if (resolve.dest != rg.finalTarget) {
					auto destIt = rg.rendertargets.find(resolve.dest);
					assert(destIt != rg.rendertargets.end());
					RenderTargetHandle targetHandle = getHandle(destIt->second);

					r.layoutTransition(targetHandle, Layout::Undefined, Layout::TransferDst);
					r.resolveMSAA(sourceHandle, targetHandle);
					r.layoutTransition(targetHandle, Layout::TransferDst, resolve.finalLayout);
				} else {
					r.resolveMSAAToSwapchain(sourceHandle, resolve.finalLayout);
				}
			}
		};

		for (const auto &op : operations) {
			std::visit(OpVisitor(renderer, *this), op);
		}

		{
			auto it DEBUG_ASSERTED = rendertargets.find(finalTarget);
			assert(it != rendertargets.end());
			renderer.presentFrame();
		}

		assert(state == RGState::Rendering);
		state = RGState::Ready;

		assert(currentRP == Default<RP>::value);

		if (hasExternalRTs) {
			for (auto &p : rendertargets) {
				// clear the bindings
				visitRendertarget(p.second
								  , [&] (ExternalRT &e) { assert(e.handle); e.handle.reset(); }
								  , nopInternal
								 );
			}

			// clear framebuffers
			for (auto &op : operations) {
				auto *rp_ = std::get_if<RenderPass>(&op);
				if (!rp_) {
					continue;
				}

				RenderPass &rp = *rp_;
				if (renderpassesWithExternalRTs.find(rp.id) == renderpassesWithExternalRTs.end()) {
					continue;
				}

				assert(rp.fb);
				LOG_TODO("cache them")
				renderer.deleteFramebuffer(std::move(rp.fb));
			}
		}
		assert(currentRP == Default<RP>::value);

		if (storedException) {
			LOG("re-throwing exception");
			std::rethrow_exception(storedException);
		}
	}


	GraphicsPipelineHandle createGraphicsPipeline(Renderer &renderer, RP rp, GraphicsPipelineDesc &desc) {
		assert(state == RGState::Ready || state == RGState::Rendering);

		LOG_TODO("use hash map")
		bool DEBUG_ASSERTED found = false;
		for (auto &op : operations) {
			auto *rp_ = std::get_if<RenderPass>(&op);
			if (!rp_) {
				continue;
			}

			RenderPass &rpData = *rp_;
			if (rpData.id == rp) {
				desc.renderPass(rpData.handle);
				found = true;
				break;
			}
		}
		assert(found);

		{
			LOG_TODO("this is too strict, renderpasses only need to be compatible instead of identical")
			auto it = graphicsPipelines.find(desc);
			if (it != graphicsPipelines.end()) {
				return it->second;
			}
		}

		// store the owning handle in graphicsPipelines and return a non-owning copy
		auto handle = renderer.createGraphicsPipeline(desc);
		GraphicsPipelineHandle result = handle;

		graphicsPipelines.emplace(desc, std::move(handle));

		return result;
	}


};


}  // namespace renderer


#endif  // RENDERGRAPH_H
