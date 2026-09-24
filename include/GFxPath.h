#pragma once

#include <string>

// A display object's path ("_level0.HUDMovieBaseInstance.Minimap") from its GFxValue - the string Infinity UI's
// patch messages are matched on.
//
// Line 1 (SE/AE 1.6) keeps this repo's own GFxValue::ToString, which calls the game's GFxValue::ToString (80274 /
// 82297). On the Skyrim 1.7 line the RE headers forward to CommonLibSSE-NG 7.2, whose ToString returned "" for a
// display object on 1.7.104 (2026-09-24: every kPostPatchInstance path arrived empty, the minimap was never created
// and the game stopped with "Minimap.swf not found"). Line 17 therefore calls the game's own function by the same
// Address Library ID Infinity UI's working 1.7 build uses (82297; tools\check-17.json).
namespace dem
{
	inline std::string GFxPath(const RE::GFxValue& a_value)
	{
#if RUNTIME_LINE == 17
		using func_t = RE::GString* (*)(const RE::GFxValue*, RE::GString*);
		static REL::Relocation<func_t> func{ REL::ID(82297) };
		RE::GString out;
		func(std::addressof(a_value), std::addressof(out));
		const char* s = out.c_str();
		return s ? std::string(s) : std::string();
#else
		return a_value.ToString().c_str();
#endif
	}
}
