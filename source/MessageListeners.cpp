#include "Settings.h"

#include "Minimap.h"

#include "IUI/API.h"
#include "LMU/API.h"

#include "UI.h"

#include "IUI/GFxLoggers.h"

extern const SKSE::LoadInterface* skse;

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg);
void LocalMapUpgradeMessageListener(SKSE::MessagingInterface::Message* a_msg);

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
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
			auto lmuInfo = skse->GetPluginInfo("LocalMapUpgrade");
			logger::debug("Local Map Upgrade found: version {:#010x} (require >= {:#010x})", lmuInfo->version, 0x03010000u);
			if (lmuInfo->version >= 0x03010000)
			{
				logger::info("Successfully registered for Local Map Upgrade messages!");
			}
			else
			{
				logger::error("Local Map Upgrade version {:#010x} is older than the required {:#010x}", lmuInfo->version, 0x03010000u);
				SKSE::stl::report_and_fail
				(
					std::format
					(
						"\n\n"
						"\"Local Map Upgrade\" 3.1.0 or newer required.\n\n"
						"Please, download it from:\n"
						"www.nexusmods.com/skyrimspecialedition/mods/129756"
					)
				);
			}
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
