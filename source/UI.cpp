#include "UI.h"

#include "SKSEMenuFramework.h"

#include "MiniMap.h"
#include "Settings.h"

#include "utils/Logger.h"

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

		constexpr const char* kShapeNames[] = { "Squared", "Round" };
		constexpr int kShapeCount = 2;

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
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabled",
				"igTextWrapped",
				"igSeparatorText",
				"igCheckbox",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igInputText",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igIsItemHovered",
				"igSetTooltip",
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

			changed |= ImGuiMCP::SliderFloat("Horizontal position", &display::positionX, -0.5F, 1.5F, "%.3f");
			HelpMarker("Offset from the left of the screen as a proportion of its width. Ultra-wide setups may want a value below 0 or above 1.");

			changed |= ImGuiMCP::SliderFloat("Vertical position", &display::positionY, -0.5F, 1.5F, "%.3f");
			HelpMarker("Offset from the top of the screen as a proportion of its height.");

			changed |= ImGuiMCP::SliderFloat("Scale", &display::scale, 0.1F, 3.0F, "%.2f");
			HelpMarker("Size of the minimap. 1.00 is the size the artwork was drawn at.");

			if (changed)
			{
				OnMainThread([]() {
					if (auto* minimap = DEM::Minimap::GetSingleton())
					{
						minimap->ApplyDisplaySettings();
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

		void RenderControlsSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Controls");

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

			if (ImGuiMCP::InputText("Hide tip", hideTipBuffer, kTipBufferSize))
			{
				display::controlHideTip = hideTipBuffer;
			}

			if (ImGuiMCP::InputText("Move tip", moveTipBuffer, kTipBufferSize))
			{
				display::controlMoveTip = moveTipBuffer;
			}

			if (ImGuiMCP::InputText("Zoom tip", zoomTipBuffer, kTipBufferSize))
			{
				display::controlZoomTip = zoomTipBuffer;
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

			if (ImGuiMCP::Button("Save"))
			{
				statusMessage = settings::Save() ? "Settings saved." : "Could not write the INI. See the log for why.";
			}
			HelpMarker("Writes every setting on this page to the plugin's INI so it survives a restart.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
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
			}
			HelpMarker("Throws away any change made here since the last save and re-reads the INI from disk. Also picks up edits made to the file by hand.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				settings::RestoreDefaults();
				RefreshTipBuffers();
				ApplyLiveSettings();

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
