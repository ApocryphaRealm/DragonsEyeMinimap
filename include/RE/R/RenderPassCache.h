#pragma once

namespace RE
{
	class RenderPassCache
	{
	public:

		struct Pool
		{
			BSRenderPass* passes;
			BSLight** lights;
			BSRenderPass* firstAvailable;
			BSRenderPass* lastAvailable;
			BSSpinLock lock;
		};

		static RenderPassCache* GetSingleton()
		{
			static const REL::Relocation<RenderPassCache*> singleton(RELOCATION_ID(528206, 415150));

			return singleton.get();
		}

		static void CleanupThisThreadPool()
		{
			using func_t = decltype(&RenderPassCache::CleanupThisThreadPool);
			REL::Relocation<func_t> func{ RELOCATION_ID(100722, 107502) };

			return func();
		}

		Pool pools[2];
	};
}