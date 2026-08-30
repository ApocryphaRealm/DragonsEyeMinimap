#include "Hooks.h"
#include "MiniMap.h"
#include "Settings.h"

#include "utils/Logger.h"

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg);

const SKSE::LoadInterface* skse;

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	// Workaround for static initialization order bug of CommonLibSSE-NG
	REL::Module::reset();

	skse = a_skse;

	const SKSE::PluginDeclaration* plugin = SKSE::PluginDeclaration::GetSingleton();

	if (!logger::init(plugin->GetName()))
	{
		return false;
	}

	logger::info("Loading {} {}...", plugin->GetName(), plugin->GetVersion());

	SKSE::Init(a_skse);
	logger::debug("SKSE core APIs initialized");

	logger::debug("Loading settings from {}.ini", plugin->GetName());
	settings::Init(std::string(plugin->GetName()) + ".ini");

	logger::set_level(settings::debug::logLevel, settings::debug::logLevel);
	logger::debug("Settings loaded; log level set to {}", static_cast<std::uint32_t>(settings::debug::logLevel));
	logger::describe_level(std::string(plugin->GetName()) + ".ini");

	if (!SKSE::GetMessagingInterface()->RegisterListener("SKSE", SKSEMessageListener))
	{
		logger::error("Could not register the SKSE message listener; plugin load aborted");
		return false;
	}

	logger::debug("SKSE message listener registered");

	logger::debug("Installing hooks...");
	hooks::Install();
	logger::debug("Hooks installed");

	// REMOVED for 1.5.9 crash isolation. This used to call
	// diagnostics::RecordHooksInstalled(...) with four REL::Relocation .address() values, which
	// resolved four relocated addresses during SKSEPluginLoad - earlier than anything else this
	// mod does. Stubbing the function would NOT have removed that behaviour, because arguments
	// are evaluated regardless of what the body does, so the whole call is gone instead.
	// See include/Diagnostics.h and the 2026-08-27 crash bug report.

	logger::set_level(logger::level::info, logger::level::info);
	logger::info("Succesfully loaded!");

	logger::set_level(settings::debug::logLevel, settings::debug::logLevel);
	logger::debug("Log level restored to configured level {}", static_cast<std::uint32_t>(settings::debug::logLevel));

	return true;
}

// ------------------------------------------------------------------------------------------
// Public C export (1.5.9) for the pointer add-on (DragonsEyePointers): where the minimap
// artwork currently sits, in STAGE pixels (HUD movie stage; the caller scales by its own
// screen size / stage size), which corner it is anchored to, and whether the minimap is shown.
// Returns false until the minimap has been positioned at least once. Safe from any thread.
// ------------------------------------------------------------------------------------------
extern "C" DLLEXPORT bool DEM_GetMinimapStageRect(float* a_left, float* a_top, float* a_right, float* a_bottom,
												  float* a_stageWidth, float* a_stageHeight, int* a_corner, bool* a_shown)
{
	DEM::Minimap::StageRect r;
	int corner = 0;
	float sw = 0.0F, sh = 0.0F;
	{
		std::scoped_lock lock(DEM::Minimap::stageRectLock);
		r = DEM::Minimap::stageRect;
		corner = DEM::Minimap::stageRectCorner;
		sw = DEM::Minimap::stageRectStageW;
		sh = DEM::Minimap::stageRectStageH;
	}
	if (a_left) { *a_left = r.left; }
	if (a_top) { *a_top = r.top; }
	if (a_right) { *a_right = r.right; }
	if (a_bottom) { *a_bottom = r.bottom; }
	if (a_stageWidth) { *a_stageWidth = sw; }
	if (a_stageHeight) { *a_stageHeight = sh; }
	if (a_corner) { *a_corner = corner; }
	if (a_shown)
	{
		const DEM::Minimap* mm = DEM::Minimap::GetSingleton();
		*a_shown = mm && mm->IsShown();
	}
	return r.valid;
}
