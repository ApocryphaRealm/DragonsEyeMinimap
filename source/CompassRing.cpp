#include "CompassRing.h"

#include "MiniMap.h"
#include "Settings.h"

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace DEM::compassring
{
	namespace
	{
		// Cached Scaleform objects, all owned by the HUD movie. Rebuilt whenever they stop being
		// display objects (movie reload).
		RE::GFxValue g_clip;
		bool g_fieldsMade = false;
		// 1.7.2: whether the clip is in HUDMovieBaseInstance.HudElements with its mode flags set - see
		// RegisterWithHud. Reset with the clip.
		bool g_hudRegistered = false;
		bool g_loggedFirstDraw = false;

		constexpr const char* kClipName = "DEMCompassRing";

		// 1.9.0 NORDIC STYLE (the owner, 2026-11-06: "a much higher quality graphic generated with Blender that looks like a
		// real compass ... the quest compass arm is a separate compass arm that points in its direction" - "make it nordic
		// looking"). The art is rendered by tools/compass/compass_scene.py and packed by build_compass.py into a SWF of five
		// named sprites, each centred on (0, 0): body (fixed), rose and north (the red north arm; both turn against the camera
		// heading, so they point at world north), needle (the gilded quest arm, turns to the quest bearing), cap (fixed). It is a SWF, not a DDS, because Scaleform cannot load a loose DDS (img:// only finds
		// images the game registered - past-mistakes library, LMU 1.3.3); loadMovie of a SWF under Interface is the route
		// HUD Position Manager's widgets already prove. Loaded into demNordic.art, a child of our clip at a NEGATIVE depth
		// so the distance text fields (depths 0..4) stay above it.
		constexpr const char* kArtUrl = "DragonsEyeMinimap/compass.swf";
		constexpr double kArtOuterRadius = 0.98 * 256.0;   // build_compass.py: OUTER_FRACTION of the 512 px art's half-width
		constexpr int kArtLoadFrames = 600;                // ~5-10 s; after that the Simple ring is drawn for the session
		RE::GFxValue g_nordic;     // holder: position, scale, visibility
		RE::GFxValue g_over;       // above the art: the distance readout's above/below glyph in Nordic style
		bool g_artRequested = false;
		bool g_artLoaded = false;
		bool g_artFailed = false;
		int g_artWaitFrames = 0;
		// last values written, for the driving tool (main thread only)
		double g_lastRoseRot = 0.0;
		double g_lastNeedleRot = 0.0;
		bool g_lastNeedleShown = false;
		bool g_lastNordicShown = false;
		bool g_lastNorthShown = false;
		constexpr const char* kFont = "$EverywhereBoldFont";  // the vanilla HUD's embedded face

		double ToRgb(std::uint32_t a_rgb) { return static_cast<double>(a_rgb & 0xFFFFFFu); }

		void Draw2(RE::GFxValue& a_clip, const char* a_call, double a_x, double a_y)
		{
			std::array<RE::GFxValue, 2> args{ RE::GFxValue{ a_x }, RE::GFxValue{ a_y } };
			(void)a_clip.Invoke(a_call, nullptr, args.data(), args.size());
		}

		// Full circle as 32 straight segments. curveTo NEVER renders in this movie (the red-
		// block diagnostic proved moveTo/lineTo/beginFill work while every curveTo path
		// produced nothing), so the polygonal circle is the reliable primitive.
		void CirclePath(RE::GFxValue& a_clip, double a_cx, double a_cy, double a_r)
		{
			constexpr int kSegs = 64;
			constexpr double kStep = 2.0 * std::numbers::pi / kSegs;
			Draw2(a_clip, "moveTo", a_cx + a_r, a_cy);
			for (int i = 1; i <= kSegs; ++i)
			{
				const double a = i * kStep;
				Draw2(a_clip, "lineTo", a_cx + std::cos(a) * a_r, a_cy + std::sin(a) * a_r);
			}
		}

		void LineStyle(RE::GFxValue& a_clip, double a_thickness, std::uint32_t a_rgb, double a_alpha)
		{
			std::array<RE::GFxValue, 3> style{ RE::GFxValue{ a_thickness }, RE::GFxValue{ ToRgb(a_rgb) }, RE::GFxValue{ a_alpha } };
			(void)a_clip.Invoke("lineStyle", nullptr, style.data(), style.size());
		}

		void NoLine(RE::GFxValue& a_clip)
		{
			// lineStyle() with no arguments removes the stroke entirely - alpha 0 still draws hairlines.
			(void)a_clip.Invoke("lineStyle");
		}

		void FillPoly(RE::GFxValue& a_clip, std::uint32_t a_rgb, double a_alpha, const std::vector<std::pair<double, double>>& a_pts)
		{
			if (a_pts.size() < 3) { return; }
			NoLine(a_clip);
			std::array<RE::GFxValue, 2> fill{ RE::GFxValue{ ToRgb(a_rgb) }, RE::GFxValue{ a_alpha } };
			(void)a_clip.Invoke("beginFill", nullptr, fill.data(), fill.size());
			Draw2(a_clip, "moveTo", a_pts[0].first, a_pts[0].second);
			for (std::size_t i = 1; i < a_pts.size(); ++i) { Draw2(a_clip, "lineTo", a_pts[i].first, a_pts[i].second); }
			Draw2(a_clip, "lineTo", a_pts[0].first, a_pts[0].second);
			(void)a_clip.Invoke("endFill");
		}

		// One TextField per label, created once with the clip. Name -> member lookup per frame.
		bool MakeField(RE::GFxMovieView* a_view, const char* a_name, double a_size, std::uint32_t a_rgb)
		{
			RE::GFxValue nextDepth;
			double depth = 100.0;
			if (g_clip.Invoke("getNextHighestDepth", &nextDepth) && nextDepth.IsNumber()) { depth = nextDepth.GetNumber(); }
			std::array<RE::GFxValue, 6> create{ RE::GFxValue{ a_name }, RE::GFxValue{ depth },
												RE::GFxValue{ 0.0 }, RE::GFxValue{ 0.0 }, RE::GFxValue{ 200.0 }, RE::GFxValue{ 40.0 } };
			if (!g_clip.Invoke("createTextField", nullptr, create.data(), create.size()))
			{
				logger::warn("CompassRing: createTextField('{}') Invoke returned false", a_name);
				return false;
			}
			RE::GFxValue field;
			if (!g_clip.GetMember(a_name, &field))
			{
				logger::warn("CompassRing: field '{}' not found after createTextField", a_name);
				return false;
			}
			field.SetMember("selectable", RE::GFxValue{ false });
			// embedFonts=false: with true, a font name that fails to resolve in THIS movie
			// renders NOTHING - and runtime-created fields have no authored embed to fall back
			// on (the SWF-authored title fields carry their own; these do not). The device font
			// always renders; the TextFormat below still asks for the HUD face first.
			field.SetMember("embedFonts", RE::GFxValue{ false });
			field.SetMember("autoSize", RE::GFxValue{ "center" });
			RE::GFxValue tf;
			a_view->CreateObject(&tf, "TextFormat");
			if (tf.IsObject())
			{
				tf.SetMember("font", RE::GFxValue{ kFont });
				tf.SetMember("size", RE::GFxValue{ a_size });
				tf.SetMember("color", RE::GFxValue{ ToRgb(a_rgb) });
				std::array<RE::GFxValue, 1> args{ tf };
				(void)field.Invoke("setNewTextFormat", nullptr, args.data(), args.size());
			}
			return true;
		}

		void SetField(const char* a_name, const char* a_text, double a_x, double a_y, bool a_visible)
		{
			RE::GFxValue field;
			if (!g_clip.GetMember(a_name, &field) || !field.IsDisplayObject()) { return; }
			field.SetMember("_visible", RE::GFxValue{ a_visible });
			if (!a_visible) { return; }
			field.SetMember("text", RE::GFxValue{ a_text });
			field.SetMember("_x", RE::GFxValue{ a_x });
			field.SetMember("_y", RE::GFxValue{ a_y });
		}

		bool Child(RE::GFxValue& a_parent, const char* a_name, RE::GFxValue& a_out)
		{
			return a_parent.IsDisplayObject() && a_parent.GetMember(a_name, &a_out) && a_out.IsDisplayObject();
		}

		bool MakeChildClip(RE::GFxValue& a_parent, const char* a_name, double a_depth, RE::GFxValue& a_out)
		{
			if (Child(a_parent, a_name, a_out)) { return true; }
			std::array<RE::GFxValue, 2> create{ RE::GFxValue{ a_name }, RE::GFxValue{ a_depth } };
			return a_parent.Invoke("createEmptyMovieClip", &a_out, create.data(), create.size()) && a_out.IsDisplayObject();
		}

		// Creates the holder, asks for the art once and reports whether it has arrived. Runs whenever the Nordic style is
		// chosen, shown or not, so the art is there by the time the map is hidden. Not ready = draw the Simple ring.
		bool EnsureNordicArt()
		{
			if (g_artFailed) { return false; }
			if (!g_nordic.IsDisplayObject())
			{
				if (!MakeChildClip(g_clip, "demNordic", -100.0, g_nordic))
				{
					g_artFailed = true;
					logger::warn("CompassRing: could not create the Nordic holder clip; the Simple ring is drawn instead");
					return false;
				}
				g_nordic.SetMember("_visible", RE::GFxValue{ false });
				g_artRequested = g_artLoaded = false;
				g_artWaitFrames = 0;
			}
			RE::GFxValue art;
			if (!MakeChildClip(g_nordic, "art", 1.0, art))
			{
				g_artFailed = true;
				logger::warn("CompassRing: could not create the Nordic art clip; the Simple ring is drawn instead");
				return false;
			}
			if (!g_artRequested)
			{
				g_artRequested = true;
				g_artWaitFrames = 0;
				RE::GFxValue url{ kArtUrl };
				const bool called = art.Invoke("loadMovie", nullptr, &url, 1);
				logger::info("CompassRing: Nordic art loadMovie(\"{}\") {}", kArtUrl, called ? "called" : "FAILED");
			}
			if (g_artLoaded) { return true; }
			RE::GFxValue rose, needle, body;
			if (Child(art, "rose", rose) && Child(art, "needle", needle) && Child(art, "body", body))
			{
				g_artLoaded = true;
				RE::GFxValue u;
				logger::info("CompassRing: Nordic art loaded after {} frame(s) from {}", g_artWaitFrames,
							 art.GetMember("_url", &u) && u.IsString() ? u.GetString() : "(no _url)");
				return true;
			}
			if (++g_artWaitFrames > kArtLoadFrames)
			{
				g_artFailed = true;
				g_nordic.SetMember("_visible", RE::GFxValue{ false });
				RE::GFxValue u;
				logger::warn("CompassRing: the Nordic art did not arrive within {} frames (Interface/{} missing or unreadable? art _url {}); the Simple ring is drawn instead",
							 kArtLoadFrames, kArtUrl, art.GetMember("_url", &u) && u.IsString() ? u.GetString() : "(none)");
			}
			return false;
		}

		void SetNum(RE::GFxValue& a_obj, const char* a_member, double a_value)
		{
			a_obj.SetMember(a_member, RE::GFxValue{ a_value });
		}

		// The way the camera looks, as the game's own compass turns, not the way the body faces (the owner, 2026-11-06: the ring
		// turned with the character, not the camera). The same yaw the minimap's camera follow reads.
		float PlayerHeading()
		{
			if (auto* camera = RE::PlayerCamera::GetSingleton()) { return camera->GetRuntimeData2().yaw; }
			if (auto* player = RE::PlayerCharacter::GetSingleton()) { return player->GetAngleZ(); }
			return 0.0F;
		}

		// Ported from the DragonsEyePointers reference: nearest DISPLAYED objective with a
		// resolvable world position. Interior targets resolve to the location's worldLocMarker;
		// other ROOT worldspaces are skipped. Runs on the MAIN thread (AdvanceMovie hook).
		bool NearestObjective(const RE::NiPoint3& a_from, RE::NiPoint3& a_out, float& a_outDist)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			bool found = false;
			float best = 0.0F;
			static std::vector<RE::TESQuest*> running;
			static int refresh = 0;
			if ((refresh++ % 4) == 0)
			{
				running.clear();
				if (auto* dh = RE::TESDataHandler::GetSingleton())
				{
					for (auto* q : dh->GetFormArray<RE::TESQuest>()) { if (q && q->IsRunning()) { running.push_back(q); } }
				}
			}
			for (auto* quest : running)
			{
				for (auto* obj : quest->objectives)
				{
					if (!obj || obj->state != RE::QUEST_OBJECTIVE_STATE::kDisplayed || !obj->targets) { continue; }
					for (std::uint32_t i = 0; i < obj->numTargets; ++i)
					{
						const auto* target = obj->targets[i];
						if (!target) { continue; }
						RE::BGSBaseAlias* base = nullptr;
						for (auto* a : quest->aliases)
						{
							if (a && a->aliasID == target->alias) { base = a; break; }
						}
						if (!base || base->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID) { continue; }
						auto* ref = static_cast<RE::BGSRefAlias*>(base)->GetReference();
						if (!ref) { continue; }
						RE::NiPoint3 pos = ref->GetPosition();
						if (auto* cell = ref->GetParentCell(); cell && cell->IsInteriorCell())
						{
							RE::TESObjectREFR* marker = nullptr;
							for (auto* loc = cell->GetLocation(); loc && !marker; loc = loc->parentLoc)
							{
								if (auto m = loc->worldLocMarker.get(); m) { marker = m.get(); }
							}
							if (!marker) { continue; }
							ref = marker;
							pos = marker->GetPosition();
						}
						auto rootOf = [](RE::TESWorldSpace* w) { while (w && w->parentWorld) { w = w->parentWorld; } return w; };
						if (auto* pws = rootOf(player->GetWorldspace()); pws && rootOf(ref->GetWorldspace()) && rootOf(ref->GetWorldspace()) != pws) { continue; }
						const float dx = pos.x - a_from.x, dy = pos.y - a_from.y;
						const float d = std::sqrt(dx * dx + dy * dy);
						if (d <= 256.0F) { continue; }
						if (!found || d < best) { found = true; best = d; a_out = pos; }
					}
				}
			}
			a_outDist = best;
			return found;
		}
	}

	void Reset()
	{
		g_clip = RE::GFxValue{};
		g_fieldsMade = false;
		g_hudRegistered = false;
		g_nordic = RE::GFxValue{};
		g_over = RE::GFxValue{};
		g_artRequested = g_artLoaded = g_artFailed = false;
		g_artWaitFrames = 0;
	}

	// THE HUD'S OWN HIDE-IN-MENU FLAGS (the owner, 2026-09-17: "make sure the compass widget has the
	// proper hud hide in menu flags"). HUDMovieBaseInstance.ShowElements(mode) walks HudElements and
	// sets each one's _visible to hasOwnProperty(mode) - that is how the vanilla HUD hides its parts
	// for the inventory, the world map, the journal, the tween menu, sleep/wait, barter, books and
	// so on, and shows them again after. The compass clip was drawn on the HUD root outside that
	// list, gated only by GameIsPaused(), so it followed no mode at all. It now registers itself
	// in HudElements and owns exactly the modes the vanilla compass (CompassShoutMeterHolder) owns:
	// All, Favor, DialogueMode, StealthMode, Swimming, HorseMode, WarHorseMode - less DialogueMode (the owner,
	// 2026-11-02: it hides in conversation with the minimap), and hidden in every menu mode like the compass. A marker property keeps a clip
	// adopted after Reset() from being pushed twice.
	static constexpr const char* kHudModes[] = { "All", "Favor", "StealthMode", "Swimming", "HorseMode", "WarHorseMode" };
	static constexpr const char* kRegisteredMarker = "demCompassInHudElements";

	static void RegisterWithHud(RE::GFxMovieView* a_view)
	{
		if (g_hudRegistered || !a_view || !g_clip.IsDisplayObject()) { return; }
		for (const char* mode : kHudModes) { g_clip.SetMember(mode, RE::GFxValue{ true }); }
		RE::GFxValue hud;
		if (!a_view->GetVariable(&hud, "_level0.HUDMovieBaseInstance") || !hud.IsObject())
		{
			static bool warned = false;
			if (!warned) { warned = true; logger::warn("CompassRing: _level0.HUDMovieBaseInstance not found; the compass cannot follow the HUD's mode flags"); }
			return;
		}
		RE::GFxValue elements;
		if (!hud.GetMember("HudElements", &elements) || !elements.IsArray())
		{
			static bool warned = false;
			if (!warned) { warned = true; logger::warn("CompassRing: HUDMovieBaseInstance.HudElements is not an array; the compass cannot follow the HUD's mode flags"); }
			return;
		}
		if (!g_clip.HasMember(kRegisteredMarker))
		{
			elements.PushBack(g_clip);
			g_clip.SetMember(kRegisteredMarker, RE::GFxValue{ true });
		}
		g_hudRegistered = true;
		logger::info("CompassRing: registered in HudElements (now {} elements) with modes All, Favor, StealthMode, Swimming, HorseMode, WarHorseMode", elements.GetArraySize());
	}

	// For the driving tool: the HUD's current mode stack, whether the clip is registered and what
	// _visible the HUD left it at, beside the vanilla compass holder's. Main thread only.
	std::string HudVisibilityJson(RE::GFxMovieView* a_view)
	{
		std::string modes; std::uint32_t count = 0; bool inList = false; int clipVisible = -1, holderVisible = -1;
		RE::GFxValue hud;
		if (a_view && a_view->GetVariable(&hud, "_level0.HUDMovieBaseInstance") && hud.IsObject())
		{
			RE::GFxValue arr;
			if (hud.GetMember("HUDModes", &arr) && arr.IsArray())
			{
				for (std::uint32_t i = 0; i < arr.GetArraySize(); ++i) { RE::GFxValue m; if (arr.GetElement(i, &m) && m.IsString()) { modes += (modes.empty() ? "" : ","); modes += m.GetString(); } }
			}
			RE::GFxValue elements;
			if (hud.GetMember("HudElements", &elements) && elements.IsArray()) { count = elements.GetArraySize(); }
			RE::GFxValue holder, v;
			if (hud.GetMember("CompassShoutMeterHolder", &holder) && holder.IsDisplayObject() && holder.GetMember("_visible", &v) && v.IsBool()) { holderVisible = v.GetBool() ? 1 : 0; }
		}
		if (g_clip.IsDisplayObject())
		{
			inList = g_clip.HasMember(kRegisteredMarker);
			RE::GFxValue v; if (g_clip.GetMember("_visible", &v) && v.IsBool()) { clipVisible = v.GetBool() ? 1 : 0; }
		}
		char nordic[256];
		std::snprintf(nordic, sizeof(nordic),
					  ",\"style\":\"%s\",\"nordicArt\":\"%s\",\"nordicShown\":%s,\"roseRotation\":%.1f,\"northArm\":%s,\"needleShown\":%s,\"needleRotation\":%.1f",
					  settings::compass::compassStyle == 0 ? "Nordic" : "Simple",
					  g_artFailed ? "failed" : (g_artLoaded ? "loaded" : (g_artRequested ? "loading" : "not requested")),
					  g_lastNordicShown ? "true" : "false", g_lastRoseRot, g_lastNorthShown ? "true" : "false", g_lastNeedleShown ? "true" : "false", g_lastNeedleRot);
		return "{\"modes\":\"" + modes + "\",\"hudElements\":" + std::to_string(count) + ",\"registered\":" + (inList ? "true" : "false") +
			",\"clipVisible\":" + std::to_string(clipVisible) + ",\"vanillaCompassVisible\":" + std::to_string(holderVisible) + nordic + "}";
	}

	void Update()
	{
		const bool ringOn = settings::compass::compassRing;
		const bool pointerOn = settings::compass::questPointer;
		// With both off, one more pass still runs: it clears what the last frame drew. Returning at once left the quest
		// pointer on screen after it was switched off with the compass ring already off (a Nexus report, 2026-11-08).
		static bool s_cleared = false;
		if (!ringOn && !pointerOn)
		{
			if (s_cleared) { return; }
			s_cleared = true;
		}
		else { s_cleared = false; }

		auto* mini = Minimap::GetSingleton();
		if (!mini || !mini->IsReady()) { return; }

		auto* view = mini->GetHudMovieView();
		if (!view) { return; }

		// Which menus hide the compass is the HUD's decision, through the mode flags this clip owns
		// (RegisterWithHud) - the same ones the vanilla compass owns - not a pause check. 1.6.6's
		// GameIsPaused() gate hid it in paused states where the vanilla compass stays (the console)
		// and could not hide it in un-paused ones where the HUD does.
		const bool gameplay = true;

		// The stage-rect statics keep the last SHOWN measurement, exactly what the ring needs.
		float l, t, r, b, sw, sh;
		bool shown;
		{
			std::scoped_lock lock(Minimap::stageRectLock);
			l = Minimap::stageRect.left; t = Minimap::stageRect.top;
			r = Minimap::stageRect.right; b = Minimap::stageRect.bottom;
			sw = Minimap::stageRectStageW; sh = Minimap::stageRectStageH;
			shown = Minimap::stageRectFromShown;
		}
		const bool mapVisible = mini->IsShown() && mini->IsVisible();
		if (sw <= 0.0F || sh <= 0.0F || (r - l) <= 8.0F) { return; }

		// (Re)create our clip on the HUD ROOT - not under the minimap clip, which is _visible
		// false exactly when the ring must show.
		if (!g_clip.IsDisplayObject())
		{
			// "_root" resolved to an EMPTY root on the first build (clip landed at depth 0 and
			// never drew): under Infinity UI the minimap's view is a patched sub-movie whose _root
			// is not the HUD. _level0 is the actual HUD movie root in every configuration.
			RE::GFxValue root;
			if (!view->GetVariable(&root, "_level0") || !root.IsDisplayObject())
			{
				if (!view->GetVariable(&root, "_root") || !root.IsDisplayObject()) { return; }
				logger::debug("CompassRing: _level0 unavailable, using _root");
			}
			RE::GFxValue existing;
			if (root.GetMember(kClipName, &existing) && existing.IsDisplayObject())
			{
				g_clip = existing;
			}
			else
			{
				RE::GFxValue nextDepth;
				double depth = 9000.0;
				if (root.Invoke("getNextHighestDepth", &nextDepth) && nextDepth.IsNumber()) { depth = nextDepth.GetNumber(); }
				std::array<RE::GFxValue, 2> create{ RE::GFxValue{ kClipName }, RE::GFxValue{ depth } };
				if (!root.Invoke("createEmptyMovieClip", &g_clip, create.data(), create.size()) || !g_clip.IsDisplayObject())
				{
					static bool warned = false;
					if (!warned) { warned = true; logger::warn("CompassRing: createEmptyMovieClip failed; the compass will not draw"); }
					return;
				}
				g_fieldsMade = false;
				logger::info("CompassRing: clip created on the HUD root at depth {}", depth);
			}
		}
		RegisterWithHud(view);

		if (!g_fieldsMade)
		{
			const auto labelRgb = settings::compass::ringColor;
			g_fieldsMade = MakeField(view, "labN", settings::compass::labelSize, settings::compass::northColor) &&
						   MakeField(view, "labE", settings::compass::labelSize, labelRgb) &&
						   MakeField(view, "labS", settings::compass::labelSize, labelRgb) &&
						   MakeField(view, "labW", settings::compass::labelSize, labelRgb) &&
						   MakeField(view, "labDist", settings::compass::labelSize, settings::compass::pointerColor);
			if (!g_fieldsMade)
			{
				static bool warned = false;
				if (!warned) { warned = true; logger::warn("CompassRing: createTextField failed; labels disabled"); }
			}
		}

		(void)g_clip.Invoke("clear");
		if (!g_over.IsDisplayObject()) { (void)MakeChildClip(g_clip, "demOver", 50.0, g_over); }
		if (g_over.IsDisplayObject()) { (void)g_over.Invoke("clear"); }

		const bool ringNow = ringOn && gameplay && !mapVisible;
		const bool nordicChosen = settings::compass::compassStyle == 0;
		const bool artReady = nordicChosen && EnsureNordicArt();
		const bool nordic = ringNow && artReady;
		if (!nordic && g_nordic.IsDisplayObject() && g_lastNordicShown) { g_nordic.SetMember("_visible", RE::GFxValue{ false }); }
		if (nordic != g_lastNordicShown) { logger::debug("CompassRing: Nordic compass {}", nordic ? "shown" : "hidden"); }
		g_lastNordicShown = nordic;
		if (!nordic) { g_lastNeedleShown = false; }

		// 1.6.6: the owner asked whether the ring shows in the Tween menu, where the minimap is hidden -
		// which is exactly the case the ring exists for. `gameplay` is !GameIsPaused(), and a paused
		// menu would suppress the ring at the very moment it should appear. Log the gate's inputs the
		// first time the minimap goes hidden, so the answer is read rather than inferred.
		{
			static bool s_loggedHidden = false;
			if (!mapVisible && !s_loggedHidden)
			{
				s_loggedHidden = true;
				logger::info("CompassRing gate with the map hidden: ringOn {}, gameplay {} (paused {}), mapVisible {} -> ring {}",
							 ringOn, gameplay, !gameplay, mapVisible, ringNow ? "SHOWN" : "hidden");
			}
		}
		const bool pointerNow = pointerOn && gameplay;
		const char* labels[4] = { "labN", "labE", "labS", "labW" };
		if (!ringNow || nordic)
		{
			// Nordic: the letters are carved into the rose art
			for (const char* n : labels) { SetField(n, "", 0, 0, false); }
		}
		if (!ringNow && !pointerNow)
		{
			SetField("labDist", "", 0, 0, false);
			return;
		}

		const double cx = (l + r) * 0.5, cy = (t + b) * 0.5;
		const double radius = std::min(r - l, b - t) * 0.5 - settings::compass::ringGap;
		if (radius <= 4.0) { return; }
		const float heading = PlayerHeading();
		auto onRing = [&](double a_bearing, double a_r, double& x, double& y) {
			const double a = a_bearing - heading;  // screen angle, 0 = up
			x = cx + std::sin(a) * a_r;
			y = cy - std::cos(a) * a_r;
		};

		if (!g_loggedFirstDraw)
		{
			g_loggedFirstDraw = true;
			logger::info("CompassRing: first draw - rect ({:.0f},{:.0f})-({:.0f},{:.0f}) centre ({:.0f},{:.0f}) radius {:.0f} shownRect {} mapVisible {}",
						 l, t, r, b, cx, cy, radius, shown, mapVisible);
		}

		constexpr double kPi = std::numbers::pi;
		constexpr double kDeg = 180.0 / std::numbers::pi;
		// the Nordic compass fills the map's inscribed circle - exactly the place the map was
		const double nordicOuter = radius + settings::compass::ringGap;
		RE::GFxValue art, needleClip;
		if (nordic)
		{
			const double scale = nordicOuter / kArtOuterRadius * 100.0;
			SetNum(g_nordic, "_x", cx);
			SetNum(g_nordic, "_y", cy);
			SetNum(g_nordic, "_xscale", scale);
			SetNum(g_nordic, "_yscale", scale);
			g_nordic.SetMember("_visible", RE::GFxValue{ true });
			if (Child(g_nordic, "art", art))
			{
				RE::GFxValue body, rose;
				// the backing disc's opacity setting carries over to the stone body
				if (Child(art, "body", body)) { SetNum(body, "_alpha", static_cast<double>(std::min<std::uint32_t>(settings::compass::discAlpha, 100))); }
				// the card turns AGAINST the heading, so north on the card stays on the world's north
				g_lastRoseRot = -static_cast<double>(heading) * kDeg;
				if (Child(art, "rose", rose)) { SetNum(rose, "_rotation", g_lastRoseRot); }
				// the red NORTH arm turns with the rose: it always points to world north (the owner, 2026-11-06: "have
				// the red compass north arm point north ... the compass rim or graphic stays stationary"). The quest
				// needle below is a separate, gilded arm.
				RE::GFxValue northArm;
				g_lastNorthShown = Child(art, "north", northArm);
				if (g_lastNorthShown) { SetNum(northArm, "_rotation", g_lastRoseRot); }
				(void)Child(art, "needle", needleClip);
				if (!pointerNow && needleClip.IsDisplayObject())
				{
					needleClip.SetMember("_visible", RE::GFxValue{ false });   // the quest pointer is switched off
					g_lastNeedleShown = false;
				}
			}
		}
		if (ringNow && !nordic)
		{
			// Backing disc (lineTo circle), then the ring as a SEGMENTED QUAD STRIP - every
			// element on the proven moveTo/lineTo/beginFill primitives only (curveTo renders
			// nothing in this movie; the red-block diagnostic settled it).
			const double th = settings::compass::ringThickness;
			NoLine(g_clip);
			std::array<RE::GFxValue, 2> fill{ RE::GFxValue{ ToRgb(0x000000) }, RE::GFxValue{ static_cast<double>(settings::compass::discAlpha) } };
			(void)g_clip.Invoke("beginFill", nullptr, fill.data(), fill.size());
			CirclePath(g_clip, cx, cy, radius + th);
			(void)g_clip.Invoke("endFill");

			{
				constexpr int kRingSegs = 48;
				constexpr double kSegStep = 2.0 * std::numbers::pi / kRingSegs;
				const double ro = radius + th * 0.5, ri = radius - th * 0.5;
				for (int s = 0; s < kRingSegs; ++s)
				{
					const double a0 = s * kSegStep, a1 = (s + 1) * kSegStep;
					FillPoly(g_clip, settings::compass::ringColor, 100.0,
							 { { cx + std::cos(a0) * ro, cy + std::sin(a0) * ro },
							   { cx + std::cos(a1) * ro, cy + std::sin(a1) * ro },
							   { cx + std::cos(a1) * ri, cy + std::sin(a1) * ri },
							   { cx + std::cos(a0) * ri, cy + std::sin(a0) * ri } });
				}
			}

			for (int i = 0; i < 8; ++i)
			{
				const double bearing = i * (kPi / 4.0);
				const bool cardinal = (i % 2) == 0;
				// The old separate widget's proportions, converted from its screen pixels to
				// stage pixels (the HUD movie scales ~2.5x onto a 3200-wide screen).
				const double tick = cardinal ? 3.2 : 1.6;
				double x0, y0, x1, y1;
				onRing(bearing, radius - tick * 2.0, x0, y0);
				onRing(bearing, radius, x1, y1);
				// filled quad perpendicular to the tick direction
				const double dx = x1 - x0, dy = y1 - y0;
				const double len = std::sqrt(dx * dx + dy * dy);
				const double px = len > 0.0 ? -dy / len * (th * 0.5) : 0.0;
				const double py = len > 0.0 ? dx / len * (th * 0.5) : 0.0;
				FillPoly(g_clip, i == 0 ? settings::compass::northColor : settings::compass::ringColor, 100.0,
						 { { x0 - px, y0 - py }, { x1 - px, y1 - py }, { x1 + px, y1 + py }, { x0 + px, y0 + py } });
				if (cardinal && g_fieldsMade)
				{
					double lx, ly;
					onRing(bearing, radius - tick * 2.0 - 4.8, lx, ly);
					static const char* kText[4] = { "N", "E", "S", "W" };
					SetField(labels[i / 2], kText[i / 2], lx, ly - settings::compass::labelSize * 0.7, true);
				}
			}
		}

		bool distShown = false;
		if (pointerNow)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			static RE::NiPoint3 target;
			static float dist = 0.0F;
			static bool have = false;
			static int frame = 0;
			if (player && (frame++ % 30) == 0) { have = NearestObjective(player->GetPosition(), target, dist); }
			const bool needleNow = nordic && player && have;
			if (needleClip.IsDisplayObject())
			{
				needleClip.SetMember("_visible", RE::GFxValue{ needleNow });
				if (needleNow != g_lastNeedleShown) { logger::debug("CompassRing: quest needle {}", needleNow ? "shown" : "hidden (no displayed objective)"); }
				g_lastNeedleShown = needleNow;
			}
			if (player && have && (ringNow || mapVisible))
			{
				const RE::NiPoint3 from = player->GetPosition();
				const double bearing = std::atan2(target.x - from.x, target.y - from.y);
				const double a = bearing - heading;
				const double ux = std::sin(a), uy = -std::cos(a);
				const double rx = std::cos(a), ry = std::sin(a);
				double tipX, tipY, baseX, baseY;
				onRing(bearing, radius, tipX, tipY);
				onRing(bearing, radius - settings::compass::pointerSize, baseX, baseY);
				const double half = settings::compass::pointerSize * 0.42;
				const double blX = baseX - rx * half, blY = baseY - ry * half;
				const double brX = baseX + rx * half, brY = baseY + ry * half;
				const double nX = baseX + ux * settings::compass::pointerSize * 0.38, nY = baseY + uy * settings::compass::pointerSize * 0.38;
				if (nordic)
				{
					// the gilded quest needle turns to the bearing, in the art's own frame (up = 0, clockwise)
					g_lastNeedleRot = a * kDeg;
					if (needleClip.IsDisplayObject()) { SetNum(needleClip, "_rotation", g_lastNeedleRot); }
				}
				else
				{
					// The vanilla compass's notched arrowhead: two triangles sharing tip and notch.
					FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { tipX, tipY }, { blX, blY }, { nX, nY } });
					FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { tipX, tipY }, { nX, nY }, { brX, brY } });
				}

				if (g_fieldsMade)
				{
					const double dz = target.z - from.z;
					const double d3 = std::sqrt(static_cast<double>(target.x - from.x) * (target.x - from.x) +
												static_cast<double>(target.y - from.y) * (target.y - from.y) + dz * dz);
					char buf[40];
					const bool up = dz > 840.0, down = dz < -840.0;
					int blen;
					if (settings::compass::metricUnits) { blen = std::snprintf(buf, sizeof(buf), "%d m", static_cast<int>(d3 * 0.01428 + 0.5)); }
					else { blen = std::snprintf(buf, sizeof(buf), "%d ft", static_cast<int>(d3 * 0.01428 * 3.2808 + 0.5)); }
					// The old separate widget's placement: clear the marker whatever the bearing by
					// stepping inward by the marker plus half the text extent along the bearing
					// direction. Width estimated (device font, ~0.55 em per character).
					const double tw = blen * settings::compass::labelSize * 0.65;
					const double inward = settings::compass::pointerSize + 4.0 +
										  0.5 * (tw * std::abs(std::sin(a)) + settings::compass::labelSize * std::abs(std::cos(a)));
					double lx, ly;
					onRing(bearing, radius - inward, lx, ly);
					if (nordic)
					{
						// Nordic: on the stone rim at the bottom, where nothing turns under it
						lx = cx;
						ly = cy + nordicOuter * 0.89;
					}
					SetField("labDist", buf, lx, ly - settings::compass::labelSize * 0.7, true);
					// Above/below the target: the old widget's small triangle beside the number,
					// not a text caret.
					RE::GFxValue& glyphClip = (nordic && g_over.IsDisplayObject()) ? g_over : g_clip;   // above the art
					if (up || down)
					{
						const double gx = lx + tw * 0.5 + 4.0, gy = ly;
						if (up) { FillPoly(glyphClip, settings::compass::pointerColor, 100.0, { { gx, gy - 2.4 }, { gx - 2.0, gy + 1.6 }, { gx + 2.0, gy + 1.6 } }); }
						else { FillPoly(glyphClip, settings::compass::pointerColor, 100.0, { { gx, gy + 2.4 }, { gx - 2.0, gy - 1.6 }, { gx + 2.0, gy - 1.6 } }); }
					}
					distShown = true;
				}
			}
		}
		if (!distShown) { SetField("labDist", "", 0, 0, false); }
	}
}
