import gfx.io.GameDelegate;

class Map.MapMarker extends MovieClip
{
	static var TopDepth: Number = 0;
	static var IconTypes: Array = new Array("EmptyMarker", "CityMarker", "TownMarker", "SettlementMarker", "CaveMarker", "CampMarker", "FortMarker", "NordicRuinMarker", "DwemerMarker", "ShipwreckMarker", "GroveMarker", "LandmarkMarker", "DragonlairMarker", "FarmMarker", "WoodMillMarker", "MineMarker", "ImperialCampMarker", "StormcloakCampMarker", "DoomstoneMarker", "WheatMillMarker", "SmelterMarker", "StableMarker", "ImperialTowerMarker", "ClearingMarker", "PassMarker", "AltarMarker", "RockMarker", "LighthouseMarker", "OrcStrongholdMarker", "GiantCampMarker", "ShackMarker", "NordicTowerMarker", "NordicDwellingMarker", "DocksMarker", "ShrineMarker", "RiftenCastleMarker", "RiftenCapitolMarker", "WindhelmCastleMarker", "WindhelmCapitolMarker", "WhiterunCastleMarker", "WhiterunCapitolMarker", "SolitudeCastleMarker", "SolitudeCapitolMarker", "MarkarthCastleMarker", "MarkarthCapitolMarker", "WinterholdCastleMarker", "WinterholdCapitolMarker", "MorthalCastleMarker", "MorthalCapitolMarker", "FalkreathCastleMarker", "FalkreathCapitolMarker", "DawnstarCastleMarker", "DawnstarCapitolMarker", "DLC02MiraakTempleMarker", "DLC02RavenRockMarker", "DLC02StandingStonesMarker", "DLC02TelvanniTowerMarker", "DLC02ToSkyrimMarker", "DLC02ToSolstheimMarker", "DLC02CastleKarstaagMarker", "", "DoorMarker", "QuestTargetMarker", "QuestTargetDoorMarker", "MultipleQuestTargetMarker", "PlayerSetMarker", "YouAreHereMarker");
	static var ExtraIconTypes: Array = new Array("EnemyMarker", "HostileMarker", "GuardMarker", "DeadMarker", "TeammateMarker", "NeutralMarker");

	var Type:String;
}
