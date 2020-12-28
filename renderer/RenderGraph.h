#ifndef RENDERGRAPH_H
#define RENDERGRAPH_H


#include "utils/Hash.h"


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
		: depthStencil_(Default<RT>::value)
		, numSamples_(1)
		, clearDepthAttachment(false)
		, depthClearValue(1.0f)
		{
			for (auto &rt : colorRTs_) {
				rt.id            = Default<RT>::value;
				rt.passBegin     = PassBegin::DontCare;
				rt.clearValue    = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			}
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
			RT             id;
			PassBegin      passBegin;
			glm::vec4      clearValue;
		};

		RT                                           depthStencil_;
		std::array<RTInfo, MAX_COLOR_RENDERTARGETS>  colorRTs_;
		HashSet<RT>                                  inputRendertargets;
		unsigned int                                 numSamples_;
		std::string                                  name_;
		bool                                         clearDepthAttachment;
		float                                        depthClearValue;
	};

	typedef  std::function<void(RP, PassResources &)>  RenderPassFunc;


private:

	enum class State : uint8_t {
		  Invalid
		, Building
		, Ready
		, Rendering
	};


	struct RenderPass {
		RenderPassHandle   handle;
		FramebufferHandle  fb;
		RenderPassFunc     func;
		PassDesc           desc;
		RenderPassDesc     rpDesc;
	};


	struct InternalRT {
		RenderTargetHandle  handle;
		RenderTargetDesc    desc;
	};


	struct ExternalRT {
		Format              format;
		Layout              initialLayout;
		Layout              finalLayout;

		// not owned by us
		// only valid during frame
		RenderTargetHandle  handle;

		// TODO: map of renderpasses which use this
	};


	typedef boost::variant<ExternalRT, InternalRT> Rendertarget;


	template <typename FE, typename FI>
	static void visitRendertarget(Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final : public boost::static_visitor<void> {
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

		return boost::apply_visitor(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename FE, typename FI>
	static void visitRendertarget(const Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final : public boost::static_visitor<void> {
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

		return boost::apply_visitor(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename T, typename FE, typename FI>
	static T visitRendertargetValue(Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final : public boost::static_visitor<T> {
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

		return boost::apply_visitor(FuncVisitor(std::move(fe), std::move(fi)), rt);
	}


	template <typename T, typename FE, typename FI>
	static T visitRendertargetValue(const Rendertarget &rt, FE &&fe, FI &&fi) {
		struct FuncVisitor final : public boost::static_visitor<T> {
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

		return boost::apply_visitor(FuncVisitor(std::move(fe), std::move(fi)), rt);
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


	void buildRenderPassFramebuffer(Renderer &renderer, RenderPass &rp) {
		const auto &desc = rp.desc;

		FramebufferDesc fbDesc;
		fbDesc.renderPass(rp.handle)
			  .name(desc.name_);

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

		// TODO: cache framebuffers
		auto fbHandle = renderer.createFramebuffer(fbDesc);
		assert(fbHandle);
		rp.fb = fbHandle;
	}


	struct Blit {
		RT             source;
		RT             dest;
		Layout         finalLayout;
	};

	struct ResolveMSAA {
		RT             source;
		RT             dest;
		Layout         finalLayout;
	};

	struct Pipeline {
		PipelineDesc    desc;
		PipelineHandle  handle;
	};

	typedef boost::variant<Blit, RP, ResolveMSAA> Operation;


	State                                            state;
	std::exception_ptr                               storedException;
	bool                                             hasExternalRTs;
	RP                                               currentRP;
	std::vector<Operation>                           operations;
	RT                                               finalTarget;

	HashMap<RT, Rendertarget>                        rendertargets;

	// TODO: use hash map
	std::vector<Pipeline>                            pipelines;

	HashMap<RP, RenderPass>                          renderPasses;
	HashSet<RP>                                      renderpassesWithExternalRTs;


	RenderGraph(const RenderGraph &)                = delete;
	RenderGraph(RenderGraph &&) noexcept            = delete;

	RenderGraph &operator=(const RenderGraph &)     = delete;
	RenderGraph &operator=(RenderGraph &&) noexcept = delete;


public:


	RenderGraph()
	: state(State::Invalid)
	, hasExternalRTs(false)
	, currentRP(Default<RP>::value)
	, finalTarget(Default<RT>::value)
	{
	}


	~RenderGraph() {
	}


	void reset(Renderer &renderer, std::function<void(void)> processEvents) {
		assert(state == State::Invalid || state == State::Ready);
		state = State::Building;

		renderPasses.clear();
		renderpassesWithExternalRTs.clear();
		hasExternalRTs = false;

		for (auto &p : pipelines) {
			renderer.deletePipeline(p.handle);
			p.handle = PipelineHandle();
		}
		pipelines.clear();

		for (auto &rt : rendertargets) {
			assert(rt.first != Default<RT>::value);

			visitRendertarget(rt.second
							  , nopExternal
							  , [&] (InternalRT &i) {
								  assert(i.handle);
								  renderer.deleteRenderTarget(i.handle);
								  i.handle = RenderTargetHandle();
							  }
							 );
		}
		rendertargets.clear();

		for (auto &p : renderPasses) {
			auto &rp = p.second;
			if (rp.handle) {
				renderer.deleteRenderPass(rp.handle);
				rp.handle = RenderPassHandle();
			}

			if (rp.fb) {
				renderer.deleteFramebuffer(rp.fb);
				rp.fb = FramebufferHandle();
			}
		}
		renderPasses.clear();

		operations.clear();

		while (!renderer.waitForDeviceIdle()) {
			processEvents();
		}
	}


	void renderTarget(RT rt, const RenderTargetDesc &desc) {
		assert(state == State::Building);
		assert(rt != Default<RT>::value);

		InternalRT temp1;
		temp1.desc   = desc;
		auto DEBUG_ASSERTED temp2 = rendertargets.emplace(rt, temp1);
		assert(temp2.second);
	}


	void externalRenderTarget(RT rt, Format format, Layout initialLayout, Layout finalLayout) {
		assert(state == State::Building);
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
		assert(state == State::Building);

		RenderPass temp1;
		temp1.desc   = desc;
		temp1.func   = f;

		auto temp2 DEBUG_ASSERTED = renderPasses.emplace(rp, temp1);
		assert(temp2.second);

		operations.push_back(rp);
	}


	void resolveMSAA(RT source, RT dest) {
		assert(state == State::Building);

		ResolveMSAA op;
		op.source = source;
		op.dest   = dest;
		operations.push_back(op);
	}


	void blit(RT source, RT dest) {
		assert(state == State::Building);

		Blit op;
		op.source = source;
		op.dest   = dest;
		operations.push_back(op);
	}


	void presentRenderTarget(RT rt) {
		assert(state == State::Building);
		assert(rt != Default<RT>::value);

		finalTarget = rt;
	}


	void build(Renderer &renderer) {
		assert(state == State::Building);
		state = State::Ready;

		assert(finalTarget != Default<RT>::value);

		LOG("RenderGraph::build start\n");

		// TODO: sort operations so they don't have to be added in order

		// create rendertargets
		// TODO: remove unused RTs first
		for (auto &p : rendertargets) {
			assert(p.first != Default<RT>::value);

			visitRendertarget(p.second
							  , nopExternal
							  , [&] (InternalRT &i) {
								  i.handle = renderer.createRenderTarget(i.desc);
							  }
							 );
		}

		// automatically decide layouts
		{
			HashMap<RT, Layout> currentLayouts;

			// initialize final render target to transfer src
			currentLayouts[finalTarget] = Layout::TransferSrc;

			// initialize external rendertargets final layouts
			for (const auto &rt : rendertargets) {
				visitRendertarget(rt.second
								  , [&rt, &currentLayouts] (const ExternalRT &e) {
									  currentLayouts[rt.first] = e.finalLayout;
								  }
								  , nopInternal
								 );
			}

			struct LayoutVisitor final : public boost::static_visitor<void> {
				HashMap<RT, Layout> &currentLayouts;
				RenderGraph &rg;


				LayoutVisitor(HashMap<RT, Layout> &currentLayouts_, RenderGraph &rg_)
				: currentLayouts(currentLayouts_)
				, rg(rg_)
				{
				}

				void operator()(Blit &b) const {
					b.finalLayout            = currentLayouts[b.dest];
					currentLayouts[b.source] = Layout::TransferSrc;
				}

				void operator()(RP &rpId) const {
					auto it = rg.renderPasses.find(rpId);
					assert(it != rg.renderPasses.end());

					auto &rp     = it->second;
					auto &rpDesc = rp.rpDesc;
					auto &desc   = rp.desc;

					rpDesc.name(desc.name_);
					rpDesc.numSamples(desc.numSamples_);

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
							// TODO: check this, might need a forward pass over operations
							Layout initial = Layout::Undefined;
							if (pb == PassBegin::Keep) {
								initial = Layout::ColorAttachment;
							}

							Layout final = Layout::ColorAttachment;
							auto layoutIt = currentLayouts.find(rtId);
							if (layoutIt == currentLayouts.end()) {
								// unused
								// TODO: remove it entirely
							} else {
								final = layoutIt->second;
							}
							assert(final != Layout::Undefined);
							assert(final != Layout::TransferDst);

							rpDesc.color(i, fmt, pb, initial, final, desc.colorRTs_[i].clearValue);
							currentLayouts[rtId] = initial;
						}
					}

					// mark input rt current layout as shader read
					for (RT inputRT : desc.inputRendertargets) {
						currentLayouts[inputRT] = Layout::ShaderRead;
					}
				}

				void operator()(ResolveMSAA &resolve) const {
					resolve.finalLayout            = currentLayouts[resolve.dest];
					currentLayouts[resolve.source] = Layout::TransferSrc;
				}
			};

			LayoutVisitor lv(currentLayouts, *this);
			for (auto it = operations.rbegin(); it != operations.rend(); it++) {
				boost::apply_visitor(lv, *it);
			}

		}

		// create low-level renderpass objects
		for (auto &p : renderPasses) {
			auto &temp = p.second;
			const auto &desc = temp.desc;

			assert(!temp.handle);
			// TODO: cache render passes
			auto rpHandle = renderer.createRenderPass(temp.rpDesc);
			assert(rpHandle);
			temp.handle = rpHandle;

			assert(!temp.fb);

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
			// TODO: check depthStencil too

			if (!hasExternal) {
				buildRenderPassFramebuffer(renderer, temp);
			} else {
				auto result DEBUG_ASSERTED = renderpassesWithExternalRTs.insert(p.first);
				assert(result.second);
			}
		}

		// write description to debug log
		{
			struct DebugVisitor final : public boost::static_visitor<void> {
				RenderGraph &rg;

	
				DebugVisitor(RenderGraph &rg_)
				: rg(rg_)
				{
				}


				void operator()(const Blit &b) const {
					LOG("Blit %s -> %s\t%s\n", to_string(b.source), to_string(b.dest), layoutName(b.finalLayout));
				}

				void operator()(const RP &rpId) const {
					LOG("RenderPass %s\n", to_string(rpId));
					auto it = rg.renderPasses.find(rpId);
					assert(it != rg.renderPasses.end());
					const auto &desc   = it->second.desc;
					const auto &rpDesc = it->second.rpDesc;

					if (desc.depthStencil_ != Default<RT>::value) {
						LOG(" depthStencil %s\n", to_string(desc.depthStencil_));
					}

					for (unsigned int i = 0; i < MAX_COLOR_RENDERTARGETS; i++) {
						if (desc.colorRTs_[i].id != Default<RT>::value) {
							const auto &rt = rpDesc.color(i);
							LOG(" color %u: %s\t%s\t%s\t%s\n", i, to_string(desc.colorRTs_[i].id), passBeginName(rt.passBegin), layoutName(rt.initialLayout), layoutName(rt.finalLayout));
						}
					}

					if (!desc.inputRendertargets.empty()) {
						LOG(" inputs:\n");
						std::vector<RT> inputs;
						inputs.reserve(desc.inputRendertargets.size());
						for (auto i : desc.inputRendertargets) {
							inputs.push_back(i);
						}

						std::sort(inputs.begin(), inputs.end());
						for (auto i : inputs) {
							LOG("  %s\n", to_string(i));
						}
					}
				}

				void operator()(const ResolveMSAA &r) const {
					LOG("ResolveMSAA %s -> %s\t%s\n", to_string(r.source), to_string(r.dest), layoutName(r.finalLayout));
				}
			};

			DebugVisitor d(*this);
			for (const auto &op : operations) {
				boost::apply_visitor(d, op);
			}
		}
		LOG("RenderGraph::build end\n");
		logFlush();
	}


	void bindExternalRT(RT rt, RenderTargetHandle handle) {
		assert(state == State::Ready);
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
		assert(state == State::Ready);
		state = State::Rendering;

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
			for (auto rpName : renderpassesWithExternalRTs) {
				auto it = renderPasses.find(rpName);
				assert(it != renderPasses.end());
				auto &rp = it->second;

				assert(!rp.fb);
				buildRenderPassFramebuffer(renderer, rp);
				assert(rp.fb);
			}
		}

		struct OpVisitor final : public boost::static_visitor<void> {
			Renderer    &r;
			RenderGraph &rg;


			OpVisitor(Renderer &r_, RenderGraph &rg_)
			: r(r_)
			, rg(rg_)
			{
			}


			void operator()(const Blit &b) const {
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

			void operator()(const RP &rp) const {
				assert(rg.currentRP == Default<RP>::value);
				rg.currentRP = rp;

				auto it = rg.renderPasses.find(rp);
				assert(it != rg.renderPasses.end());

				r.beginRenderPass(it->second.handle, it->second.fb);

				PassResources res;
				// TODO: build ahead of time, fill here?
				for (RT inputRT : it->second.desc.inputRendertargets) {
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
				it->second.func(rp, res);
				} catch (std::exception &e) {
					// TODO: log renderpass
					LOG("Exception \"%s\" during renderpass\n", e.what());
					if (rg.storedException) {
						LOG("Already have an exception, not stored\n");
					} else {
						rg.storedException = std::current_exception();
					}

				}
				r.endRenderPass();

				assert(rg.currentRP == rp);
				rg.currentRP = Default<RP>::value;
			}

			void operator()(const ResolveMSAA &resolve) const {
				auto srcIt = rg.rendertargets.find(resolve.source);
				assert(srcIt != rg.rendertargets.end());
				RenderTargetHandle sourceHandle = getHandle(srcIt->second);

				auto destIt = rg.rendertargets.find(resolve.dest);
				assert(destIt != rg.rendertargets.end());
				RenderTargetHandle targetHandle = getHandle(destIt->second);

				r.layoutTransition(targetHandle, Layout::Undefined, Layout::TransferDst);
				r.resolveMSAA(sourceHandle, targetHandle);
				r.layoutTransition(targetHandle, Layout::TransferDst, resolve.finalLayout);
			}
		};

		for (const auto &op : operations) {
			boost::apply_visitor(OpVisitor(renderer, *this), op);
		}

		{
			auto it = rendertargets.find(finalTarget);
			assert(it != rendertargets.end());
			renderer.presentFrame(getHandle(it->second));
		}

		assert(state == State::Rendering);
		state = State::Ready;

		assert(currentRP == Default<RP>::value);

		if (hasExternalRTs) {
			for (auto &p : rendertargets) {
				// clear the bindings
				visitRendertarget(p.second
								  , [&] (ExternalRT &e) { assert(e.handle); e.handle = RenderTargetHandle(); }
								  , nopInternal
								 );
			}

			// clear framebuffers
			for (auto rpName : renderpassesWithExternalRTs) {
				// clear framebuffers
				auto it = renderPasses.find(rpName);
				assert(it != renderPasses.end());
				auto &rp = it->second;

				assert(rp.fb);
				// TODO: cache them
				renderer.deleteFramebuffer(rp.fb);
				rp.fb = FramebufferHandle();
			}
		}
		assert(currentRP == Default<RP>::value);

		if (storedException) {
			LOG("re-throwing exception");
			std::rethrow_exception(storedException);
		}
	}


	PipelineHandle createPipeline(Renderer &renderer, RP rp, PipelineDesc &desc) {
		assert(state == State::Ready || state == State::Rendering);

		auto it = renderPasses.find(rp);
		assert(it != renderPasses.end());
		desc.renderPass(it->second.handle);

		// TODO: use hash map
		for (const auto &pipeline : pipelines) {
			if (pipeline.desc == desc) {
				return pipeline.handle;
			}
		}

		auto handle = renderer.createPipeline(desc);

		Pipeline pipeline;
		pipeline.desc   = desc;
		pipeline.handle = handle;
		pipelines.emplace_back(std::move(pipeline));

		return handle;
	}


};


}  // namespace renderer


#endif  // RENDERGRAPH_H
