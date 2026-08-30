#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "MiniMap.h"
#include "Settings.h"

#include <string>

namespace DEM::devbench
{
	namespace
	{
		std::string JsonStr(const std::string& a_json, const std::string& a_key)
		{
			const std::string pat = "\"" + a_key + "\"";
			auto p = a_json.find(pat);
			if (p == std::string::npos) { return {}; }
			p = a_json.find(':', p + pat.size());
			if (p == std::string::npos) { return {}; }
			p = a_json.find_first_not_of(" \t", p + 1);
			if (p == std::string::npos || a_json[p] != '"') { return {}; }
			const auto e = a_json.find('"', p + 1);
			return e == std::string::npos ? std::string{} : a_json.substr(p + 1, e - p - 1);
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string args = a_argsJson ? a_argsJson : "{}";
			const std::string op = JsonStr(args, "op");

			auto* mini = Minimap::GetSingleton();
			if (!mini)
			{
				a_write(a_sink, "{\"ok\":false,\"error\":\"minimap singleton not created yet\"}");
				return;
			}

			if (op == "show" || op == "hide")
			{
				auto* task = SKSE::GetTaskInterface();
				if (!task)
				{
					a_write(a_sink, "{\"ok\":false,\"error\":\"no task interface\"}");
					return;
				}
				const bool show = (op == "show");
				task->AddTask([show]() {
					if (auto* m = Minimap::GetSingleton(); m && m->IsReady())
					{
						// Runtime toggle only - never persists bShowOnGameStart (same contract as
						// the hide key's tap).
						show ? m->Show(false) : m->Hide(false);
					}
				});
				std::string reply = std::string("{\"ok\":true,\"op\":\"") + op + "\",\"queued\":true}";
				a_write(a_sink, reply.c_str());
				return;
			}

			if (op == "state" || op.empty())
			{
				float l = 0, t = 0, r = 0, b = 0, sw = 0, sh = 0;
				bool fromShown = false;
				{
					std::scoped_lock lock(Minimap::stageRectLock);
					l = Minimap::stageRect.left; t = Minimap::stageRect.top;
					r = Minimap::stageRect.right; b = Minimap::stageRect.bottom;
					sw = Minimap::stageRectStageW; sh = Minimap::stageRectStageH;
					fromShown = Minimap::stageRectFromShown;
				}
				char buf[512];
				std::snprintf(buf, sizeof(buf),
							  "{\"ok\":true,\"ready\":%s,\"shown\":%s,\"visible\":%s,"
							  "\"stageRect\":{\"left\":%.1f,\"top\":%.1f,\"right\":%.1f,\"bottom\":%.1f,\"stageW\":%.1f,\"stageH\":%.1f,\"fromShown\":%s},"
							  "\"compass\":{\"ring\":%s,\"pointer\":%s}}",
							  mini->IsReady() ? "true" : "false",
							  (mini->IsReady() && mini->IsShown()) ? "true" : "false",
							  (mini->IsReady() && mini->IsVisible()) ? "true" : "false",
							  l, t, r, b, sw, sh, fromShown ? "true" : "false",
							  settings::compass::compassRing ? "true" : "false",
							  settings::compass::questPointer ? "true" : "false");
				a_write(a_sink, buf);
				return;
			}

			a_write(a_sink, "{\"ok\":false,\"error\":\"op must be show|hide|state\"}");
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }
		auto* dev = DevBenchAPI::GetDevBenchInterface001();
		if (!dev)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; the dem.control driving tool is unavailable this session (the minimap works normally)"); }
			return;
		}
		constexpr const char* descriptor =
			"{"
			"\"description\":\"Drive Dragon's Eye Minimap for testing. op: show|hide (runtime toggle, "
			"never persisted), state (ready/shown/visible, the stage rect, compass toggles).\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";
		if (dev->RegisterTool("dem.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("Registered \"dem.control\" driving tool with DevBench (build {})", dev->GetBuildNumber());
		}
		registered = true;
	}
}
