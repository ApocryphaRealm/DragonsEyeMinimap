#include "Settings.h"

#include "Minimap.h"

#include "IUI/API.h"
#include "LMU/API.h"

#include "UI.h"

#include "IUI/GFxLoggers.h"

extern const SKSE::LoadInterface* skse;

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg);
void LocalMapUpgradeMessageListener(SKSE::MessagingInterface::Message* a_msg);
void VerifyLocalMapUpgradeCapabilities();

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{

	// By kDataLoaded every plugin has had its chance to send its post-load messages, so if
	// Local Map Upgrade's pixel-shader pointers have not arrived by now they are not coming.
	if (a_msg->type == SKSE::MessagingInterface::kDataLoaded)
	{
		VerifyLocalMapUpgradeCapabilities();
	}

	// Once every plugin has finished its own post-load work, SKSE Menu Framework is
	// certain to be in the process, so its module can be looked up and the settings
	// page registered.
	if (a_msg->type == SKSE::MessagingInterface::kPostPostLoad)
	{
		logger::debug("kPostPostLoad received; registering settings page with SKSE Menu Framework");
		UI::Register();
	}

	// If all plugins have been loaded
	if (a_msg->type == SKSE::MessagingInterface::kPostLoad)
	{
		logger::debug("kPostLoad received; registering for Infinity UI and Local Map Upgrade messages");

		if (SKSE::GetMessagingInterface()->RegisterListener("InfinityUI", InfinityUIMessageListener))
		{
			logger::info("Successfully registered for Infinity UI messages!");
		}
		else
		{
			logger::error("RegisterListener(\"InfinityUI\") failed; plugin not detected");
			SKSE::stl::report_and_fail
			(
				std::format
				(
					"\n\n"
					"\"Infinity UI\" installation not detected.\n\n"
					"Please, download it from:\n"
					"www.nexusmods.com/skyrimspecialedition/mods/74483"
				)
			);
		}

		if (SKSE::GetMessagingInterface()->RegisterListener("LocalMapUpgrade", LocalMapUpgradeMessageListener))
		{
			// Deliberately NOT gated on Local Map Upgrade's reported version number. What this
			// plugin actually needs is the pixel-shader function pointers Local Map Upgrade sends,
			// so that is what gets checked - at kDataLoaded, once it has had its chance to send
			// them (see VerifyLocalMapUpgradeCapabilities below). A version gate was only ever a
			// proxy for the same question, and it broke whenever the number moved: it hard-failed
			// the game against a fork numbered from 1.0.0 even when that fork's API was a strict
			// superset. A capability check is version-agnostic, so it survives renumbering, forks,
			// and anything else that provides the same API.
			//
			// GetPluginInfo can hand back null even directly after a successful RegisterListener,
			// so the version is read only when there is something to read - CLAUDE.md rule 14,
			// debug logging and null checks together.
			if (auto lmuInfo = skse->GetPluginInfo("LocalMapUpgrade"))
			{
				logger::debug("Local Map Upgrade found: version {:#010x} (informational only - not gated on)", lmuInfo->version);
			}
			else
			{
				logger::warn("Local Map Upgrade registered but GetPluginInfo returned null; relying on the capability check alone");
			}

			logger::info("Successfully registered for Local Map Upgrade messages!");
		}
		else
		{
			logger::error("RegisterListener(\"LocalMapUpgrade\") failed; plugin not detected");
			SKSE::stl::report_and_fail
			(
				std::format
				(
					"\n\n"
					"\"Local Map Upgrade\" installation not detected.\n\n"
					"Please, download it from:\n"
					"www.nexusmods.com/skyrimspecialedition/mods/129756"
				)
			);
		}
	}
}

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	using namespace IUI;

	if (!a_msg || std::string_view(a_msg->sender) != "InfinityUI")
	{
		logger::warn("InfinityUIMessageListener invoked with a null message or unexpected sender");
		return;
	}

	if (auto message = API::TranslateAs<API::Message>(a_msg))
	{
		std::string_view movieUrl = message->movie->GetMovieDef()->GetFileURL();

		if (movieUrl.find("HUDMenu") == std::string::npos)
		{
			logger::debug("Ignoring Infinity UI message (type {}) for \"{}\"; not the HUD movie", static_cast<std::uint32_t>(a_msg->type), movieUrl);
			return;
		}

		switch (a_msg->type)
		{
		case API::Message::Type::kStartLoadInstances:
			{
				logger::info("Started loading patches");
				break;
			}
		case API::Message::Type::kPreReplaceInstance:
			{
				if (auto msg = API::TranslateAs<API::PreReplaceInstanceMessage>(a_msg))
				{
					logger::debug("kPreReplaceInstance: original instance \"{}\" about to be replaced", msg->originalInstance.ToString().c_str());
				}
				break;
			}
		case API::Message::Type::kPostPatchInstance:
			{
				if (auto msg = API::TranslateAs<API::PostPatchInstanceMessage>(a_msg))
				{
					std::string pathToNew = msg->newInstance.ToString().c_str();
					logger::debug("kPostPatchInstance: new instance patched at \"{}\"", pathToNew);

					if (pathToNew == DEM::Minimap::path)
					{
						logger::debug("Patched instance matches minimap path \"{}\"; initializing minimap singleton", DEM::Minimap::path);
						DEM::Minimap::InitSingleton(msg->newInstance);
					}
				}
				break;
			}
		case API::Message::Type::kAbortPatchInstance:
			{
				if (auto msg = API::TranslateAs<API::AbortPatchInstanceMessage>(a_msg))
				{
					logger::debug("kAbortPatchInstance: patch aborted for instance \"{}\"", msg->originalValue.ToString().c_str());
				}
				break;
			}
		case API::Message::Type::kFinishLoadInstances:
			{
				if (auto msg = API::TranslateAs<API::FinishLoadInstancesMessage>(a_msg))
				{
					logger::debug("kFinishLoadInstances: {} instance(s) patched", msg->loadedCount);

					if (auto minimap = DEM::Minimap::GetSingleton())
					{
						logger::debug("Minimap singleton found; adding it to the HUD menu's runtime objects");

						auto hudMenu = static_cast<RE::HUDMenu*>(msg->menu);

						hudMenu->GetRuntimeData().objects.push_back(minimap);
					}
					else
					{
						logger::error("Minimap singleton not found at end of HUD patch load; \"{}\" was never patched", DEM::Minimap::path);
						SKSE::stl::report_and_fail
						(
							std::format
							(
								"\n\n"
								"\"Data\\Interface\\InfinityUI\\HUDMenu\\HUDMovieBaseInstance\\Minimap.swf\" not found.\n"
								"Please, check your installation files."
							)
						);
					}
				}
				logger::info("Finished loading HUD patches");
				break;
			}
		case API::Message::Type::kPostInitExtensions:
			{
				if (auto msg = API::TranslateAs<API::PostInitExtensionsMessage>(a_msg))
				{
					logger::debug("Extensions initialization finished");
				}
				break;
			}
		default:
			logger::debug("Unhandled Infinity UI message type {} for \"{}\"", static_cast<std::uint32_t>(a_msg->type), movieUrl);
			break;
		}
	}
}

void LocalMapUpgradeMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	using namespace LMU;

	if (!a_msg || std::string_view(a_msg->sender) != "LocalMapUpgrade")
	{
		logger::warn("LocalMapUpgradeMessageListener invoked with a null message or unexpected sender");
		return;
	}

	if (auto message = API::TranslateAs<API::Message>(a_msg))
	{
		switch (a_msg->type)
		{
		case API::Message::Type::kPixelShaderPropertiesHook:
			{
				if (auto msg = API::TranslateAs<API::PixelShaderPropertiesHookMessage>(a_msg))
				{
					DEM::Minimap::SetPixelShaderProperties = msg->SetPixelShaderProperties;
					DEM::Minimap::GetPixelShaderProperties = msg->GetPixelShaderProperties;
					logger::debug("Pixel shaders properties hooked");
				}
				else
				{
					logger::warn("kPixelShaderPropertiesHook payload size mismatch; possible Local Map Upgrade version skew");
				}
				break;
			}
		default:
			logger::debug("Unhandled Local Map Upgrade message type {}", static_cast<std::uint32_t>(a_msg->type));
			break;
		}
	}
}


// Replaces the old "Local Map Upgrade >= 3.1.0" version gate. The thing this plugin depends
// on is the pair of pixel-shader function pointers Local Map Upgrade hands over in its
// kPixelShaderPropertiesHook message - they drive the minimap's shape and style, so without
// them the minimap cannot work correctly. Checking for the pointers directly means any build
// that actually provides the API is accepted, whatever version number it reports.
//
// This stays a hard failure rather than a silent degrade: Local Map Upgrade is a hard
// requirement (a missing registration already fails above), and a minimap that quietly could
// not change shape would be a worse experience than a message saying what to install.
void VerifyLocalMapUpgradeCapabilities()
{
	const bool hasSetter = DEM::Minimap::SetPixelShaderProperties != nullptr;
	const bool hasGetter = DEM::Minimap::GetPixelShaderProperties != nullptr;

	logger::debug("Local Map Upgrade capability check: SetPixelShaderProperties={} GetPixelShaderProperties={}", hasSetter, hasGetter);

	if (hasSetter && hasGetter)
	{
		logger::info("Local Map Upgrade pixel-shader hooks available");

		return;
	}

	logger::error("Local Map Upgrade did not provide its pixel-shader hooks (setter={} getter={}); it is present but not compatible", hasSetter, hasGetter);
	SKSE::stl::report_and_fail
	(
		std::format
		(
			"\n\n"
			"\"Local Map Upgrade\" is installed but did not provide the interface\n"
			"this mod needs, so it is an incompatible build.\n\n"
			"Please install a current version from:\n"
			"www.nexusmods.com/skyrimspecialedition/mods/129756"
		)
	);
}