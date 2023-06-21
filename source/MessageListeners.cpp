#include "Settings.h"

#include "Minimap.h"

#include "IUI/API.h"

#include "IUI/GFxLoggers.h"

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg);

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
			logger::error("Infinity UI installation not detected. Please, go to ... to get it");
		}
	}
}

template <logger::level logLevel = logger::level::debug>
void LogMapMembers(const IUI::API::PostPatchInstanceMessage* a_msg)
{
	RE::GFxValue value;
	logger::at_level(logLevel, "{}", "Logging HUDMovieBaseInstance...");
	if (!a_msg->movie->GetVariable(&value, "HUDMovieBaseInstance"))
	{
		logger::error("Could not get HUDMovieBaseInstance");
	}
	else
	{
		IUI::GFxMemberLogger<logLevel> memberLogger;

		memberLogger.LogMembersOf(value);
	}

	logger::at_level(logLevel, "{}", "Logging HUDMovieBaseInstance.Minimap...");
	if (!a_msg->movie->GetVariable(&value, DEM::Minimap::path.data()))
	{
		logger::error("Could not get HUDMovieBaseInstance.Minimap");
	}
	else
	{
		IUI::GFxMemberLogger<logLevel> memberLogger;

		memberLogger.LogMembersOf(value);
	}

	logger::at_level(logLevel, "{}", "Logging WorldMap.LocalMapMenu...");
	if (!a_msg->movie->GetVariable(&value, "WorldMap.LocalMapMenu"))
	{
		logger::error("Could not get WorldMap.LocalMapMenu");
	}
	else
	{
		IUI::GFxMemberLogger<logLevel> memberLogger;

		memberLogger.LogMembersOf(value);
	}
}

void InfinityUIMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	if (!a_msg || std::string_view(a_msg->sender) != "InfinityUI") 
	{
		return;
	}

	if (auto message = IUI::API::TranslateAs<IUI::API::Message>(a_msg)) 
	{
		std::string_view movieUrl = message->movie->GetMovieDef()->GetFileURL();

		if (movieUrl.find("HUDMenu") == std::string::npos)
		{
			return;
		}

		switch (a_msg->type)
		{
		case IUI::API::Message::Type::kStartLoadInstances:
			{
				logger::info("Started loading patches");
				break;
			}
		case IUI::API::Message::Type::kPreReplaceInstance:
			{
				break;
			}
		case IUI::API::Message::Type::kPostPatchInstance:
			{
				if (auto msg = IUI::API::TranslateAs<IUI::API::PostPatchInstanceMessage>(a_msg))
				{
					std::string pathToNew = msg->newInstance.ToString().c_str();

					if (pathToNew == DEM::Minimap::path)
					{
						DEM::Minimap::InitSingleton(msg->newInstance);

						LogMapMembers<logger::level::debug>(msg);
					}
				}
				break;
			}
		case IUI::API::Message::Type::kAbortPatchInstance:
			{
				if (auto msg = IUI::API::TranslateAs<IUI::API::AbortPatchInstanceMessage>(a_msg))
				{
					std::string pathToOriginal = msg->originalValue.ToString().c_str();
				}
				break;
			}
		case IUI::API::Message::Type::kFinishLoadInstances:
			{
				if (auto msg = IUI::API::TranslateAs<IUI::API::FinishLoadInstancesMessage>(a_msg))
				{
					auto hudMenu = static_cast<RE::HUDMenu*>(msg->menu);

					hudMenu->GetRuntimeData().objects.push_back(DEM::Minimap::GetSingleton());
				}
				logger::info("Finished loading HUD patches");
				break;
			}
		case IUI::API::Message::Type::kPostInitExtensions:
			{
				if (auto msg = IUI::API::TranslateAs<IUI::API::PostInitExtensionsMessage>(a_msg))
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
