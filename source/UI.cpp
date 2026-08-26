#include "UI.h"

#include "SKSEMenuFramework.h"

#include "MiniMap.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <chrono>
#include <vector>

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
				"igPopItemWidth",
				// Toggle() - the on/off switch every boolean setting now renders as
				// instead of a tick-box (utils/Toggle.h, CLAUDE.md rule 32).
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
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

		// Heartbeat proving the settings panel is actually on screen.
		//
		// The framework offers no "menu closed" callback, but Render() only runs while the menu
		// is open, so a recent draw IS the open signal. Written on the render thread, read on the
		// input thread, hence atomic. Milliseconds since an arbitrary epoch; only differences are
		// ever used.
		std::atomic<std::int64_t> panelLastDrawnMs{ 0 };

		std::int64_t NowMs()
		{
			return std::chrono::duration_cast<std::chrono::milliseconds>(
					   std::chrono::steady_clock::now().time_since_epoch())
				.count();
		}

		void MarkPanelDrawn() { panelLastDrawnMs.store(NowMs(), std::memory_order_relaxed); }

		// 500 ms is generously longer than any frame the game will draw while a menu is open, and
		// far shorter than a human can close a menu and press an unrelated key.
		bool PanelIsOpen()
		{
			const std::int64_t last = panelLastDrawnMs.load(std::memory_order_relaxed);
			return last != 0 && (NowMs() - last) < 500;
		}

		// DirectInput scan codes - the same table GetIDCode() reports and Controls.cpp compares
		// against. Confirmed live 2026-08-26: Escape reported 1, Tab 15, F1 59, K 37, L 38.
		constexpr std::int32_t kScanEscape = 0x01;
		constexpr std::int32_t kScanTab = 0x0F;
		constexpr std::int32_t kScanEnter = 0x1C;
		constexpr std::int32_t kScanSpace = 0x39;
		constexpr std::int32_t kScanUp = 0xC8;
		constexpr std::int32_t kScanLeft = 0xCB;
		constexpr std::int32_t kScanRight = 0xCD;
		constexpr std::int32_t kScanDown = 0xD0;

		// Is the framework currently running ImGui's own keyboard navigation?
		//
		// Asked at runtime rather than assumed, because it decides how many keys are off limits
		// and stock SMF has been observed both ways. There is no "which key is nav" export to
		// query - the whole cimgui surface is exported but nothing reports a nav binding - so the
		// closest honest answer is to read the config flag ImGui itself acts on. Returns false if
		// the export is missing or IO is null, which errs toward allowing a bind rather than
		// blocking one on a guess.
		bool ImGuiKeyboardNavEnabled()
		{
			auto* io = ImGuiMCP::GetIO();

			if (!io)
			{
				logger::trace("ImGuiKeyboardNavEnabled: GetIO() returned null; assuming nav off");
				return false;
			}

			return (io->ConfigFlags & ImGuiMCP::ImGuiConfigFlags_NavEnableKeyboard) != 0;
		}

		// FORWARD COMPATIBILITY WITH THE SMF/ImGui REMAKE.
		//
		// Everything below infers which keys the framework has taken, because stock SMF cannot be
		// asked: it exports the whole cimgui surface but nothing that reports a nav binding. The
		// remake is the fix, and this is the contract it should honour so that this mod - and any
		// other - stops having to infer:
		//
		//     extern "C" __declspec(dllexport)
		//     std::uint32_t SMF_GetReservedKeyCodes(std::int32_t* a_buffer, std::uint32_t a_capacity);
		//
		// Fills a_buffer with the DirectInput scan codes the framework currently consumes (nav,
		// activate, back - whatever its Controls page has them set to) and returns how many it
		// wrote; called with a null buffer it returns the count required. DirectInput scan codes
		// specifically, because that is what RE::ButtonEvent::GetIDCode() reports and therefore
		// what a keybind is stored as - returning ImGuiKey values would make every caller repeat
		// a mapping the framework is far better placed to do once.
		//
		// Once that export exists this returns its answer and the inference below never runs, so
		// a player who rebinds nav on the remake's Controls page immediately gets the right keys
		// refused here, with no change to this mod at all.
		bool FrameworkReportsReservedKeys(std::vector<std::int32_t>& a_out)
		{
			using func_t = std::uint32_t (*)(std::int32_t*, std::uint32_t);
			static const auto func = GetMenuFrameworkFunction<func_t>("SMF_GetReservedKeyCodes");

			if (!func)
			{
				return false;
			}

			const std::uint32_t needed = func(nullptr, 0);

			if (needed == 0)
			{
				a_out.clear();
				return true;
			}

			a_out.resize(needed);
			const std::uint32_t written = func(a_out.data(), needed);
			a_out.resize(written < needed ? written : needed);

			return true;
		}

		// nullptr means the key is fine to bind; anything else is the reason it is not, phrased
		// for the player rather than for the log.
		const char* ReservedKeyReason(std::int32_t a_code)
		{
			// Preferred path once the remake ships - ask the framework instead of inferring.
			std::vector<std::int32_t> reported;

			if (FrameworkReportsReservedKeys(reported))
			{
				if (std::find(reported.begin(), reported.end(), a_code) != reported.end())
				{
					return "the menu framework uses it";
				}

				return nullptr;
			}

			// Always off limits regardless of framework configuration: these are the game's own
			// menu keys, and binding one costs the player that key everywhere.
			if (a_code == kScanTab)
			{
				return "Tab opens the Tween menu";
			}

			if (a_code == kScanEscape)
			{
				return "Escape closes menus";
			}

			// Conditional: only reserved while the framework actually drives ImGui navigation
			// from the keyboard. When it does not, these are ordinary keys and there is no reason
			// to refuse them.
			if (ImGuiKeyboardNavEnabled())
			{
				switch (a_code)
				{
				case kScanUp:
				case kScanDown:
				case kScanLeft:
				case kScanRight:
					return "arrow keys drive menu navigation";
				case kScanEnter:
					return "Enter activates the focused control";
				case kScanSpace:
					return "Space activates the focused control";
				default:
					break;
				}
			}

			return nullptr;
		}

		// Runs on the framework's input thread. Only ever writes the scan code and clears the
		// target, so there is nothing here that needs the main thread.
		bool __stdcall OnInputEvent(RE::InputEvent* a_event)
		{
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

			const BindTarget target = bindTarget.load();

			if (target != BindTarget::kNone)
			{
				// A stale arm must never eat a gameplay keypress. The panel only draws while the
				// menu is open, so if it has not drawn recently the menu is closed and this arm is
				// left over - drop it and let the key through untouched.
				if (!PanelIsOpen())
				{
					logger::debug("Bind still armed with the settings panel closed; disarming and "
								  "letting key code {} through", code);
					bindTarget.store(BindTarget::kNone);
					return false;
				}

				// Refuse keys the menu itself needs. Binding one used to succeed silently and cost
				// the player that key: Tab was captured, stored as the hide key, and then toggled
				// the minimap every time the Tween menu opened (2026-08-26 report). Leave the bind
				// ARMED so a different key can just be pressed, and do NOT swallow - the key still
				// has to do its normal job.
				if (const char* reason = ReservedKeyReason(code))
				{
					logger::info("Refusing to bind key code {} - {}", code, reason);
					statusMessage = std::string{ "That key is reserved (" } + reason +
									"). Press a different key.";
					return false;
				}

				// Refuse a key the other binding already owns. Nothing checked this before, so
				// binding hide to L while zoom was already L was accepted without a word.
				const std::int32_t otherCode = (target == BindTarget::kHide)
												   ? settings::controls::zoomToggleKeyCode
												   : settings::controls::hideKeyCode;
				const char* otherName = (target == BindTarget::kHide) ? "zoom toggle" : "hide";

				if (code != 0 && code == otherCode)
				{
					logger::info("Refusing to bind key code {} - already the {} key", code, otherName);
					statusMessage = std::string{ "That key is already the " } + otherName +
									" key. Press a different key.";
					return false;
				}

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

				statusMessage.clear();
				bindTarget.store(BindTarget::kNone);

				// Swallow it, so binding a key does not also trigger whatever it is bound to.
				return true;
			}

			// Deliberately not swallowing arrow keys for NudgeableSlider here - by request,
			// this mod does not try to claim exclusive input away from gamepad-equivalent menu
			// navigation elsewhere on screen. NudgeableSlider's own nudge is independent of
			// this callback either way (it reads ImGui's own key state through the framework's
			// separate hook), so it is unaffected by removing this.
			return false;
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

				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow))
				{
					nudge -= a_step;
				}
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow))
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
				if (ImGuiMCP::Toggle("Show minimap", &shown))
				{
					// This is the deliberate choice, so it persists (default a_persist = true)
					// and doubles as the on-start setting. The hide KEY deliberately does not -
					// it only changes what is on screen right now.
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
				if (ImGuiMCP::Toggle("Show minimap on game start", &display::showOnGameStart))
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

			if (ImGuiMCP::Toggle("Rotate with the player", &controls::followPlayerCameraRotation))
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
		// Heartbeat for PanelIsOpen(). This function only runs while the framework's menu is
		// actually on screen, so the fact it ran at all is the signal - it is what lets a bind
		// left armed when the menu was closed be detected and dropped instead of eating a
		// keypress during play.
		MarkPanelDrawn();

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
