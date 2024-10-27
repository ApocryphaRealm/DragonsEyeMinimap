#include "Settings.h"

#include "Minimap.h"

#include "IUI/API.h"
#include "LMU/API.h"

#include "IUI/GFxLoggers.h"

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg);
void LocalMapUpgradeMessageListener(SKSE::MessagingInterface::Message* a_msg);

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	// If all plugins have been loaded
	if (a_msg->type == SKSE::MessagingInterface::kPostLoad) 
	{
		if (SKSE::GetMessagingInterface()->RegisterListener("InfinityUI", InfinityUIMessageListener)) 
		{
			logger::info("Successfully registered for Infinity UI messages!");
		}
		else
		{
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
			logger::info("Successfully registered for Local Map Upgrade messages!");
		}
		else
		{
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
		return;
	}

	if (auto message = API::TranslateAs<API::Message>(a_msg)) 
	{
		std::string_view movieUrl = message->movie->GetMovieDef()->GetFileURL();

		if (movieUrl.find("HUDMenu") == std::string::npos)
		{
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
				break;
			}
		case API::Message::Type::kPostPatchInstance:
			{
				if (auto msg = API::TranslateAs<API::PostPatchInstanceMessage>(a_msg))
				{
					std::string pathToNew = msg->newInstance.ToString().c_str();

					if (pathToNew == DEM::Minimap::path)
					{
						DEM::Minimap::InitSingleton(msg->newInstance);
					}
				}
				break;
			}
		case API::Message::Type::kFinishLoadInstances:
			{
				if (auto msg = API::TranslateAs<API::FinishLoadInstancesMessage>(a_msg))
				{
					if (auto minimap = DEM::Minimap::GetSingleton())
					{
						auto hudMenu = static_cast<RE::HUDMenu*>(msg->menu);

						hudMenu->GetRuntimeData().objects.push_back(minimap);
					}
					else
					{
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
			break;
		}
	}
}

void LocalMapUpgradeMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	using namespace LMU;

	if (!a_msg || std::string_view(a_msg->sender) != "LocalMapUpgrade")
	{
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
				break;
			}
		case API::Message::Type::kPostCreateMarkersHook:
			{
				if (auto msg = API::TranslateAs<API::PostCreateMarkersHookMessage>(a_msg))
				{
					DEM::Minimap::PostCreateMarkers = msg->PostCreateMarkers;
					logger::debug("Post create markers hooked");
				}
				break;
			}
		default:
			break;
		}
	}
}
