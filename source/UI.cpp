#include "UI.h"

#include "SKSEMenuFramework.h"

#include "MiniMap.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		// Which key, if any, the next keypress should be bound to. kNone means the Bind
		// buttons are idle; OnInputEvent clears it back to kNone as soon as it captures one.
		enum class BindTarget
		{
			kNone,
			kHide,
			kZoom
		};
		std::atomic<BindTarget> bindTarget{ BindTarget::kNone };
		SKSEMenuFramework::Model::InputEvent* inputHook = nullptr;

		constexpr const char* kShapeNames[] = { "Squared", "Round" };
		constexpr int kShapeCount = 2;

		constexpr const char* kAnchorNames[] = { "Top left", "Top right", "Bottom left", "Bottom right" };
		constexpr int kAnchorCount = 4;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		// The framework renders from the renderer's present hook, which is not the thread
		// Scaleform and the rest of the game expect to be talked to. Anything that reaches into
		// the minimap has to be handed to the main thread first.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface())
			{
				taskInterface->AddTask(std::move(a_task));
			}
		}

		// The bundled header reaches ImGui through the framework's exported cimgui entry points.
		// Older builds of SKSE Menu Framework do not export them, and every widget call in
		// Render() would then call through a null function pointer, so refuse to register
		// unless the ones this panel needs are all there.
		bool HasRequiredExports()
		{
			// These are the exported names the ImGuiMCP wrappers actually resolve, which is not
			// always the name of the function being called: the varargs ones forward to a
			// va_list variant, so TextDisabled resolves igTextDisabledV, not igTextDisabled.
			// Probing the wrong name lets Register() succeed and then jump through a null
			// pointer on the first draw, which is exactly what this guard exists to stop.
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igSeparator",
				"igCheckbox",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igInputInt",
				"igIsKeyPressed_Bool",
				"igIsItemClicked",
				"igIsItemActive",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igIndent",
				"igUnindent",
				"igPushItemWidth",
				"igPopItemWidth"
			};

			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("SKSE Menu Framework does not export \"{}\"", name);

					return false;
				}
			}

			return true;
		}

		// Runs on the framework's input thread. Only ever writes the scan code and clears the
		// target, so there is nothing here that needs the main thread.
		bool __stdcall OnInputEvent(RE::InputEvent* a_event)
		{
			const BindTarget target = bindTarget.load();
			if (target == BindTarget::kNone)
			{
				return false;
			}

			auto* buttonEvent = a_event ? a_event->AsButtonEvent() : nullptr;

			// IsPressed() (any nonzero value) rather than IsDown() (only the very first frame
			// of the press): the framework's own contract for when this callback fires and
			// with which frame of the event is undocumented, so accepting any pressed frame is
			// the more robust match. Only capturing once is handled below by clearing the
			// target, not by requiring a particular frame.
			if (!buttonEvent || !buttonEvent->IsPressed())
			{
				return false;
			}

			// Keyboard only, to match what Controls.cpp compares against. Storing a mouse
			// IDCode here would bind a code that also matches a low keyboard scan code.
			if (buttonEvent->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
			{
				return false;
			}

			const auto code = static_cast<std::int32_t>(buttonEvent->GetIDCode());

			if (target == BindTarget::kHide)
			{
				settings::controls::hideKeyCode = code;
				logger::debug("Hide key bound to key code {}", code);
			}
			else if (target == BindTarget::kZoom)
			{
				settings::controls::zoomToggleKeyCode = code;
				logger::debug("Zoom toggle key bound to key code {}", code);
			}

			bindTarget.store(BindTarget::kNone);

			// Swallow it, so binding a key does not also trigger whatever it is bound to.
			return true;
		}

		// A slider that the arrow keys can also nudge, once it has been clicked. Dragging is
		// hopeless for the last decimal place, and the framework does not turn on ImGui's own
		// keyboard navigation, so this tracks the selection itself rather than changing a
		// setting shared with every other mod's page.
		bool NudgeableSlider(const char* a_label, float* a_value, float a_min, float a_max,
							 const char* a_format, float a_step)
		{
			bool changed = ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);

			if (ImGuiMCP::IsItemClicked() || ImGuiMCP::IsItemActive())
			{
				selectedSlider = a_label;
			}

			if (selectedSlider == a_label)
			{
				float nudge = 0.0F;

				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow))
				{
					nudge -= a_step;
				}
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow))
				{
					nudge += a_step;
				}

				if (nudge != 0.0F)
				{
					*a_value = std::clamp(*a_value + nudge, a_min, a_max);
					changed = true;
				}

				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("<-->");
			}

			return changed;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("(?)");

			if (ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", a_description);
			}
		}

		// A key-code box plus a Bind button that captures the next keypress into it. Shared by
		// the hide key and the zoom key so the two behave identically.
		void KeyBindRow(const char* a_label, std::int32_t* a_keyCode, BindTarget a_target, const char* a_bindButtonId)
		{
			int keyCode = *a_keyCode;
			if (ImGuiMCP::InputInt(a_label, &keyCode))
			{
				*a_keyCode = keyCode < 0 ? 0 : keyCode;
				logger::debug("{} set to {} (typed)", a_label, *a_keyCode);
			}

			ImGuiMCP::SameLine();

			if (bindTarget.load() == a_target)
			{
				if (ImGuiMCP::Button("Press a key... (cancel)"))
				{
					bindTarget.store(BindTarget::kNone);
					logger::debug("{} bind cancelled", a_label);
				}
			}
			else if (ImGuiMCP::Button(a_bindButtonId))
			{
				bindTarget.store(a_target);
				logger::debug("{} bind started; waiting for a keypress", a_label);
			}
		}

		void RenderDisplaySection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Display");

			bool changed = false;

			// Everything below feeds `changed`, which is acted on at the end of the section.
			int anchor = static_cast<int>(display::anchor);
			if (ImGuiMCP::Combo("Corner", &anchor, kAnchorNames, kAnchorCount))
			{
				display::anchor = static_cast<std::uint32_t>(anchor);
				changed = true;
			}
			HelpMarker("Which screen corner the minimap sits in. With both offsets at 0 the artwork lines up flush with that corner.");

			// Each corner keeps its own nudge, so switching corners does not lose the
			// adjustment made to the one you were on.
			const int offsetCorner = display::AnchorIndex();

			changed |= NudgeableSlider("Offset X", &display::offsetX[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);
			HelpMarker("Nudge from the corner, in screen pixels. Positive is always rightwards, whichever corner is anchored. Each corner remembers its own pair.");

			changed |= NudgeableSlider("Offset Y", &display::offsetY[offsetCorner], -600.0F, 600.0F, "%.0f px", 1.0F);
			HelpMarker("Nudge from the corner, in screen pixels. Positive is always downwards, whichever corner is anchored. Each corner remembers its own pair.");

			ImGuiMCP::TextDisabled("Editing the %s offset.", kAnchorNames[offsetCorner]);

			// The upper end is whatever keeps the minimap within a quarter of the screen, so
			// the slider cannot ask for a size the plugin will refuse to apply.
			auto* sized = DEM::Minimap::GetSingleton();
			const float maxScale = sized ? sized->GetMaxScale() : display::kScaleSliderMax;

			display::scale = std::clamp(display::scale, display::kScaleSliderMin, maxScale);

			changed |= NudgeableSlider("Scale", &display::scale, display::kScaleSliderMin, maxScale, "%.2f", 0.01F);
			HelpMarker("Size of the minimap. 1.00 is the size the artwork was drawn at. The top of the range is capped so the minimap stays within a quarter of the screen.");

			ImGuiMCP::TextDisabled("Largest allowed: %.2f (a quarter of the screen)", maxScale);

			if (changed)
			{
				OnMainThread([]() {
					if (auto* target = DEM::Minimap::GetSingleton())
					{
						target->ApplyDisplaySettings();
					}
				});
			}

			int shape = static_cast<int>(display::shape);
			if (ImGuiMCP::Combo("Shape", &shape, kShapeNames, kShapeCount))
			{
				display::shape = static_cast<std::uint32_t>(shape);

				OnMainThread([]() {
					if (auto* minimap = DEM::Minimap::GetSingleton())
					{
						minimap->ApplyShapeSetting();
					}
				});
			}
			HelpMarker("Whether the minimap is drawn as a square or as a circle.");

			auto* minimap = DEM::Minimap::GetSingleton();

			if (minimap && minimap->IsReady())
			{
				bool shown = minimap->IsShown();
				if (ImGuiMCP::Checkbox("Show minimap", &shown))
				{
					// Show()/Hide() also persist bShowOnGameStart, exactly as pressing the
					// hide key in game does, so this doubles as the on-start setting.
					OnMainThread([shown]() {
						if (auto* target = DEM::Minimap::GetSingleton())
						{
							shown ? target->Show() : target->Hide();
						}
					});
				}
				HelpMarker("Hides or shows the minimap right now, and remembers the choice for the next time you play.");
			}
			else
			{
				if (ImGuiMCP::Checkbox("Show minimap on game start", &display::showOnGameStart))
				{
					logger::debug("Show on game start set to {}", display::showOnGameStart);
				}
				HelpMarker("The minimap has not been built yet, so this only sets what happens once it is.");
			}
		}

		void RenderZoomSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Map zoom");

			// The key, and the two levels it alternates between, do not need the minimap to
			// exist - only the live slider and "Set to current" do, since those talk to the
			// camera. Keeping the key controls out from behind that gate is what makes it
			// possible to bind or type the zoom key before the minimap has loaded.
			KeyBindRow("Zoom toggle key", &controls::zoomToggleKeyCode, BindTarget::kZoom, "Bind##zoom");
			HelpMarker("Press this key to jump between the two zoom levels below, instead of holding the control key and scrolling. 0 disables it.");

			if (controls::zoomToggleKeyCode == 0)
			{
				ImGuiMCP::TextDisabled("No zoom key set.");
			}

			ImGuiMCP::Spacing();

			auto* minimap = DEM::Minimap::GetSingleton();
			const bool ready = minimap && minimap->IsReady();

			if (NudgeableSlider("Default zoom", &controls::zoomDefault, 0.0F, 1.0F, "%.3f", 0.01F))
			{
				logger::debug("Default zoom set to {:.3f}", controls::zoomDefault);
			}
			if (ready)
			{
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Set to current##default"))
				{
					controls::zoomDefault = minimap->GetMapZoom();
					logger::debug("Default zoom set to current camera zoom {:.3f}", controls::zoomDefault);
				}
			}

			if (NudgeableSlider("Zoomed in", &controls::zoomZoomedIn, 0.0F, 1.0F, "%.3f", 0.01F))
			{
				logger::debug("Zoomed-in zoom set to {:.3f}", controls::zoomZoomedIn);
			}
			if (ready)
			{
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Set to current##zoomedin"))
				{
					controls::zoomZoomedIn = minimap->GetMapZoom();
					logger::debug("Zoomed-in zoom set to current camera zoom {:.3f}", controls::zoomZoomedIn);
				}
			}
			HelpMarker("The zoom toggle key alternates between these two. Zoom the map where you want it, then press \"Set to current\" to store that level rather than typing a number in units the game does not document.");

			if (!ready)
			{
				ImGuiMCP::TextDisabled("Zoom the map in game and use \"Set to current\" once the minimap is running - "
									   "the numbers above are in the camera's own units, which are not documented.");

				return;
			}

			ImGuiMCP::Spacing();

			// Read back from the camera every frame rather than keeping our own copy, so the
			// slider shows where the zoom actually ended up after the game clamped it.
			float live = minimap->GetMapZoom();
			if (NudgeableSlider("Live zoom", &live, 0.0F, 1.0F, "%.3f", 0.01F))
			{
				logger::debug("Live zoom set to {:.3f}", live);

				OnMainThread([live]() {
					if (auto* target = DEM::Minimap::GetSingleton())
					{
						target->SetMapZoom(live);
					}
				});
			}
			HelpMarker("How far the minimap is zoomed in, right now. The game applies its own limits, so the value can settle somewhere other than where you left it.");
		}

		void RenderControlsSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Controls");

			KeyBindRow("Hide key", &controls::hideKeyCode, BindTarget::kHide, "Bind##hide");
			HelpMarker("Press this key to show or hide the minimap immediately. 0 disables it.");

			if (controls::hideKeyCode == 0)
			{
				ImGuiMCP::TextDisabled("No hide key set.");
			}

			ImGuiMCP::Spacing();

			if (ImGuiMCP::Checkbox("Rotate with the player", &controls::followPlayerCameraRotation))
			{
				logger::debug("Rotate with player camera set to {}", controls::followPlayerCameraRotation);
			}
			HelpMarker("On: the minimap turns to face where the player is looking. Off: north is always up, like the local map.");
		}

		void RenderDebugSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Debug");

			int logLevel = static_cast<int>(debug::logLevel);
			if (ImGuiMCP::Combo("Log level", &logLevel, kLogLevelNames, kLogLevelCount))
			{
				debug::logLevel = static_cast<logger::level>(logLevel);
				logger::set_level(debug::logLevel, debug::logLevel);
				logger::debug("Log level set to {}", kLogLevelNames[logLevel]);
			}
			HelpMarker("How much detail the plugin writes to its log. Leave this on Info unless you are chasing a problem.");
		}

		void RenderButtons()
		{
			ImGuiMCP::SeparatorText("");

			// Save and Reload drive the game's own INISettingCollection, whose handle and
			// Setting objects the main thread also touches through Minimap::Show()/Hide().
			// Queue them rather than racing it from the render thread.
			if (ImGuiMCP::Button("Save"))
			{
				statusMessage = "Saving...";
				OnMainThread([]() {
					statusMessage = settings::Save() ? "Settings saved." : "Could not write the INI. See the log for why.";
				});
			}
			HelpMarker("Writes every setting on this page to the plugin's INI so it survives a restart.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
				statusMessage = "Reloading...";
				OnMainThread([]() {
					if (settings::Reload())
					{
						ApplyLiveSettings();

						statusMessage = "Settings reloaded from the INI.";
					}
					else
					{
						statusMessage = "Could not read the INI. See the log for why.";
					}
				});
			}
			HelpMarker("Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() {
					settings::RestoreDefaults();
					ApplyLiveSettings();

					logger::debug("Restored default settings");
				});

				statusMessage = "Defaults restored. Press Save to keep them.";
			}
			HelpMarker("Puts every setting back to the value it has on a fresh install. Nothing is written until you press Save.");

			if (!statusMessage.empty())
			{
				ImGuiMCP::TextWrapped("%s", statusMessage.c_str());
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("SKSE Menu Framework is not installed; settings will be read from the INI only");

			return;
		}

		if (!HasRequiredExports())
		{
			logger::warn("The installed SKSE Menu Framework is older than this plugin's settings "
						 "menu needs. Update it to version 3 or newer to configure the minimap in game.");

			return;
		}

		// Only needed for the "Bind" buttons; without it both keys can still be typed in.
		if (GetMenuFrameworkFunction<void*>("RegisterInpoutEvent"))
		{
			inputHook = SKSEMenuFramework::AddInputEvent(OnInputEvent);
		}
		else
		{
			logger::info("SKSE Menu Framework does not export \"RegisterInpoutEvent\"; "
						 "both keys can still be set by typing their scan codes");
		}

		SKSEMenuFramework::SetSection("Dragon's Eye Minimap");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);

		logger::info("Registered the settings page with SKSE Menu Framework");
	}

	void ApplyLiveSettings()
	{
		logger::set_level(settings::debug::logLevel, settings::debug::logLevel);

		OnMainThread([]() {
			if (auto* minimap = DEM::Minimap::GetSingleton())
			{
				minimap->ApplyDisplaySettings();
				minimap->ApplyShapeSetting();
			}
		});
	}

	void __stdcall SettingsPanel::Render()
	{
		ImGuiMCP::TextWrapped("Changes apply as soon as you make them. Press Save to keep them for the next time you play.");
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);

		RenderDisplaySection();
		ImGuiMCP::Spacing();

		RenderZoomSection();
		ImGuiMCP::Spacing();

		RenderControlsSection();
		ImGuiMCP::Spacing();

		RenderDebugSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
