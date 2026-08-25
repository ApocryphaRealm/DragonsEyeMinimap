#include "Minimap.h"

#include <numbers>

namespace DEM
{
	bool Minimap::ProcessMessage(RE::UIMessage* a_message)
	{
		if (!localMap)
		{
			InitLocalMap();
		}

		if (localMap && !inputHandler->registered)
		{
			RE::MenuControls::GetSingleton()->AddHandler(inputHandler.get());

			logger::debug("Minimap input handler registered with MenuControls");
		}

		return false;
	}

	void Minimap::RegisterHUDComponent(RE::FxDelegateArgs& a_params)
	{
		RE::HUDObject::RegisterHUDComponent(a_params);
		displayObj.Invoke("AddToHudElements");

		logger::debug("Minimap HUD component registered and added to HudElements");
	}

	void Minimap::InitLocalMap()
	{
		localMap = static_cast<RE::LocalMapMenu*>(std::malloc(sizeof(RE::LocalMapMenu)));
		if (localMap)
		{
			logger::debug("InitLocalMap: allocated LocalMapMenu instance");

			localMap->Ctor();

			// Cache references ///////////////////////////////////////////////////////////////////////
			localMap_ = &localMap->GetRuntimeData();
			cullingProcess = &localMap->localCullingProcess;
			cameraContext = cullingProcess->GetLocalMapCamera();

			// Remove vanilla local map input controller
			RE::MenuControls::GetSingleton()->RemoveHandler(localMap_->inputHandler.get());

			// Set init state /////////////////////////////////////////////////////////////////////////
			localMap_->usingCursor = 0;

			cameraContext->currentState->Begin();

			RE::NiPoint3 playerPos = RE::PlayerCharacter::GetSingleton()->GetPosition();
			cameraContext->SetDefaultStateInitialPosition(playerPos);

			// Init scaleform /////////////////////////////////////////////////////////////////////////

			// Set to reuse game logic
			localMap_->movieView = view.get();

			view->GetVariable(&localMap_->root, (std::string(DEM::Minimap::path) + ".MapClip").c_str());

			localMap_->root.Invoke("InitMap");
			localMap_->root.Invoke("SetShape", std::array<RE::GFxValue, 1>{ static_cast<std::uint32_t>(shape) });

			if (!localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay) || !localMap_->iconDisplay.IsObject())
			{
				logger::debug("InitLocalMap: IconDisplay not reachable yet; Advance will keep retrying (see EnsureIconDisplay)");
			}
			else if (!localMap_->iconDisplay.GetMember("MarkerData", &localMap->markerData))
			{
				logger::debug("InitLocalMap: IconDisplay.MarkerData not reachable yet; Advance will keep retrying (see EnsureIconDisplay)");
			}

			// The Controls clip - the "hold to control/tap to hide" prompt and its buttons -
			// starts visible by default: frame 1 of its own timeline is the label the AS2 side
			// calls "show", and a MovieClip always begins on frame 1 with no script needed to
			// put it there. Upstream relied on FoldControls()/ShowControls() running before the
			// player ever saw it and HideControlsAfter() to fade it out again; neither runs any
			// more, so without this it would sit on screen, showing whatever text was authored
			// into it, for as long as the minimap exists. Setting _visible directly does not
			// depend on the "show"/"fadeOut" frame labels or their animation, unlike gotoAndPlay.
			RE::GFxValue controlsClip;
			if (localMap_->root.GetMember("Controls", &controlsClip) && controlsClip.IsDisplayObject())
			{
				controlsClip.SetMember("_visible", RE::GFxValue{ false });
			}

			// ApplyDisplaySettings() ran once already, from the constructor, before localMap_
			// existed - it bailed out of the title-positioning part of the work back then, so
			// do it again now that LocationName/ClearedHint/LocalMapHolder can actually be
			// reached. Everything else it does is safe to repeat.
			ApplyDisplaySettings();

			// A few more times, on the next real Advance() calls - see the comment on
			// pendingReapplyFrames.
			pendingReapplyFrames = kPendingReapplyFrames;

			logger::debug("InitLocalMap: scaleform wired up, queued {} reapply frame(s), showOnGameStart {}",
						  pendingReapplyFrames, settings::display::showOnGameStart);

			if (settings::display::showOnGameStart)
			{
				Show();
			}
			else
			{
				Hide();
			}
		}
		else
		{
			logger::debug("InitLocalMap: allocation of LocalMapMenu failed; minimap will not be functional");
		}
	}

	bool Minimap::EnsureIconDisplay()
	{
		if (localMap_ == nullptr)
		{
			return false;
		}

		// IsObject(), NOT IsDisplayObject(). IconDisplay is `Map.Display`, declared in the mod's
		// own ActionScript as a plain AS2 class - `class Map.Display`, with no `extends MovieClip`
		// - and constructed as `IconDisplay = new Display(this)` in LocalMap.as. It is a plain
		// object property on the clip, so IsDisplayObject() correctly returns false for it.
		//
		// Testing it with IsDisplayObject() is why the minimap never showed a single marker: the
		// GetMember call succeeded every time and the predicate rejected the result. 1.2.3's
		// retry loop then faithfully retried a test that could never pass.
		if (localMap_->iconDisplay.IsObject())
		{
			return true;
		}

		// Runs from Advance(), so it must not log on every frame it fails - only on the
		// transitions (CLAUDE.md rule 14: never log unconditionally in per-frame code).
		static bool resolvedOnce = false;
		static bool loggedRetrying = false;

		if (!localMap_->root.IsDisplayObject())
		{
			return false;
		}

		if (!localMap_->root.GetMember("IconDisplay", &localMap_->iconDisplay) || !localMap_->iconDisplay.IsObject())
		{
			if (!loggedRetrying)
			{
				logger::debug("EnsureIconDisplay: IconDisplay not present yet; will keep retrying each frame");
				loggedRetrying = true;
			}

			return false;
		}

		// MarkerData is what PopulateData()/RefreshMarkers() write through, so a resolved
		// IconDisplay without it is not actually usable.
		if (!localMap_->iconDisplay.GetMember("MarkerData", &localMap->markerData))
		{
			if (!loggedRetrying)
			{
				logger::debug("EnsureIconDisplay: IconDisplay resolved but MarkerData is not there yet; will keep retrying");
				loggedRetrying = true;
			}

			return false;
		}

		if (!resolvedOnce)
		{
			logger::info("IconDisplay resolved; minimap markers are available");
			resolvedOnce = true;
		}

		return true;
	}

	void Minimap::MeasureStage()
	{
		if (auto* view = displayObj.GetMovieView())
		{
			RE::GFxValue width, height;
			if (view->GetVariable(&width, "Stage.width") && width.IsNumber())
			{
				stageWidth = static_cast<float>(width.GetNumber());
			}
			if (view->GetVariable(&height, "Stage.height") && height.IsNumber())
			{
				stageHeight = static_cast<float>(height.GetNumber());
			}
		}

		logger::info("Minimap stage: {}x{}", stageWidth, stageHeight);

		if (stageWidth <= 0.0F || stageHeight <= 0.0F)
		{
			logger::error("Could not read the Stage size; the minimap cannot be positioned");
		}
	}

	bool Minimap::StageToParent(float a_stageX, float a_stageY, float& a_outX, float& a_outY)
	{
		auto* view = displayObj.GetMovieView();
		if (!view)
		{
			logger::debug("StageToParent: displayObj has no movie view");

			return false;
		}

		// IUI::GFxObject::GetMember hides the two-argument RE version, so use the wrapper's
		// single-argument form.
		RE::GFxValue parent = displayObj.GetMember("_parent");
		if (!parent.IsDisplayObject())
		{
			logger::debug("StageToParent: displayObj's _parent is not a display object");

			return false;
		}

		// globalToLocal rewrites the point object in place. This mirrors the idiom the mod's
		// own ActionScript already uses in LocalMap.as InitMap, which converts the other way
		// with _parent.localToGlobal.
		IUI::GFxObject point(view);
		point.SetMember("x", RE::GFxValue{ static_cast<double>(a_stageX) });
		point.SetMember("y", RE::GFxValue{ static_cast<double>(a_stageY) });

		RE::GFxValue argument = point;
		if (!parent.Invoke("globalToLocal", nullptr, &argument, 1))
		{
			logger::debug("StageToParent: globalToLocal invocation failed");

			return false;
		}

		a_outX = static_cast<float>(point.GetMember("x").GetNumber());
		a_outY = static_cast<float>(point.GetMember("y").GetNumber());

		return true;
	}

	bool Minimap::GetArtBoundsInParent(float& a_left, float& a_top, float& a_right, float& a_bottom)
	{
		// IUI::GFxObject::GetMember hides the two-argument RE version, so use the wrapper's
		// single-argument form.
		RE::GFxValue parent = displayObj.GetMember("_parent");
		if (!parent.IsDisplayObject())
		{
			logger::debug("GetArtBoundsInParent: displayObj's _parent is not a display object");

			return false;
		}

		// getBounds() measures a clip's full render extent regardless of _visible - Flash does
		// not exclude hidden children from it. The Controls clip (the old hide-tip/buttons UI,
		// permanently hidden since 1.6.2 - see InitLocalMap()) is still a child of this tree,
		// so measuring the WHOLE clip's bounds folds in whatever space Controls was authored
		// to occupy, even though nothing of it is drawn. If Controls sits below and to the
		// left of the map artwork, as its old "hold to control" layout suggests, that alone
		// would explain corner-dependent offsets needed to compensate: a left anchor reading
		// artLeft from a box that extends further left than the visible map places the map
		// inset from the true edge, needing a negative correction to pull it back - and
		// likewise a positive one on the bottom, for the same reason in the other direction.
		//
		// Once localMap_ exists, BackgroundArtSquare/BackgroundArtCircle - whichever matches
		// the current shape - is measured instead. That clip is exactly the visible map frame,
		// with no Controls artwork anywhere inside it, so its bounds are not inflated the same
		// way. Before localMap_ exists (the constructor's very first call, before LocalMap has
		// even been constructed) there is nothing more precise to measure yet, so the whole
		// clip's bounds are used as an approximation for that one early call; the later re-apply
		// once localMap_ exists (see InitLocalMap() and pendingReapplyFrames) replaces it with
		// the precise measurement.
		RE::GFxValue bounds;
		bool measured = false;

		if (localMap_)
		{
			using Shape = LMU::PixelShaderProperty::Shape;
			const char* artName = shape == Shape::kRound ? "BackgroundArtCircle" : "BackgroundArtSquare";

			RE::GFxValue art;
			if (localMap_->root.GetMember(artName, &art) && art.IsDisplayObject())
			{
				measured = art.Invoke("getBounds", &bounds, std::array<RE::GFxValue, 1>{ parent }) && bounds.IsObject();
			}

			if (!measured)
			{
				logger::debug("GetArtBoundsInParent: could not measure {} directly; falling back to whole-clip bounds", artName);
			}
		}

		if (!measured && !(displayObj.Invoke("getBounds", &bounds, parent) && bounds.IsObject()))
		{
			logger::debug("GetArtBoundsInParent: whole-clip getBounds also failed");

			return false;
		}

		if (!measured)
		{
			logger::debug("GetArtBoundsInParent: measured via whole-clip bounds fallback (localMap_ {})",
						  localMap_ ? "present" : "not yet available");
		}

		RE::GFxValue xMin, xMax, yMin, yMax;
		bounds.GetMember("xMin", &xMin);
		bounds.GetMember("xMax", &xMax);
		bounds.GetMember("yMin", &yMin);
		bounds.GetMember("yMax", &yMax);

		a_left = static_cast<float>(xMin.GetNumber());
		a_top = static_cast<float>(yMin.GetNumber());
		a_right = static_cast<float>(xMax.GetNumber());
		a_bottom = static_cast<float>(yMax.GetNumber());

		return a_right > a_left && a_bottom > a_top;
	}

	float Minimap::GetMaxScale() const
	{
		// A quarter of the screen means one quadrant: half its width and half its height.
		if (stageWidth <= 0.0F || stageHeight <= 0.0F || artWidthAtScaleOne <= 0.0F || artHeightAtScaleOne <= 0.0F)
		{
			return settings::display::kScaleSliderMax;
		}

		const float byWidth = stageWidth * 0.5F / artWidthAtScaleOne;
		const float byHeight = stageHeight * 0.5F / artHeightAtScaleOne;

		return std::clamp(std::min({ byWidth, byHeight, settings::display::kScaleSliderMax }),
						  settings::display::kScaleSliderMin, settings::display::kScaleSliderMax);
	}

	bool Minimap::ApplyDisplaySettingsOnce(float& a_outDeltaX, float& a_outDeltaY)
	{
		if (!displayObj.HasMember("Minimap") || stageWidth <= 0.0F || stageHeight <= 0.0F)
		{
			logger::debug("ApplyDisplaySettings: skipped, displayObj has Minimap member {}, stage {}x{}",
						  displayObj.HasMember("Minimap"), stageWidth, stageHeight);

			return true;
		}

		const float scale = std::clamp(settings::display::scale, settings::display::kScaleSliderMin, GetMaxScale());

		displayObj.SetMember("_xscale", baseXScale * scale);
		displayObj.SetMember("_yscale", baseYScale * scale);

		float artLeft = 0.0F, artTop = 0.0F, artRight = 0.0F, artBottom = 0.0F;
		if (!GetArtBoundsInParent(artLeft, artTop, artRight, artBottom))
		{
			logger::error("Could not measure the minimap artwork; leaving it where it is");

			return true;
		}

		// Remember how big the artwork is at scale 1, so the quarter-screen cap has something
		// real to work from rather than a value guessed before the map was attached.
		if (scale > 0.0F)
		{
			artWidthAtScaleOne = (artRight - artLeft) / scale;
			artHeightAtScaleOne = (artBottom - artTop) / scale;
		}

		using Anchor = settings::display::Anchor;
		const int corner = settings::display::AnchorIndex();
		const auto anchor = static_cast<Anchor>(corner);

		const bool atRight = anchor == Anchor::kTopRight || anchor == Anchor::kBottomRight;
		const bool atBottom = anchor == Anchor::kBottomLeft || anchor == Anchor::kBottomRight;

		// Each corner keeps its own nudge, so switching corners does not carry over the
		// adjustment made to a different one.
		const float offsetX = settings::display::offsetX[corner];
		const float offsetY = settings::display::offsetY[corner];

		// Where the artwork's chosen corner should end up, in stage pixels.
		const float wantStageX = (atRight ? stageWidth : 0.0F) + offsetX;
		const float wantStageY = (atBottom ? stageHeight : 0.0F) + offsetY;

		float wantX = 0.0F, wantY = 0.0F;
		if (!StageToParent(wantStageX, wantStageY, wantX, wantY))
		{
			logger::error("Could not reach the minimap's parent clip; leaving it where it is");

			return true;
		}

		// Move by the difference between where the chosen edge is and where it should be,
		// rather than computing an absolute _x. The registration point can sit anywhere
		// inside the artwork and this does not care where.
		const float currentX = static_cast<float>(displayObj.GetMember("_x").GetNumber());
		const float currentY = static_cast<float>(displayObj.GetMember("_y").GetNumber());

		// Deliberately not clamped to stay on screen any more. The offset is what the player
		// asked for, and clamping it here fought that choice on some corners for reasons that
		// were never fully pinned down (see PORT-NOTES.md). It also turned out to be
		// unnecessary: fScale is already capped, in GetMaxScale(), to whatever keeps the
		// artwork within a quarter of the screen, which is what actually stops the map from
		// growing large enough to need rescuing from going off screen. An offset large enough
		// to push a quarter-screen-sized map off screen is a choice, not a scale runaway, and
		// is left alone.
		const float newX = currentX + (wantX - (atRight ? artRight : artLeft));
		const float newY = currentY + (wantY - (atBottom ? artBottom : artTop));

		displayObj.SetMember("_x", newX);
		displayObj.SetMember("_y", newY);

		// How far this pass actually had to move the clip. Invoking InitMap below re-derives the
		// map extents and changes the artwork's measured bounds, so a single pass positions
		// against a measurement that is stale the moment it is used. The caller repeats until
		// this delta is negligible.
		a_outDeltaX = newX - currentX;
		a_outDeltaY = newY - currentY;

		// For the log line below only - StageToParent is not otherwise needed once the
		// clamp is gone, but knowing where the screen edges landed is still useful for
		// diagnosing anything that still looks wrong.
		float screenMinX = 0.0F, screenMinY = 0.0F, screenMaxX = 0.0F, screenMaxY = 0.0F;
		StageToParent(0.0F, 0.0F, screenMinX, screenMinY);
		StageToParent(stageWidth, stageHeight, screenMaxX, screenMaxY);

		// The map extents the renderer uses are worked out once, by the ActionScript InitMap,
		// from the clip's geometry at that moment. Moving or rescaling the clip afterwards
		// leaves them stale, so ask for them again.
		if (localMap_)
		{
			localMap_->root.Invoke("InitMap");
		}

		logger::info("Display applied: anchor {}, offset ({}, {}), scale {} -> _x {}, _y {} "
					 "(art {},{} to {},{}, screen {},{} to {},{})",
					 settings::display::anchor, offsetX, offsetY,
					 scale, newX, newY, artLeft, artTop, artRight, artBottom,
					 screenMinX, screenMinY, screenMaxX, screenMaxY);

		return true;

	}

	// Runs ApplyDisplaySettingsOnce() until the position stops moving.
	//
	// One pass is not enough and never was. Each pass measures the artwork, positions the clip
	// from that measurement, then invokes InitMap to refresh the map extents - and InitMap
	// changes the artwork's geometry, so the measurement the position was derived from is
	// already stale. The next pass corrects by a smaller amount, and so on.
	//
	// the author and a second user on another machine both logged the same signature: the measured
	// art width climbing 102.7 -> 136.3 -> 153.1 -> 169.9 -> 186.7 before settling, with _x
	// walking ~9.2px each time. Because the passes were spread across separate frames and
	// events, that showed up in game as the minimap visibly growing and sliding after a load
	// rather than simply appearing where it belongs. Converging here, inside one call, makes
	// it a single invisible step.
	//
	// Bounded rather than looping until stable: if some future change makes this oscillate
	// instead of converge, a capped loop degrades to the old visible drift rather than hanging
	// the render thread.
	void Minimap::ApplyDisplaySettings()
	{
		constexpr int kMaxPasses = 8;
		constexpr float kSettledPixels = 0.5F;

		int pass = 0;
		float deltaX = 0.0F, deltaY = 0.0F;

		for (; pass < kMaxPasses; ++pass)
		{
			deltaX = 0.0F;
			deltaY = 0.0F;

			if (!ApplyDisplaySettingsOnce(deltaX, deltaY))
			{
				// The pass bailed out - it has already logged why. Nothing to converge on.
				return;
			}

			if (std::abs(deltaX) < kSettledPixels && std::abs(deltaY) < kSettledPixels)
			{
				break;
			}
		}

		if (pass >= kMaxPasses)
		{
			logger::warn("ApplyDisplaySettings: position still moving after {} passes (last delta {}, {}); "
						 "leaving it here", kMaxPasses, deltaX, deltaY);
		}
		else
		{
			logger::debug("ApplyDisplaySettings: settled after {} pass(es)", pass + 1);
		}

		// Remember where this left the clip so Advance() can detect the artwork having stopped
		// changing size between calls, which is a different question from converging within one.
		lastAppliedX = static_cast<float>(displayObj.GetMember("_x").GetNumber());
		lastAppliedY = static_cast<float>(displayObj.GetMember("_y").GetNumber());

		ApplyTitlePosition();
	}

	void Minimap::ApplyTitlePosition()
	{
		if (!localMap_)
		{
			return;
		}

		RE::GFxValue locationName, clearedHint, mapHolder;
		if (!localMap_->root.GetMember("LocationName", &locationName) || !locationName.IsDisplayObject() ||
			!localMap_->root.GetMember("ClearedHint", &clearedHint) || !clearedHint.IsDisplayObject() ||
			!localMap_->root.GetMember("LocalMapHolder", &mapHolder) || !mapHolder.IsDisplayObject())
		{
			logger::error("Could not reach LocationName/ClearedHint/LocalMapHolder; leaving the title where it is");

			return;
		}

		const auto getNum = [](RE::GFxValue& a_obj, const char* a_member) {
			RE::GFxValue value;
			a_obj.GetMember(a_member, &value);
			return static_cast<float>(value.GetNumber());
		};

		const float mapTop = getNum(mapHolder, "_y");
		const float mapBottom = mapTop + getNum(mapHolder, "_height");

		const float nameY = getNum(locationName, "_y");
		const float hintY = getNum(clearedHint, "_y");

		if (!hasTitleGeometry)
		{
			const float nameBottom = nameY + getNum(locationName, "_height");
			const float hintBottom = hintY + getNum(clearedHint, "_height");

			const float groupTop = std::min(nameY, hintY);
			const float groupBottom = std::max(nameBottom, hintBottom);

			titleGroupHeight = groupBottom - groupTop;
			titleNameOffset = nameY - groupTop;
			titleHintOffset = hintY - groupTop;

			// Whichever edge the title started nearer to is the gap it was authored with; the
			// same magnitude is used on the other edge when the group is later moved there.
			titleGap = std::min(std::abs(groupTop - mapBottom), std::abs(mapTop - groupBottom));

			hasTitleGeometry = true;

			logger::info("Title layout: gap {}, group height {}, name offset {}, hint offset {}",
						 titleGap, titleGroupHeight, titleNameOffset, titleHintOffset);
		}

		using Anchor = settings::display::Anchor;
		const auto anchor = static_cast<Anchor>(settings::display::AnchorIndex());
		const bool atBottom = anchor == Anchor::kBottomLeft || anchor == Anchor::kBottomRight;

		// Anchored to the top of the screen, the title goes below the map, out of the way of
		// the screen edge the map itself is pushed against; anchored to the bottom, above it.
		const float groupTop = atBottom ? mapTop - titleGap - titleGroupHeight : mapBottom + titleGap;

		locationName.SetMember("_y", groupTop + titleNameOffset);
		clearedHint.SetMember("_y", groupTop + titleHintOffset);
	}

	void Minimap::ApplyShapeSetting()
	{
		const Shape newShape = static_cast<Shape>(settings::display::shape);

		if (newShape == shape)
		{
			return;
		}

		shape = newShape;

		if (!localMap_)
		{
			return;
		}

		// The AS2 SetShape duplicates the chosen background art into a clip called
		// "backgroundArtMask", hands that to VisionCone.setMask, and hides the other shape's
		// art. It was written to run once. Run twice it leaves two problems behind:
		//
		//  - the previous duplicate is still on the display list, and once setMask has moved
		//    on to the new one it stops being a mask and starts being drawn, so the old shape
		//    reappears alongside the new one;
		//  - the art for the shape being switched *to* was hidden by the previous call, and
		//    SetShape only ever hides the alternative, never re-shows the chosen one, so the
		//    duplicate it takes as the new mask is invisible.
		//
		// Clearing the stale duplicate and un-hiding both arts first puts the clip back in the
		// state SetShape expects to find.
		RE::GFxValue staleMask;
		if (localMap_->root.GetMember("backgroundArtMask", &staleMask) && staleMask.IsDisplayObject())
		{
			staleMask.Invoke("removeMovieClip");
		}

		for (const char* artName : { "BackgroundArtSquare", "BackgroundArtCircle" })
		{
			RE::GFxValue art;
			if (localMap_->root.GetMember(artName, &art) && art.IsDisplayObject())
			{
				art.SetMember("_visible", RE::GFxValue{ true });
			}
		}

		localMap_->root.Invoke("SetShape", std::array<RE::GFxValue, 1>{ static_cast<std::uint32_t>(shape) });

		logger::debug("Shape set to {}", shape == Shape::kRound ? "round" : "squared");
	}

	RE::Setting* Minimap::GetHUDOpacitySetting()
	{
		// Resolved once. A null result is cached too - if it is not there on the first look it
		// will not appear later, and re-scanning every frame would be wasteful.
		static bool resolved = false;
		static RE::Setting* setting = nullptr;

		if (resolved)
		{
			return setting;
		}

		resolved = true;

		auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
		auto* ini = RE::INISettingCollection::GetSingleton();

		// SkyrimPrefs.ini keeps fHUDOpacity under [MAIN] on this machine, but the engine's own
		// registered name for it is what GetSetting matches, and that has not proven to be the
		// same thing. Try what it plausibly is, in both collections.
		// Confirmed in game 2026-08-25: the engine registers this with no section suffix at all,
		// as plain "fHUDOpacity", even though SkyrimPrefs.ini files it under [MAIN]. The section
		// in the file is not part of the name GetSetting matches. Both earlier guesses -
		// ":Display" (1.1.8-1.2.2) and ":MAIN" (1.2.3-1.2.6) - were therefore wrong, and each
		// silently fell back to fully opaque. The known-correct name goes first; the rest stay as
		// fallbacks in case another runtime or a mod registers it differently.
		constexpr const char* kCandidates[] = {
			"fHUDOpacity",
			"fHUDOpacity:MAIN",
			"fHUDOpacity:Interface",
			"fHUDOpacity:Display",
		};

		for (const char* name : kCandidates)
		{
			if (prefs)
			{
				if (RE::Setting* found = prefs->GetSetting(name))
				{
					logger::info("HUD Opacity setting resolved as \"{}\" in SkyrimPrefs.ini", name);
					setting = found;

					return setting;
				}
			}

			if (ini)
			{
				if (RE::Setting* found = ini->GetSetting(name))
				{
					logger::info("HUD Opacity setting resolved as \"{}\" in Skyrim.ini", name);
					setting = found;

					return setting;
				}
			}
		}

		// Nothing matched. Rather than guess a fifth name next time, report what the engine
		// actually has, so the real name is in the log.
		int reported = 0;

		auto dump = [&](RE::INISettingCollection* a_collection, const char* a_which) {
			if (!a_collection)
			{
				return;
			}

			for (RE::Setting* candidate : a_collection->settings)
			{
				if (!candidate || !candidate->name)
				{
					continue;
				}

				if (std::string_view(candidate->name).find("HUDOpacity") != std::string_view::npos)
				{
					logger::warn("HUD Opacity: {} has a setting named \"{}\" - none of the names tried matched it",
								 a_which, candidate->name);
					++reported;
				}
			}
		};

		dump(prefs, "SkyrimPrefs.ini");
		dump(ini, "Skyrim.ini");

		if (reported == 0)
		{
			logger::warn("HUD Opacity: no setting containing \"HUDOpacity\" exists in either collection; the minimap background will stay fully opaque");
		}

		return nullptr;
	}

	void Minimap::ApplyBackgroundOpacity()
	{
		// Called every Advance() while visible - only log when the resolved alpha actually
		// changes since last frame, not on every call, or Debug mode would fill the log with
		// nothing but this line.
		static float lastLoggedAlpha = -1.0F;

		if (!localMap_)
		{
			return;
		}

		const char* artName = shape == Shape::kRound ? "BackgroundArtCircle" : "BackgroundArtSquare";

		RE::GFxValue art;
		if (!localMap_->root.GetMember(artName, &art) || !art.IsDisplayObject())
		{
			return;
		}

		// Falls back to fully opaque (matches this clip's pre-1.6.8 behavior) if the engine
		// setting couldn't be found, rather than crashing on a null read - see the comment on
		// hudOpacitySetting in MiniMap.h for why this check exists at all.
		float hudOpacity = 1.0F;
		RE::Setting* hudOpacitySetting = GetHUDOpacitySetting();
		if (hudOpacitySetting)
		{
			hudOpacity = hudOpacitySetting->data.f;
		}
		else
		{
			static bool warnedMissingSetting = false;
			if (!warnedMissingSetting)
			{
				logger::warn("HUD Opacity setting could not be resolved; "
							 "the minimap background will stay fully opaque regardless of the HUD Opacity slider");
				warnedMissingSetting = true;
			}
		}

		const double alpha = std::clamp(hudOpacity, 0.0F, 1.0F) * 100.0;

		RE::GFxValue::DisplayInfo displayInfo;
		art.GetDisplayInfo(&displayInfo);
		displayInfo.SetAlpha(alpha);
		art.SetDisplayInfo(displayInfo);

		if (static_cast<float>(alpha) != lastLoggedAlpha)
		{
			logger::debug("Background opacity applied: fHUDOpacity {} -> alpha {} on {}", hudOpacity, alpha, artName);

			lastLoggedAlpha = static_cast<float>(alpha);
		}
	}

	void Minimap::SetMapZoom(float a_zoom)
	{
		if (!cameraContext || !cameraContext->defaultState)
		{
			logger::debug("SetMapZoom({}): ignored, camera context {} available", a_zoom, cameraContext ? "has no default state" : "not");

			return;
		}

		const float delta = a_zoom - cameraContext->defaultState->zoom;

		// Steer the absolute value through zoomInput, which is the same channel the pan/zoom
		// controls use, so the camera applies its own limits rather than us guessing at them.
		cameraContext->zoomInput += delta;

		logger::debug("SetMapZoom: requested {}, current {}, zoomInput += {}", a_zoom, cameraContext->defaultState->zoom, delta);
	}

	void Minimap::ToggleZoomPreset()
	{
		if (!cameraContext)
		{
			logger::debug("ToggleZoomPreset: ignored, no camera context yet");

			return;
		}

		if (!cameraContext->defaultState)
		{
			logger::debug("ToggleZoomPreset: ignored, camera context has no default state yet");

			return;
		}

		// Alternates deterministically rather than jumping to "whichever preset the camera is
		// currently further from": if the player has scrolled the map to a third value between
		// the two, distance-based picking can toggle back and forth between the same target,
		// or pick one arbitrarily depending on which side of the midpoint they landed on. A
		// remembered on/off state means a tap always does what it did last time.
		zoomedIn = !zoomedIn;

		const float target = zoomedIn ? settings::controls::zoomZoomedIn : settings::controls::zoomDefault;

		logger::info("Zoom toggle: camera reports {}, targeting {} ({})",
					 cameraContext->defaultState->zoom, target, zoomedIn ? "zoomed in" : "default");

		SetMapZoom(target);
	}

	void Minimap::SetLocalMapExtents(const RE::FxDelegateArgs& a_delegateArgs)
	{
		float localLeft = a_delegateArgs[0].GetNumber();
		float localTop = a_delegateArgs[1].GetNumber();
		float localRight = a_delegateArgs[2].GetNumber();
		float localBottom = a_delegateArgs[3].GetNumber();

		float identityMat2D[2][3] = { { 1.0F, 0.0F, 0.0F }, { 0.0F, 1.0F, 0.0F } };

		RE::GPointF localTopLeft{ localLeft, localTop };
		localMap->topLeft = view->TranslateToScreen(localTopLeft, identityMat2D);

		RE::GPointF localBottomRight{ localRight, localBottom };
		localMap->bottomRight = view->TranslateToScreen(localBottomRight, identityMat2D);

		float aspectRatio = (localMap->bottomRight.x - localMap->topLeft.x) / (localMap->bottomRight.y - localMap->topLeft.y);

		cameraContext->defaultState->minFrustumHalfWidth = aspectRatio * cameraContext->defaultState->minFrustumHalfHeight;

		minCamFrustumHalfWidth = cameraContext->defaultState->minFrustumHalfWidth;
		minCamFrustumHalfHeight = cameraContext->defaultState->minFrustumHalfHeight;

		logger::debug("SetLocalMapExtents: local ({},{})-({},{}) -> screen ({},{})-({},{}), aspect {}, minFrustumHalf {}x{}",
					  localLeft, localTop, localRight, localBottom,
					  localMap->topLeft.x, localMap->topLeft.y, localMap->bottomRight.x, localMap->bottomRight.y,
					  aspectRatio, minCamFrustumHalfWidth, minCamFrustumHalfHeight);
	}

	void Minimap::Advance()
	{
		// Re-apply until the position stops moving, rather than for a fixed number of frames.
		//
		// The artwork keeps resizing for the first few seconds after a load - the map texture
		// arrives, InitMap re-derives the extents - and each re-apply aligns whatever it measures
		// at that moment, moving the clip roughly 9.3 units. How many re-applies that takes is not
		// fixed, so the old countdown of 6 was a guess that was sometimes one short, leaving the
		// minimap ~9 units off with its frame past the screen edge. ApplyDisplaySettings()
		// converges within a single call; this converges across calls, which is the other half of
		// the same problem and the half 1.2.5 did not fix.
		if (pendingReapplyFrames > 0)
		{
			--pendingReapplyFrames;

			const float beforeX = lastAppliedX;
			const float beforeY = lastAppliedY;

			ApplyDisplaySettings();

			constexpr float kStillPixels = 0.5F;

			if (std::abs(lastAppliedX - beforeX) < kStillPixels && std::abs(lastAppliedY - beforeY) < kStillPixels)
			{
				++displayStableFrames;

				if (displayStableFrames >= kRequiredStableFrames)
				{
					logger::info("Display settled at _x {}, _y {} after {} re-applies", lastAppliedX, lastAppliedY,
								 kPendingReapplyFrames - pendingReapplyFrames);
					pendingReapplyFrames = 0;
				}
			}
			else
			{
				// Still moving - restart the run of quiet frames.
				displayStableFrames = 0;
			}

			if (pendingReapplyFrames == 0 && displayStableFrames < kRequiredStableFrames)
			{
				logger::warn("Display never settled within {} re-applies; left at _x {}, _y {}",
							 kPendingReapplyFrames, lastAppliedX, lastAppliedY);
			}
		}

		// ---- AE diagnostic ------------------------------------------------------------------
		// A reporter on Anniversary Edition 1.6.1170 sees no minimap at all while its settings
		// menu works normally. Their log showed this block never executing once in a session -
		// zero background-opacity applications, zero scaleform refreshes - which explains the
		// symptom exactly, since nothing below here runs and nothing above it checks visibility.
		//
		// Which half of the condition is false is not known, so report them separately rather
		// than guessing. Transition-gated (rule 14): this runs every frame, so it logs only when
		// the picture actually changes.
		{
			const bool visible = IsVisible();
			const bool shown = IsShown();
			const bool haveLocalMap = localMap_ != nullptr;
			const bool enabled = haveLocalMap && localMap_->enabled;
			const bool displayIsObject = displayObj.IsDisplayObject();

			RE::GFxValue::DisplayInfo info;
			const bool gotInfo = displayObj.GetDisplayInfo(&info);

			const auto state = std::make_tuple(visible, shown, haveLocalMap, enabled, displayIsObject, gotInfo);
			static std::optional<std::remove_const_t<decltype(state)>> lastLogged;

			if (!lastLogged || *lastLogged != state)
			{
				lastLogged = state;
				logger::info("Visibility gate: IsVisible={} IsShown={} | localMap_={} enabled={} displayObj.IsDisplayObject={} GetDisplayInfo={}",
							 visible, shown, haveLocalMap, enabled, displayIsObject, gotInfo);

				if (!visible || !shown)
				{
					logger::warn("Visibility gate CLOSED - the minimap will not be drawn this frame. IsVisible() reads displayObj's own _visible; IsShown() reads localMap_->enabled, which Show() sets on root, a different object.");
				}
			}
		}
		// ---- end AE diagnostic --------------------------------------------------------------

		if (IsVisible() && IsShown())
		{
			ApplyBackgroundOpacity();

			RE::GFxValue updateScaleform = displayObj.GetMember("updateScaleform");

			// Clearing the flag is the correct thing to do - SetBoolean() on the value GetMember()
			// returns writes to a local copy and never reaches the ActionScript object, so before
			// 1.2.8 it was never cleared at all.
			//
			// But it must NOT gate the work below. 1.2.8 cleared the flag correctly and moved this
			// whole block behind it, which dropped it from ~19,000 executions a session to exactly
			// one - and every marker, the player rotation arrow and the vision cone stopped updating,
			// because all of that is per-frame work that only ever ran at all thanks to the flag
			// never being cleared. The accident was load-bearing. Confirmed in game by the author: markers
			// vanished on 1.2.8 having worked on 1.2.6.
			if (updateScaleform.GetBool())
			{
				displayObj.SetMember("updateScaleform", RE::GFxValue{ false });
				logger::debug("Advance: updateScaleform flag was set; cleared it");
			}

			// Per-frame from here: actors move in and out of range, the player turns, the camera
			// rotates. A bare scope, so the body below keeps its existing indentation.
			{

				std::array<RE::GFxValue, 2> title;

				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
				float playerCameraRotation = RE::PlayerCamera::GetSingleton()->GetRuntimeData2().yaw;

				float cellNorthRotation = 0.0F;

				if (RE::TESObjectCELL* parentCell = player->parentCell)
				{
					cellNorthRotation = -parentCell->GetNorthRotation();

					if (settings::controls::followPlayerCameraRotation)
					{
						cameraContext->SetNorthRotation(playerCameraRotation);
					}
					else
					{
						cameraContext->SetNorthRotation(cellNorthRotation);
					}
					cameraContext->zRotation = 0.0F;

					if (parentCell->IsInteriorCell())
					{
						title[0] = parentCell->GetFullName();

						logger::debug("Advance: title from interior cell \"{}\"", title[0].GetString());
					}
					else if (RE::BGSLocation* location = parentCell->GetLocation())
					{
						title[0] = location->GetFullName();

						if (location->everCleared)
						{
							// Falls back to leaving title[1] unset (same as the interior-cell and
							// worldspace branches above, which never set it at all) if the game
							// setting couldn't be found, rather than crashing on a null read - see
							// the comment on clearedStrSetting in MiniMap.h.
							if (clearedStrSetting)
							{
								title[1] = clearedStrSetting->data.s;
							}
							else
							{
								static bool warnedMissingClearedStr = false;
								if (!warnedMissingClearedStr)
								{
									logger::warn("Game setting \"sCleared\" was not found; the cleared-location suffix will be omitted from the title");
									warnedMissingClearedStr = true;
								}
							}
						}

						// Per-frame path - log only when the location or its cleared state actually changes,
						// per CLAUDE.md rule 14. Ungated, this produced 18,989 identical lines in one session.
						{
							const std::string currentTitle = title[0].GetString() ? title[0].GetString() : "";
							const bool currentCleared = static_cast<bool>(location->everCleared);

							static std::string lastTitle;
							static bool lastCleared = false;
							static bool everLogged = false;

							if (!everLogged || currentTitle != lastTitle || currentCleared != lastCleared)
							{
								everLogged = true;
								lastTitle = currentTitle;
								lastCleared = currentCleared;

								logger::debug("Advance: title from location \"{}\", everCleared {}", currentTitle, currentCleared);
							}
						}
					}
					else
					{
						RE::TESWorldSpace* worldSpace = player->GetWorldspace();
						title[0] = worldSpace->GetFullName();

						logger::debug("Advance: title from worldspace \"{}\"", title[0].GetString());
					}
				}
				else
				{
					logger::debug("Advance: player has no parentCell; title left unset");
				}

				localMap_->root.Invoke("SetTitle", nullptr, title);
				
				localMap->PopulateData();

				// Re-resolves IconDisplay if the one-shot lookup in InitLocalMap() was too early.
				if (EnsureIconDisplay())
				{
					localMap_->iconDisplay.Invoke("CreateMarkers");
				}

				localMap->RefreshMarkers();

				if (settings::controls::followPlayerCameraRotation)
				{
					RE::GFxValue youAreHereMarker;
					if (EnsureIconDisplay() &&
						localMap_->iconDisplay.GetMember("YouAreHereMarker", &youAreHereMarker) && youAreHereMarker.IsDisplayObject())
					{
						float playerToCamAngle = player->GetAngleZ() - playerCameraRotation;
						float playerToCamAngleDeg = playerToCamAngle * 180 * std::numbers::inv_pi;

						RE::GFxValue::DisplayInfo youAreHereMarkerDisplayInfo;
						youAreHereMarker.GetDisplayInfo(&youAreHereMarkerDisplayInfo);
						youAreHereMarkerDisplayInfo.SetRotation(playerToCamAngleDeg);
						youAreHereMarker.SetDisplayInfo(youAreHereMarkerDisplayInfo);
					}
					else
					{
						static bool warnedMissingYouAreHereMarker = false;
						if (!warnedMissingYouAreHereMarker)
						{
							logger::warn("Advance: could not reach YouAreHereMarker; player rotation marker will not be updated");
							warnedMissingYouAreHereMarker = true;
						}
					}
				}
				else
				{
					RE::GFxValue visionCone;
					if (localMap_->root.GetMember("VisionCone", &visionCone) && visionCone.IsDisplayObject())
					{
						float playerCameraToNorthAngle = playerCameraRotation - cellNorthRotation;
						float playerCameraToNorthAngleDeg = playerCameraToNorthAngle * 180 * std::numbers::inv_pi;

						RE::GFxValue::DisplayInfo visionConeDisplayInfo;
						visionCone.GetDisplayInfo(&visionConeDisplayInfo);
						visionConeDisplayInfo.SetRotation(playerCameraToNorthAngleDeg);
						visionCone.SetDisplayInfo(visionConeDisplayInfo);
					}
					else
					{
						static bool warnedMissingVisionCone = false;
						if (!warnedMissingVisionCone)
						{
							logger::warn("Advance: could not reach VisionCone; vision cone rotation will not be updated");
							warnedMissingVisionCone = true;
						}
					}
				}

			}

			isCameraUpdatePending = true;
		}
	}

	void Minimap::PreRender()
	{
		if (IsVisible() && IsShown())
		{
			if (isCameraUpdatePending)
			{
				RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

				RE::NiPoint3 playerPos = player->GetPosition();
				cameraContext->defaultState->initialPosition.x = playerPos.x;
				cameraContext->defaultState->initialPosition.y = playerPos.y;

				// Nothing writes this any more - panning was the only thing that did - but
				// zeroing it here still matters: it is what recenters the camera on the player
				// on the frame after a cell change, before Update() runs.
				cameraContext->defaultState->translation = RE::NiPoint3::Zero();

				cameraContext->Update();

				isCameraUpdatePending = false;

				RE::LoadedAreaBound* loadedAreaBound = RE::TES::GetSingleton()->GetRuntimeData2().loadedAreaBound;
				cameraContext->SetAreaBounds(loadedAreaBound->maxExtent, loadedAreaBound->minExtent);

				if (isFogOfWarEnabled)
				{
					UpdateFogOfWar();
				}

				RenderOffScreen();
			}
		}
	}

}
