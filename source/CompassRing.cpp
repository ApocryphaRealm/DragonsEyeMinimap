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

		// Full circle with 12 curveTo segments (the AS2 drawing API has no arc primitive).
		void CirclePath(RE::GFxValue& a_clip, double a_cx, double a_cy, double a_r)
		{
			constexpr int kSegs = 12;
			constexpr double kStep = 2.0 * std::numbers::pi / kSegs;
			const double ctrlR = a_r / std::cos(kStep * 0.5);
			Draw2(a_clip, "moveTo", a_cx + a_r, a_cy);
			for (int i = 1; i <= kSegs; ++i)
			{
				const double aEnd = i * kStep;
				const double aMid = aEnd - kStep * 0.5;
				std::array<RE::GFxValue, 4> args{ RE::GFxValue{ a_cx + std::cos(aMid) * ctrlR }, RE::GFxValue{ a_cy + std::sin(aMid) * ctrlR },
												  RE::GFxValue{ a_cx + std::cos(aEnd) * a_r }, RE::GFxValue{ a_cy + std::sin(aEnd) * a_r } };
				(void)a_clip.Invoke("curveTo", nullptr, args.data(), args.size());
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
			if (!g_clip.Invoke("createTextField", nullptr, create.data(), create.size())) { return false; }
			RE::GFxValue field;
			if (!g_clip.GetMember(a_name, &field)) { return false; }
			field.SetMember("selectable", RE::GFxValue{ false });
			field.SetMember("embedFonts", RE::GFxValue{ true });
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
			RE::GFxValue root;
			if (!view->GetVariable(&root, "_root") || !root.IsDisplayObject()) { return; }
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
			// Backing disc like the round minimap's plate, then the ring and its ticks.
			NoLine(g_clip);
			std::array<RE::GFxValue, 2> fill{ RE::GFxValue{ ToRgb(0x000000) }, RE::GFxValue{ static_cast<double>(settings::compass::discAlpha) } };
			(void)g_clip.Invoke("beginFill", nullptr, fill.data(), fill.size());
			CirclePath(g_clip, cx, cy, radius + settings::compass::ringThickness * 2.0);
			(void)g_clip.Invoke("endFill");

			LineStyle(g_clip, settings::compass::ringThickness, settings::compass::ringColor, 100.0);
			CirclePath(g_clip, cx, cy, radius);

			for (int i = 0; i < 8; ++i)
			{
				const double bearing = i * (kPi / 4.0);
				const bool cardinal = (i % 2) == 0;
				const double tick = cardinal ? 8.0 : 4.0;
				double x0, y0, x1, y1;
				onRing(bearing, radius - tick * 2.0, x0, y0);
				onRing(bearing, radius, x1, y1);
				LineStyle(g_clip, settings::compass::ringThickness,
						  i == 0 ? settings::compass::northColor : settings::compass::ringColor, 100.0);
				Draw2(g_clip, "moveTo", x0, y0);
				Draw2(g_clip, "lineTo", x1, y1);
				if (cardinal && g_fieldsMade)
				{
					double lx, ly;
					onRing(bearing, radius - tick * 2.0 - 14.0, lx, ly);
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
					if (settings::compass::metricUnits) { std::snprintf(buf, sizeof(buf), "%d m%s", static_cast<int>(d3 * 0.01428 + 0.5), up ? " ^" : (down ? " v" : "")); }
					else { std::snprintf(buf, sizeof(buf), "%d ft%s", static_cast<int>(d3 * 0.01428 * 3.2808 + 0.5), up ? " ^" : (down ? " v" : "")); }
					double lx, ly;
					onRing(bearing, radius - settings::compass::pointerSize - 22.0, lx, ly);
					SetField("labDist", buf, lx, ly - settings::compass::labelSize * 0.7, true);
					distShown = true;
				}
			}
		}
		if (!distShown) { SetField("labDist", "", 0, 0, false); }
	}
}
