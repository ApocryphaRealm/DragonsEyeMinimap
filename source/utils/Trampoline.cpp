#include "utils/Trampoline.h"

#include <Psapi.h>

namespace hooks
{
	// Reference: BranchTrampoline::Create() of https://github.com/ianpatt/skse64/blob/master/skse64_common/BranchTrampoline.cpp
	CustomTrampoline::CustomTrampoline(const std::string_view& a_name, void* a_module, std::size_t a_size) :
		Trampoline{ a_name }
	{
		// search backwards from module base
		auto moduleBase = reinterpret_cast<std::uintptr_t>(a_module);
		std::uintptr_t addr = moduleBase;
		std::uintptr_t maxDisplacement = 0x80000000 - (1024 * 1024 * 128);	// largest 32-bit displacement with 128MB scratch space
		std::uintptr_t lowestOKAddress = (moduleBase >= maxDisplacement) ? moduleBase - maxDisplacement : 0;
		addr--;

		void* base = nullptr;

		while (!base)
		{
			MEMORY_BASIC_INFORMATION info;

			if (!VirtualQuery((void*)addr, &info, sizeof(info)))
			{
				break;
			}

			if (info.State == MEM_FREE)
			{
				// free block, big enough?
				if (info.RegionSize >= a_size)
				{
					// try to allocate it
					addr = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize - a_size;

					base = VirtualAlloc(reinterpret_cast<void*>(addr), a_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
				}
			}

			// move back and try again
			if (!base)
			{
				addr = ((uintptr_t)info.BaseAddress) - 1;
			}

			if (addr < lowestOKAddress)
			{
				break;
			}
		}

		inst->set_trampoline(base, a_size,
			[](void* a_mem, std::size_t)
			{
				REX::W32::VirtualFree(a_mem, 0, MEM_RELEASE);
			});
	}

	std::pair<std::uintptr_t, std::size_t> SigScanner::GetModuleInfo(REX::W32::HMODULE a_moduleHandle)
	{
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(a_moduleHandle), &moduleInfo, sizeof(MODULEINFO));
		auto base = reinterpret_cast<std::uintptr_t>(moduleInfo.lpBaseOfDll);
		std::size_t size = moduleInfo.SizeOfImage;

		return { base, size };
	}
}