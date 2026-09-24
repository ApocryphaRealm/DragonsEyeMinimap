#pragma once

#if RUNTIME_LINE == 17
// Skyrim 1.7 line: CommonLibSSE-NG 7.2 ships this type with its 1.7 layout, so that one is used (see CMakeLists.txt, line 17).
#	include <include/RE/L/LoadedAreaBound.h>
#else

#include "RE/N/NiRefObject.h"
#include "RE/T/TESObjectCELL.h"

namespace RE
{
	class bhkAabbPhantom;

	class LoadedAreaBound : NiRefObject
	{
	public:

		bhkAabbPhantom* aabbPhantoms[6];  // 10
		TESObjectCELL* cell;			  // 40
		std::uint64_t unk48;			  // 48
		std::uint64_t unk50;			  // 50
		std::uint64_t unk58;			  // 58
		std::uint64_t unk60;			  // 60
		std::uint64_t unk68;			  // 68
		std::uint64_t unk70;			  // 70
		NiPoint3 maxExtent;				  // 78
		NiPoint3 minExtent;				  // 84
		float unk90;					  // 90
		float unk94;					  // 94
		float unk98;					  // 98
		float unk9C;					  // 9C
	};
	static_assert(sizeof(LoadedAreaBound) == 0xA0);
}
#endif  // RUNTIME_LINE
