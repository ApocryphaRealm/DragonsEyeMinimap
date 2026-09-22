#pragma once

// Skyrim 1.7 line (CommonLibSSE-NG 7.2): the two CommonLibSSE-NG 3.7 names this repo still uses.
// SKSE::WinAPI -> REX::W32; RE::Offset -> CommonLibSSE-NG 3.7's RELOCATION_ID pairs (1.7 resolves the AE id),
// each verified on 1.7.104 with .MD/scripts/check-17.py (tools/check-17.json).
#if RUNTIME_LINE == 17
#	include <REX/W32/KERNEL32.h>

#	pragma push_macro("GetModuleHandle")
#	undef GetModuleHandle
namespace SKSE::WinAPI
{
	using HMODULE = REX::W32::HMODULE;
	inline HMODULE GetModuleHandle(const char* a_name) { return REX::W32::GetModuleHandleA(a_name); }
	inline HMODULE GetModuleHandle(const wchar_t* a_name) { return REX::W32::GetModuleHandleW(a_name); }
	inline HMODULE GetModuleHandleA(const char* a_name) { return REX::W32::GetModuleHandleA(a_name); }
	inline HMODULE GetModuleHandleW(const wchar_t* a_name) { return REX::W32::GetModuleHandleW(a_name); }
	// a call site written GetModuleHandle("x.dll") becomes GetModuleHandleW("x.dll") wherever windows.h's UNICODE macro is active
	inline HMODULE GetModuleHandleW(const char* a_name) { return REX::W32::GetModuleHandleA(a_name); }
	inline bool VirtualFree(void* a_address, std::size_t a_size, std::uint32_t a_type) { return REX::W32::VirtualFree(a_address, a_size, a_type); }
}
#	pragma pop_macro("GetModuleHandle")

namespace RE::Offset
{
	namespace GFxValue {	namespace ObjectInterface {
		inline constexpr auto AttachMovie = RELOCATION_ID(80197, 82219);
		inline constexpr auto DeleteMember = RELOCATION_ID(80207, 82230);
		inline constexpr auto GetArraySize = RELOCATION_ID(80214, 82237);
		inline constexpr auto GetDisplayInfo = RELOCATION_ID(80216, 82239);
		inline constexpr auto GetElement = RELOCATION_ID(80218, 82241);
		inline constexpr auto GetMember = RELOCATION_ID(80222, 82245);
		inline constexpr auto GotoAndPlay = RELOCATION_ID(80230, 82253);
		inline constexpr auto HasMember = RELOCATION_ID(80231, 82254);
		inline constexpr auto Invoke = RELOCATION_ID(80233, 82256);
		inline constexpr auto ObjectAddRef = RELOCATION_ID(80244, 82269);
		inline constexpr auto ObjectRelease = RELOCATION_ID(80245, 82270);
		inline constexpr auto PushBack = RELOCATION_ID(80248, 82273);
		inline constexpr auto RemoveElements = RELOCATION_ID(80252, 82280);
		inline constexpr auto SetArraySize = RELOCATION_ID(80261, 82285);
		inline constexpr auto SetDisplayInfo = RELOCATION_ID(80263, 82287);
		inline constexpr auto SetElement = RELOCATION_ID(80265, 82289);
		inline constexpr auto SetMember = RELOCATION_ID(80268, 82292);
		inline constexpr auto SetText = RELOCATION_ID(80270, 82293);
	}}
	namespace TESQuest {
		inline constexpr auto EnsureQuestStarted = RELOCATION_ID(24481, 25003);
		inline constexpr auto ResetQuest = RELOCATION_ID(24486, 25014);
	}
	namespace LocalMapCamera {
		inline constexpr auto Ctor = RELOCATION_ID(16084, 16325);
		inline constexpr auto SetNorthRotation = RELOCATION_ID(16089, 16330);
	}
}
#endif
