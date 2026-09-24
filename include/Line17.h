#pragma once

// =====================================================================================================================
// Skyrim 1.7 build line only (RUNTIME_LINE == 17, CommonLibSSE-NG 7.2). Empty on line 1.
//
// On this line include/RE forwards every engine type CommonLibSSE-NG 7.2 also carries to 7.2's own header (see
// CMakeLists.txt), so every layout the game is read through is 7.2's. What 7.2 does NOT carry is the engine functions
// this plugin calls and the member names its code was written against (our include/RE copies, line 1). This header
// supplies exactly those, in two kinds:
//
//   * VIEW structs (LocalMapMenu, its RUNTIME_DATA, LocalMapCullingProcess, CullJobDescriptor): the same members under
//     the names the minimap code uses, laid over 7.2's type. Every member's offset and the struct's size is
//     static_assert-ed EQUAL to 7.2's declaration below, so the layout is 7.2's, checked by the compiler - it is never
//     retyped from memory. A view is only ever reached by casting a pointer to the real engine object; it is never
//     constructed or destroyed.
//   * ENGINE FUNCTIONS: each resolved by an Address Library id pair whose AE id is the one 1.7 uses. Every pair is in
//     tools/check-17.json and resolves on 1.7.104 (.MD/scripts/check-17.py). They are also the very functions the
//     game's own LocalMapCullingProcess::RenderOffScreen (16094 / 16335) calls on 1.7.104, with the same arguments
//     and the same member offsets (0x301F8 cull job, 0x302B8 camera, 0x302C8 accumulator, 0x302D0 image-space
//     parameters, 0x30358 fog-of-war overlay) - read from the decrypted 1.7.104 code with .MD/scripts/gamecode.py.
//
// Two places where 7.2 itself would be WRONG to use here, both proven from that same 1.7.104 code:
//   * BSGraphics::RenderTargetManager::GetSingleton() in 7.2 dereferences id 411451 as a pointer. The game passes the
//     id's ADDRESS as `this` (lea rcx, [id 411451]) - the manager is a static object, not a pointer to one. Line 1's
//     copy does the same as the game, so this file does too (RenderTargetManager()).
//   * RE::ControlMap::ToggleControls takes a third argument in 7.2 (see Controls.cpp).
// =====================================================================================================================
#if RUNTIME_LINE == 17

#	include <cstddef>

namespace DEM::line17
{
	using ClibLocalMapMenu = RE::LocalMapMenu;
	using ClibCullingProcess = RE::LocalMapMenu::LocalMapCullingProcess;
	using ClibRuntimeData = RE::LocalMapMenu::RUNTIME_DATA;

	// -----------------------------------------------------------------------------------------------------------------
	// The argument block LocalMapCullingProcess::AttachFogOfWarOverlay (16101 / 16344) reads and writes. 7.2 has no
	// type for it. Its field offsets were compared instruction by instruction between 1.5.97 and 1.7.104: both read
	// the word at +0x08 (grid divisions) and write the extents at +0x0C..+0x14 and +0x18..+0x20.
	// -----------------------------------------------------------------------------------------------------------------
	struct FogOfWar
	{
		RE::NiNode*   overlayHolder;  // 00
		std::uint16_t gridDivisions;  // 08
		RE::NiPoint3  minExtent;      // 0C
		RE::NiPoint3  maxExtent;      // 18
	};
	static_assert(sizeof(FogOfWar) == 0x28);
	static_assert(offsetof(FogOfWar, gridDivisions) == 0x08);
	static_assert(offsetof(FogOfWar, minExtent) == 0x0C);
	static_assert(offsetof(FogOfWar, maxExtent) == 0x18);

	// -----------------------------------------------------------------------------------------------------------------
	// RE::BSCullingJob (7.2) under line 1's CullJobDescriptor names, plus the engine's cull entry point.
	// -----------------------------------------------------------------------------------------------------------------
	struct CullJobDescriptor
	{
		void Cull(std::int32_t a_unk0, std::uint32_t a_unk1)
		{
			using func_t = void (*)(CullJobDescriptor*, std::int32_t, std::uint32_t);
			static REL::Relocation<func_t> func{ RELOCATION_ID(100213, 106921) };
			func(this, a_unk0, a_unk1);
		}

		// members
		RE::NiPointer<RE::BSGraphics::BSShaderAccumulator> shaderAccumulator;           // 00
		RE::NiPointer<RE::BSGraphics::BSShaderAccumulator> shaderAccumulatorSecondary;  // 08
		RE::NiPointer<RE::NiCamera>                         camera;                      // 10
		RE::BSCompoundFrustum*                              compoundFrustum;             // 18
		RE::NiFrustum*                                      frustum;                     // 20
		RE::BSPortalGraphEntry*                             portalGraphEntry;            // 28
		RE::BSCullingProcess*                               cullingProcess;              // 30
		RE::NiFrustumPlanes*                                customCullPlanes;            // 38
		RE::NiPointer<RE::NiAVObject>                       scene;                       // 40
		RE::BSTArray<RE::NiPointer<RE::NiAVObject>>*        cullingObjects;              // 48
		std::uint32_t                                       cullMode;                    // 50
		float                                               lightRadius;                 // 54
		std::uint32_t                                       unk58;                       // 58
		bool                                                isParabolic;                 // 5C
		bool                                                isDirectionalLight;          // 5D
		bool                                                ignorePreprocess;            // 5E
		bool                                                doCustomCullPlanes;          // 5F
		bool                                                cameraRelatedUpdates;        // 60
		bool                                                unk61;                       // 61
		bool                                                updateAccumulateFlag;        // 62
	};
	static_assert(sizeof(CullJobDescriptor) == sizeof(RE::BSCullingJob));
	static_assert(offsetof(CullJobDescriptor, shaderAccumulator) == offsetof(RE::BSCullingJob, accumulator));
	static_assert(offsetof(CullJobDescriptor, camera) == offsetof(RE::BSCullingJob, camera));
	static_assert(offsetof(CullJobDescriptor, portalGraphEntry) == offsetof(RE::BSCullingJob, portalGraphEntry));
	static_assert(offsetof(CullJobDescriptor, cullingProcess) == offsetof(RE::BSCullingJob, cullingProcess));
	static_assert(offsetof(CullJobDescriptor, scene) == offsetof(RE::BSCullingJob, scene));
	static_assert(offsetof(CullJobDescriptor, cullingObjects) == offsetof(RE::BSCullingJob, accumulatedObjectArray));
	static_assert(offsetof(CullJobDescriptor, cullMode) == offsetof(RE::BSCullingJob, cullMode));

	// -----------------------------------------------------------------------------------------------------------------
	// LocalMapMenu::LocalMapCullingProcess (7.2) under line 1's names, with the engine functions line 1's copy had.
	// -----------------------------------------------------------------------------------------------------------------
	struct LocalMapCullingProcess
	{
	public:
		// The argument block CullCellObjects (16100 / 16343) and CullTerrain take: 1.5.97 and 1.7.104 read it the same
		// way (the culling process through +0x00, the flag at +0x08).
		struct UnkData
		{
			LocalMapCullingProcess* ptr;   // 00
			bool                    unk8;  // 08
		};
		static_assert(sizeof(UnkData) == 0x10);

		[[nodiscard]] RE::LocalMapCamera* GetLocalMapCamera() noexcept { return &camera; }

		void RenderOffScreen()
		{
			using func_t = void (*)(LocalMapCullingProcess*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(16094, 16335) };
			func(this);
		}

		void CreateFogOfWar()
		{
			using func_t = void (*)(LocalMapCullingProcess*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(16095, 16336) };
			func(this);
		}

		static std::uint32_t CullCellObjects(UnkData& a_unkData, const RE::TESObjectCELL* a_cell)
		{
			using func_t = std::uint32_t (*)(UnkData&, const RE::TESObjectCELL*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(16100, 16343) };
			return func(a_unkData, a_cell);
		}

		static int AttachFogOfWarOverlay(FogOfWar& a_fogOfWar, const RE::TESObjectCELL* a_cell)
		{
			using func_t = int (*)(FogOfWar&, const RE::TESObjectCELL*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(16101, 16344) };
			return func(a_fogOfWar, a_cell);
		}

		[[nodiscard]] RE::NiPointer<RE::BSGraphics::BSShaderAccumulator>& GetShaderAccumulator() noexcept { return shaderAccumulator; }
		[[nodiscard]] RE::ImageSpaceShaderParam&                          GetImageSpaceShaderParam() noexcept { return imageSpaceShaderParam; }
		[[nodiscard]] RE::NiPointer<RE::NiNode>&                          GetFogOfWarOverlay() noexcept { return fogOfWarOverlay; }

		// members
		RE::BSCullingProcess                               cullingProcess;         // 00000
		CullJobDescriptor                                  cullJobDesc;            // 301F8
		RE::LocalMapCamera                                 camera;                 // 30260
		RE::NiPointer<RE::BSGraphics::BSShaderAccumulator> shaderAccumulator;      // 302C8
		RE::ImageSpaceShaderParam                          imageSpaceShaderParam;  // 302D0
		std::uint32_t                                      renderTarget;           // 30350
		std::uint32_t                                      renderMode;             // 30354
		RE::NiPointer<RE::NiNode>                          fogOfWarOverlay;        // 30358
	};
	static_assert(sizeof(LocalMapCullingProcess) == sizeof(ClibCullingProcess));
	static_assert(offsetof(LocalMapCullingProcess, cullingProcess) == offsetof(ClibCullingProcess, cullingProcess));
	static_assert(offsetof(LocalMapCullingProcess, cullJobDesc) == offsetof(ClibCullingProcess, cullingJob));
	static_assert(offsetof(LocalMapCullingProcess, camera) == offsetof(ClibCullingProcess, camera));
	static_assert(offsetof(LocalMapCullingProcess, shaderAccumulator) == offsetof(ClibCullingProcess, accumulator));
	static_assert(offsetof(LocalMapCullingProcess, imageSpaceShaderParam) == offsetof(ClibCullingProcess, imageSpaceShaderParam));
	static_assert(offsetof(LocalMapCullingProcess, renderTarget) == offsetof(ClibCullingProcess, renderTarget));
	static_assert(offsetof(LocalMapCullingProcess, renderMode) == offsetof(ClibCullingProcess, renderMode));
	static_assert(offsetof(LocalMapCullingProcess, fogOfWarOverlay) == offsetof(ClibCullingProcess, unk30358));
	// The offsets the game's own 1.7.104 RenderOffScreen uses for the same members.
	static_assert(offsetof(LocalMapCullingProcess, cullJobDesc) == 0x301F8);
	static_assert(offsetof(LocalMapCullingProcess, shaderAccumulator) == 0x302C8);
	static_assert(offsetof(LocalMapCullingProcess, imageSpaceShaderParam) == 0x302D0);
	static_assert(offsetof(LocalMapCullingProcess, fogOfWarOverlay) == 0x30358);

	// -----------------------------------------------------------------------------------------------------------------
	// LocalMapMenu::RUNTIME_DATA (7.2) under line 1's names. 7.2 calls +0x5C/+0x5D/+0x5E showingMap/dragging/
	// controlsReady; line 1 calls them enabled/usingCursor/inForeground. Same bytes: the game's 1.7.104
	// LocalMapMenu::InputHandler::CanProcess tests the menu at +0x303FC and +0x303FE exactly as 1.5.97 does, and the
	// 1.7.104 LocalMapMenu constructor (52964) initialises the same RUNTIME_DATA offsets (0x303A0..0x303FE) as 1.5.97's.
	// -----------------------------------------------------------------------------------------------------------------
	struct RUNTIME_DATA
	{
		RE::BSScaleformExternalTexture                            imageData;            // 00
		RE::GFxValue                                              root;                 // 18
		RE::GFxValue                                              iconDisplay;          // 30
		RE::GFxMovieView*                                         movieView;            // 48
		RE::BSTSmartPointer<ClibLocalMapMenu::InputHandler>       inputHandler;         // 50
		std::int32_t                                              selectedMarkerIndex;  // 58
		bool                                                      enabled;              // 5C
		bool                                                      usingCursor;          // 5D
		bool                                                      inForeground;         // 5E
		std::uint8_t                                              pad5F;                // 5F
	};
	static_assert(sizeof(RUNTIME_DATA) == sizeof(ClibRuntimeData));
	static_assert(offsetof(RUNTIME_DATA, imageData) == offsetof(ClibRuntimeData, unk303A0));
	static_assert(offsetof(RUNTIME_DATA, root) == offsetof(ClibRuntimeData, localMapMovie));
	static_assert(offsetof(RUNTIME_DATA, iconDisplay) == offsetof(ClibRuntimeData, mapMovie));
	static_assert(offsetof(RUNTIME_DATA, movieView) == offsetof(ClibRuntimeData, unk303E8));
	static_assert(offsetof(RUNTIME_DATA, inputHandler) == offsetof(ClibRuntimeData, unk303F0));
	static_assert(offsetof(RUNTIME_DATA, selectedMarkerIndex) == offsetof(ClibRuntimeData, selectedMarker));
	static_assert(offsetof(RUNTIME_DATA, enabled) == offsetof(ClibRuntimeData, showingMap));
	static_assert(offsetof(RUNTIME_DATA, usingCursor) == offsetof(ClibRuntimeData, dragging));
	static_assert(offsetof(RUNTIME_DATA, inForeground) == offsetof(ClibRuntimeData, controlsReady));

	// -----------------------------------------------------------------------------------------------------------------
	// LocalMapMenu (7.2) under line 1's names, with the three engine functions the minimap drives it through.
	// -----------------------------------------------------------------------------------------------------------------
	struct LocalMapMenu
	{
	public:
		using FogOfWar = line17::FogOfWar;
		using LocalMapCullingProcess = line17::LocalMapCullingProcess;
		using RUNTIME_DATA = line17::RUNTIME_DATA;

		LocalMapMenu* Ctor()
		{
			using func_t = LocalMapMenu* (*)(LocalMapMenu*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(52076, 52964) };
			return func(this);
		}

		void PopulateData()
		{
			using func_t = void (*)(LocalMapMenu*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(52081, 52971) };
			func(this);
		}

		void RefreshMarkers()
		{
			using func_t = void (*)(LocalMapMenu*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(52090, 52980) };
			func(this);
		}

		[[nodiscard]] RUNTIME_DATA& GetRuntimeData() noexcept { return runtimeData; }

		// members
		RE::BSTArray<RE::MapMenuMarker> mapMarkers;           // 00000
		RE::GFxValue                    markerData;           // 00018
		RE::GPointF                     topLeft;              // 00030
		RE::GPointF                     bottomRight;          // 00038
		LocalMapCullingProcess          localCullingProcess;  // 00040
		RUNTIME_DATA                    runtimeData;          // 303A0
	};
	static_assert(sizeof(LocalMapMenu) == sizeof(ClibLocalMapMenu));
	static_assert(offsetof(LocalMapMenu, mapMarkers) == offsetof(ClibLocalMapMenu, mapMarkers));
	static_assert(offsetof(LocalMapMenu, markerData) == offsetof(ClibLocalMapMenu, unk00018));
	static_assert(offsetof(LocalMapMenu, topLeft) == offsetof(ClibLocalMapMenu, unk00030));
	static_assert(offsetof(LocalMapMenu, bottomRight) == offsetof(ClibLocalMapMenu, unk00038));
	static_assert(offsetof(LocalMapMenu, localCullingProcess) == offsetof(ClibLocalMapMenu, localCullingProcess));
	static_assert(offsetof(LocalMapMenu, runtimeData) == offsetof(ClibLocalMapMenu, runtimeData));
	static_assert(offsetof(LocalMapMenu, runtimeData) == 0x303A0);

	// -----------------------------------------------------------------------------------------------------------------
	// Engine functions and globals line 1 reaches through its own include/RE copies, which 7.2 does not declare.
	// Each id pair is in tools/check-17.json; each is called by the game's own 1.7.104 RenderOffScreen the same way.
	// -----------------------------------------------------------------------------------------------------------------

	// The main shadow scene node: the first entry of the list at 513211 / 390951 (the 1.7.104 RenderOffScreen loads it
	// with `mov rsi, [id 390951]` and writes +0x219, 7.2's disableLightUpdate).
	inline RE::ShadowSceneNode* GetMainShadowSceneNode()
	{
		REL::Relocation<RE::ShadowSceneNode**> list{ RELOCATION_ID(513211, 390951) };
		return list.get()[0];
	}
#	if defined(EXCLUSIVE_SKYRIM_FLAT)
	static_assert(offsetof(RE::ShadowSceneNode, disableLightUpdate) == 0x219);
#	endif

	inline void SetClearColor(RE::BSGraphics::Renderer* a_renderer, float a_red, float a_green, float a_blue, float a_alpha)
	{
		using func_t = void (*)(RE::BSGraphics::Renderer*, float, float, float, float);
		static REL::Relocation<func_t> func{ RELOCATION_ID(75463, 77249) };
		func(a_renderer, a_red, a_green, a_blue, a_alpha);
	}

	// The render target manager is a STATIC object at 524970 / 411451 - the game passes that address itself as `this`
	// on 1.5.97 and on 1.7.104. (7.2's GetSingleton() reads a pointer out of it instead; not used here.)
	inline RE::BSGraphics::RenderTargetManager* RenderTargetManager()
	{
		REL::Relocation<RE::BSGraphics::RenderTargetManager*> instance{ RELOCATION_ID(524970, 411451) };
		return instance.get();
	}

	inline void SetupRenderTargetAt(RE::BSGraphics::RenderTargetManager* a_manager, std::uint32_t a_index, RE::RENDER_TARGET a_target,
		RE::BSGraphics::SetRenderTargetMode a_mode, bool a_unk3)
	{
		using func_t = void (*)(RE::BSGraphics::RenderTargetManager*, std::uint32_t, RE::RENDER_TARGET, RE::BSGraphics::SetRenderTargetMode, bool);
		static REL::Relocation<func_t> func{ RELOCATION_ID(75646, 77453) };
		func(a_manager, a_index, a_target, a_mode, a_unk3);
	}

	inline void SetupDepthStencilAt(RE::BSGraphics::RenderTargetManager* a_manager, std::int32_t a_index, RE::BSGraphics::SetRenderTargetMode a_mode,
		std::int32_t a_slice, bool a_unk4)
	{
		using func_t = void (*)(RE::BSGraphics::RenderTargetManager*, std::int32_t, RE::BSGraphics::SetRenderTargetMode, std::int32_t, bool);
		static REL::Relocation<func_t> func{ RELOCATION_ID(75647, 77454) };
		func(a_manager, a_index, a_mode, a_slice, a_unk4);
	}

	inline int GetDepthStencil(const RE::BSGraphics::RenderTargetManager* a_manager)
	{
		using func_t = int (*)(const RE::BSGraphics::RenderTargetManager*);
		static REL::Relocation<func_t> func{ RELOCATION_ID(75650, 77457) };
		return func(a_manager);
	}

	// The 1.7.104 RenderOffScreen passes 0x11 (swap) and 0x10 (map) as the render targets and reads the swap target's
	// width from manager+0x1DC - element 0x11 of a 0x1C-byte RenderTargetProperties array starting at +0.
	static_assert(static_cast<std::uint32_t>(RE::RENDER_TARGET::kLOCAL_MAP) == 0x10);
	static_assert(static_cast<std::uint32_t>(RE::RENDER_TARGET::kLOCAL_MAP_SWAP) == 0x11);
	static_assert(sizeof(RE::BSGraphics::RenderTargetProperties) == 0x1C);
	static_assert(offsetof(RE::BSGraphics::RenderTargetManager, renderTargetData) == 0);

	inline void SetupPixelConstantGroup(RE::ImageSpaceShaderParam* a_param, std::uint32_t a_index, float a_value1, float a_value2, float a_value3, float a_value4)
	{
		using func_t = void (*)(RE::ImageSpaceShaderParam*, std::uint32_t, float, float, float, float);
		static REL::Relocation<func_t> func{ RELOCATION_ID(100198, 106905) };
		func(a_param, a_index, a_value1, a_value2, a_value3, a_value4);
	}

	// Effect 98 (0x62) is ISLocalMap: the 1.7.104 RenderOffScreen passes `mov edx, 0x62` to this same function.
	inline void* CopyWithImageSpaceEffect(RE::ImageSpaceManager* a_manager, std::int32_t a_effectIndex, RE::RENDER_TARGET a_swap, RE::RENDER_TARGET a_target,
		RE::ImageSpaceShaderParam* a_param)
	{
		using func_t = void* (*)(RE::ImageSpaceManager*, std::int32_t, RE::RENDER_TARGET, RE::RENDER_TARGET, RE::ImageSpaceShaderParam*);
		static REL::Relocation<func_t> func{ RELOCATION_ID(99025, 105676) };
		return func(a_manager, a_effectIndex, a_swap, a_target, a_param);
	}
	static_assert(RE::ImageSpaceManager::ImageSpaceEffectEnum::ISLocalMap == 98);

	inline void ClearActiveRenderPasses(RE::BSGraphics::BSShaderAccumulator* a_accumulator, bool a_unk0)
	{
		using func_t = void (*)(RE::BSGraphics::BSShaderAccumulator*, bool);
		static REL::Relocation<func_t> func{ RELOCATION_ID(99964, 106610) };
		func(a_accumulator, a_unk0);
	}

	inline void SetRenderMode(std::uint32_t a_renderMode)
	{
		using func_t = void (*)(std::uint32_t);
		static REL::Relocation<func_t> func{ RELOCATION_ID(98987, 105641) };
		func(a_renderMode);
	}

	// Line 1's RendererShadowState setters, over 7.2's layout. The 1.7.104 RenderOffScreen inlines exactly this: it
	// writes alphaBlendWriteMode at +0xB0 and sets dirty bit 0x80 on a change, and writes depthStencilDepthMode at +0x88
	// and sets or clears dirty bit 0x4 depending on +0x8C.
	inline void SetAlphaBlendWriteMode(RE::BSGraphics::RendererShadowState* a_state, std::uint32_t a_mode)
	{
		auto& data = a_state->GetRuntimeData();
		if (data.alphaBlendWriteMode != a_mode) {
			data.alphaBlendWriteMode = a_mode;
			data.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND);
		}
	}

	inline void SetDepthStencilDepthMode(RE::BSGraphics::RendererShadowState* a_state, RE::BSGraphics::DepthStencilDepthMode a_mode)
	{
		auto& data = a_state->GetRuntimeData();
		if (data.depthStencilDepthMode != a_mode) {
			data.depthStencilDepthMode = a_mode;
			if (data.depthStencilDepthModePrevious != a_mode) {
				data.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
			} else {
				data.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE);
			}
		}
	}
	static_assert(offsetof(RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA, alphaBlendWriteMode) == 0xB0);
	static_assert(offsetof(RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA, depthStencilDepthMode) == 0x88);
	static_assert(offsetof(RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA, depthStencilDepthModePrevious) == 0x8C);
	static_assert(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND == 0x80);
	static_assert(RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE == 0x4);
}

#endif  // RUNTIME_LINE == 17
