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
		bool g_loggedFirstDraw = false;

		constexpr const char* kClipName = "DEMCompassRing";
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
			constexpr int kSegs = 32;
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

		float PlayerHeading()
		{
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
	}

	void Update()
	{
		const bool ringOn = settings::compass::compassRing;
		const bool pointerOn = settings::compass::questPointer;
		if (!ringOn && !pointerOn) { return; }

		auto* mini = Minimap::GetSingleton();
		if (!mini || !mini->IsReady()) { return; }

		auto* view = mini->GetHudMovieView();
		if (!view) { return; }

		// Draw only during real gameplay: paused covers every blocking menu (map, journal,
		// inventory, tween ...); this runs on the main thread so RE::UI is safe here.
		auto* ui = RE::UI::GetSingleton();
		const bool gameplay = ui && !ui->GameIsPaused();

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

		const bool ringNow = ringOn && gameplay && !mapVisible;
		const bool pointerNow = pointerOn && gameplay;
		const char* labels[4] = { "labN", "labE", "labS", "labW" };
		if (!ringNow)
		{
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
		if (ringNow)
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
				constexpr int kRingSegs = 24;
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
				// The vanilla compass's notched arrowhead: two triangles sharing tip and notch.
				FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { tipX, tipY }, { blX, blY }, { nX, nY } });
				FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { tipX, tipY }, { nX, nY }, { brX, brY } });

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
					SetField("labDist", buf, lx, ly - settings::compass::labelSize * 0.7, true);
					// Above/below the target: the old widget's small triangle beside the number,
					// not a text caret.
					if (up || down)
					{
						const double gx = lx + tw * 0.5 + 4.0, gy = ly;
						if (up) { FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { gx, gy - 2.4 }, { gx - 2.0, gy + 1.6 }, { gx + 2.0, gy + 1.6 } }); }
						else { FillPoly(g_clip, settings::compass::pointerColor, 100.0, { { gx, gy + 2.4 }, { gx - 2.0, gy - 1.6 }, { gx + 2.0, gy - 1.6 } }); }
					}
					distShown = true;
				}
			}
		}
		if (!distShown) { SetField("labDist", "", 0, 0, false); }
	}
}
