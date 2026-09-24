#pragma once

#if RUNTIME_LINE == 17
// Skyrim 1.7 line: CommonLibSSE-NG 7.2 ships this type with its 1.7 layout, so that one is used (see CMakeLists.txt, line 17).
#	include <include/RE/G/GFxImageLoader.h>
#else

#include "RE/G/GFxState.h"

namespace RE
{
	class GImageInfoBase;

	class GFxImageLoader : public GFxState
	{
	public:

		~GFxImageLoader() override = default;  // 00

		// add
		virtual GImageInfoBase* LoadImage(const char* a_imageUrl) = 0; // 01
	};
	static_assert(sizeof(GFxImageLoader) == 0x18);
}
#endif  // RUNTIME_LINE
