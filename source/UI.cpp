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
		constexpr std::size_t kTipBufferSize = 256;

		// ImGui edits text through a fixed char buffer, so the control tips live here while the
		// panel is open and are copied back into settings:: as soon as they are edited.
		char hideTipBuffer[kTipBufferSize]{};
		char moveTipBuffer[kTipBufferSize]{};
		char zoomTipBuffer[kTipBufferSize]{};

		std::string statusMessage;

		// While set, the next key pressed becomes the minimap's hide key instead of doing
		// whatever it normally does.
		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		std::atomic<bool> awaitingKeyBind{ false };
		SKSEMenuFramework::Model::InputEvent* inputHook = nullptr;

		constexpr const char* kShapeNames[] = { "Squared", "Round" };
		constexpr int kShapeCount = 2;

		constexpr const char* kAnchorNames[] = { "Top left", "Top right", "Bottom left", "Bottom right" };
		constexpr int kAnchorCount = 4;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		void CopyToBuffer(char (&a_buffer)[kTipBufferSize], const std::string& a_value)
		{
			const std::size_t length = a_value.size() < kTipBufferSize - 1 ? a_value.size() : kTipBufferSize - 1;

			std::memcpy(a_buffer, a_value.data(), length);
			a_buffer[length] = '\0';
		}

		void RefreshTipBuffers()
		{
			CopyToBuffer(hideTipBuffer, settings::display::controlHideTip);
			CopyToBuffer(moveTipBuffer, settings::display::controlMoveTip);
			CopyToBuffer(zoomTipBuffer, settings::display::controlZoomTip);
		}

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
				"igInputText",
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
		// flag, so there is nothing here that needs the main thread.
		bool __stdcall OnInputEvent(RE::InputEvent* a_event)
		{
			if (!awaitingKeyBind.load())
			{
				return false;
			}

			auto* buttonEvent = a_event ? a_event->AsButtonEvent() : nullptr;
			if (!buttonEvent || !buttonEvent->IsDown())
			{
				return false;
			}

			// Keyboard only, to match what Controls.cpp will compare against. Storing a mouse
			// IDCode here would bind a code that also matches a low keyboard scan code.
			if (buttonEvent->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
			{
				return false;
			}

			settings::controls::hideKeyCode = static_cast<std::int32_t>(buttonEvent->GetIDCode());
			awaitingKeyBind.store(false);

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
			HelpMarker("Which screen corner the minimap sits in. With the edge margin at 0 it tucks right into that corner.");

			changed |= NudgeableSlider("Edge margin", &display::edgeMargin, 0.0F, 200.0F, "%.0f px", 1.0F);
			HelpMarker("How far in from the corner's two edges the minimap sits. 0 puts it flush against them.");

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
					// Show()/Hide() also persist bShowOnGameStart, exactly as tapping the
					// control key in game does, so this doubles as the on-start setting.
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
				ImGuiMCP::Checkbox("Show minimap on game start", &display::showOnGameStart);
				HelpMarker("The minimap has not been built yet, so this only sets what happens once it is.");
			}
		}

		void RenderZoomSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Map zoom");

			auto* minimap = DEM::Minimap::GetSingleton();

			if (!minimap || !minimap->IsReady())
			{
				ImGuiMCP::TextDisabled("Available once the minimap is running.");

				return;
			}

			// Read back from the camera every frame rather than keeping our own copy, so the
			// slider shows where the zoom actually ended up after the game clamped it.
			float live = minimap->GetMapZoom();
			if (NudgeableSlider("Zoom", &live, 0.0F, 1.0F, "%.3f", 0.01F))
			{
				OnMainThread([live]() {
					if (auto* target = DEM::Minimap::GetSingleton())
					{
						target->SetMapZoom(live);
					}
				});
			}
			HelpMarker("How far the minimap is zoomed in, right now. The game applies its own limits, so the value can settle somewhere other than where you left it.");

			ImGuiMCP::TextDisabled("Camera reports %.4f", live);

			ImGuiMCP::Spacing();

			NudgeableSlider("Preset 1", &controls::zoomPreset1, 0.0F, 1.0F, "%.3f", 0.01F);
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Set to current##z1"))
			{
				controls::zoomPreset1 = minimap->GetMapZoom();
			}

			NudgeableSlider("Preset 2", &controls::zoomPreset2, 0.0F, 1.0F, "%.3f", 0.01F);
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Set to current##z2"))
			{
				controls::zoomPreset2 = minimap->GetMapZoom();
			}
			HelpMarker("Zoom the map where you want it, then press \"Set to current\" to store that level as a preset.");

			int zoomKey = controls::zoomToggleKeyCode;
			if (ImGuiMCP::InputInt("Zoom toggle key", &zoomKey))
			{
				controls::zoomToggleKeyCode = zoomKey < 0 ? 0 : zoomKey;
			}
			HelpMarker("Tapping this key jumps between the two presets, instead of holding the control key and scrolling. 0 disables it.");

			if (controls::zoomToggleKeyCode == 0)
			{
				ImGuiMCP::TextDisabled("No zoom key set.");
			}
		}

		void RenderControlsSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Controls");

			int keyCode = controls::hideKeyCode;
			if (ImGuiMCP::InputInt("Hide key", &keyCode))
			{
				controls::hideKeyCode = keyCode < 0 ? 0 : keyCode;
			}
			HelpMarker("DirectInput scan code of a key that shows or hides the minimap the moment it is pressed. "
					   "0 disables it. This is separate from the game's Local Map key, which keeps its own "
					   "tap-to-hide and hold-to-control behaviour either way.");

			ImGuiMCP::SameLine();

			if (awaitingKeyBind.load())
			{
				if (ImGuiMCP::Button("Press a key... (cancel)"))
				{
					awaitingKeyBind.store(false);
				}
			}
			else if (ImGuiMCP::Button("Bind"))
			{
				awaitingKeyBind.store(true);
			}

			if (controls::hideKeyCode == 0)
			{
				ImGuiMCP::TextDisabled("No hide key set; the game's Local Map key still works.");
			}

			ImGuiMCP::Spacing();

			ImGuiMCP::Checkbox("Rotate with the player", &controls::followPlayerCameraRotation);
			HelpMarker("On: the minimap turns to face where the player is looking. Off: north is always up, like the local map.");

			ImGuiMCP::SliderFloat("Hold to control (s)", &controls::holdDownToControlSecs, 0.0F, 2.0F, "%.2f");
			HelpMarker("How long the control key has to be held before it starts panning and zooming the minimap instead of hiding it.");

			ImGuiMCP::SliderFloat("Hide controls after (s)", &controls::delayToHideControlsSecs, 0.0F, 10.0F, "%.2f");
			HelpMarker("How long the control tips stay on screen once you stop controlling the minimap.");
		}

		void RenderTipsSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Control tips");

			ImGuiMCP::TextDisabled("The prompts drawn next to the minimap. Handy for translating them.");

			// Assigning a std::string frees its old buffer. The main thread hands these to
			// Scaleform as .c_str() while drawing the control tips, so writing them from the
			// render thread can pull the memory out from under an Invoke that is in flight.
			// Copy the edited text on the main thread instead.
			const auto setTip = [](std::string* a_target, const char* a_text) {
				OnMainThread([a_target, text = std::string(a_text)]() { *a_target = text; });
			};

			if (ImGuiMCP::InputText("Hide tip", hideTipBuffer, kTipBufferSize))
			{
				setTip(&display::controlHideTip, hideTipBuffer);
			}

			if (ImGuiMCP::InputText("Move tip", moveTipBuffer, kTipBufferSize))
			{
				setTip(&display::controlMoveTip, moveTipBuffer);
			}

			if (ImGuiMCP::InputText("Zoom tip", zoomTipBuffer, kTipBufferSize))
			{
				setTip(&display::controlZoomTip, zoomTipBuffer);
			}
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
						RefreshTipBuffers();
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
					RefreshTipBuffers();
					ApplyLiveSettings();
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

		RefreshTipBuffers();

		// Only needed for the "Bind" button; without it the key can still be typed in.
		if (GetMenuFrameworkFunction<void*>("RegisterInpoutEvent"))
		{
			inputHook = SKSEMenuFramework::AddInputEvent(OnInputEvent);
		}
		else
		{
			logger::info("SKSE Menu Framework does not export \"RegisterInpoutEvent\"; "
						 "the hide key can still be set by typing its scan code");
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

		RenderTipsSection();
		ImGuiMCP::Spacing();

		RenderDebugSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
