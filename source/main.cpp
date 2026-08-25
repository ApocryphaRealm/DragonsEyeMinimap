#include "Hooks.h"
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

	// This build is a diagnostic sent to a specific reporter, not a release. It carries the
	// same version number as 1.3.0, so say so plainly at the top of the log - otherwise a log
	// from it is indistinguishable from a release log.
	logger::info("*** AE TEST BUILD - diagnostic only, not a release. Extra visibility logging is enabled. ***");

	if (!SKSE::GetMessagingInterface()->RegisterListener("SKSE", SKSEMessageListener))
	{
		logger::error("Could not register the SKSE message listener; plugin load aborted");
		return false;
	}

	logger::debug("SKSE message listener registered");

	logger::debug("Installing hooks...");
	hooks::Install();
	logger::debug("Hooks installed");

	logger::set_level(logger::level::info, logger::level::info);
	logger::info("Succesfully loaded!");

	logger::set_level(settings::debug::logLevel, settings::debug::logLevel);
	logger::debug("Log level restored to configured level {}", static_cast<std::uint32_t>(settings::debug::logLevel));

	return true;
}