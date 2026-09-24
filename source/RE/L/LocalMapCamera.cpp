// Compiled on both build lines. On the Skyrim 1.7 line the header forwards to CommonLibSSE-NG 7.2's LocalMapCamera, whose
// library defines these same functions in a file that also defines LocalMapCamera's constructor - and linking that file
// needs the engine-only TESCamera/LocalMapCamera virtuals it cannot resolve. Defining them here keeps that file out of
// the link. The bodies match 7.2's line for line, and the two ids (RE::Offset::LocalMapCamera in Compat17.h on line 17,
// 16084/16325 and 16089/16330) are in tools/check-17.json.
#include "RE/L/LocalMapCamera.h"

#include "RE/I/INISettingCollection.h"
#include "RE/N/NiCamera.h"

namespace RE
{
	void LocalMapCamera::SetAreaBounds(NiPoint3& a_maxExtent, NiPoint3& a_minExtent)
	{
		auto ini = INISettingCollection::GetSingleton();
		auto fMapLocalHeight = ini ? ini->GetSetting("fMapLocalHeight:MapMenu") : nullptr;

		minExtent = a_minExtent;
		maxExtent = a_maxExtent;
		maxExtent.z += fMapLocalHeight ? fMapLocalHeight->GetFloat() : 0.0F;
	}

	void LocalMapCamera::SetDefaultStateInitialPosition(NiPoint3& a_position)
	{
		auto ini = INISettingCollection::GetSingleton();
		auto fMapLocalHeight = ini ? ini->GetSetting("fMapLocalHeight:MapMenu") : nullptr;

		defaultState->initialPosition = a_position;
		defaultState->initialPosition.z += fMapLocalHeight ? fMapLocalHeight->GetFloat() : 0.0F;
	}

	void LocalMapCamera::SetDefaultStateMinFrustumDimensions(float a_width, float a_height)
	{
		defaultState->minFrustumHalfWidth = a_width / 2.0F;
		defaultState->minFrustumHalfHeight = a_height / 2.0F;
	}

	void LocalMapCamera::SetDefaultStateTranslation(float a_x, float a_y, float a_z)
	{
		defaultState->translation.x = a_x - defaultState->initialPosition.x;
		defaultState->translation.y = a_y - defaultState->initialPosition.y;
		defaultState->translation.z = a_z - defaultState->initialPosition.z;
	}

	void LocalMapCamera::SetNorthRotation(float a_northRotation)
	{
		using func_t = decltype(&LocalMapCamera::SetNorthRotation);
		REL::Relocation<func_t> func{ Offset::LocalMapCamera::SetNorthRotation };
		return func(this, a_northRotation);
	}

	LocalMapCamera* LocalMapCamera::Ctor(float a_zRotation)
	{
		using func_t = decltype(&LocalMapCamera::Ctor);
		REL::Relocation<func_t> func{ Offset::LocalMapCamera::Ctor };
		return func(this, a_zRotation);
	}
}
