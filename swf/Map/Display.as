import gfx.io.GameDelegate;
import Map.LocalMap;
import Map.MapMarker;
import Shared.ButtonChange;
import Shared.GlobalFunc;

class Map.Display
{
	/* Constants */
	private static var REFRESH_SHOW:Number = 0;
	private static var REFRESH_X:Number = 1;
	private static var REFRESH_Y:Number = 2;
	private static var REFRESH_ROTATION:Number = 3;
	private static var REFRESH_STRIDE:Number = 4;
	private static var CREATE_NAME:Number = 0;
	private static var CREATE_ICONTYPE:Number = 1;
	private static var CREATE_UNDISCOVERED:Number = 2;
	private static var CREATE_STRIDE:Number = 3;

	private static var SHAPE_SQUARED:Number = 0;
	private static var SHAPE_ROUND:Number = 1;

	/* API */
	public var MarkerData:Array = new Array();
	public var ExtraMarkerData:Array = new Array();
	public var YouAreHereMarker:MovieClip;
	public var PlayerLocationMarkerType:String;
	public var MarkerDescriptionObj:MovieClip;

	/* Properties */
	private var localMap:LocalMap;
	private var visionCone:MovieClip;
	private var mapHeight:Number;
	private var mapWidth:Number;
	private var mapRadius:Number;
	private var mapCenter:Object = new Object();
	private var markerToCenter:Object = new Object();
	private var markerContainer:MovieClip;
	private var shape:Number = SHAPE_SQUARED;
	private var showEnemyMarkers:Boolean = true;
	private var showHostileMarkers:Boolean = true;
	private var showGuardMarkers:Boolean = true;
	private var showDeadMarkers:Boolean = true;

	private var markers:Array;

	function Display(a_localMap:LocalMap)
	{
		localMap = a_localMap;

		mapWidth = localMap.textureWidth;
		mapHeight = localMap.textureHeight;
		mapRadius = mapWidth / 2;
		mapCenter._x = mapWidth / 2;
		mapCenter._y = mapHeight / 2;

		visionCone = localMap.VisionCone;

		markerContainer = localMap.createEmptyMovieClip();
		markers = new Array();
	}

	public function SetShape(a_shape:Number):Void
	{
		shape = a_shape;
	}

	/* API */
	public function SetNumMarkers(a_numMarkers:Number):Void
	{
		MapMarker.TopDepth = a_numMarkers;
	}

	/* API */
	public function CreateMarkers(a_showEnemyMarkers:Boolean, a_showHostileMarkers:Boolean,
								  a_showGuardMarkers:Boolean, a_showDeadMarkers:Boolean):Void
	{
		showEnemyMarkers = a_showEnemyMarkers;
		showHostileMarkers = a_showHostileMarkers;
		showGuardMarkers = a_showGuardMarkers;
		showDeadMarkers = a_showDeadMarkers;

		markerContainer.removeMovieClip();
		markerContainer = localMap.createEmptyMovieClip("MarkerClips", 1);

		var dataLen:Number = MarkerData.length;

		delete markers;
		markers = new Array(dataLen / CREATE_STRIDE);

		var i:Number = 0;
		var k:Number = 0;

		for (var j:Number = 0; j < dataLen; j = j + CREATE_STRIDE)
		{
			var createIconType = MarkerData[j + CREATE_ICONTYPE];
			var markerType:String;
			if (createIconType == 0)
			{
				markerType = MapMarker.ExtraIconTypes[ExtraMarkerData[k]];
				k++;
			}
			else
			{
				markerType = MapMarker.IconTypes[createIconType];
			}

			var marker:MovieClip = markerContainer.attachMovie(markerType, "Marker" + i, i);
			if (createIconType == PlayerLocationMarkerType)
			{
				YouAreHereMarker = marker.Icon;
			}
			marker.Index = i;
			marker.label = MarkerData[j + CREATE_NAME];
			marker.textField._visible = false;
			marker.visible = false;
			marker.Type = markerType;

			if (MarkerData[j + CREATE_UNDISCOVERED] && marker.IconClip != undefined)
			{
				var depth:Number = marker.IconClip.getNextHighestDepth();
				marker.IconClip.attachMovie(markerType + "Undiscovered", "UndiscoveredIcon", depth);
			}

			markers[i] = marker;
			++i;
		}
	}

	/* API */
	function RefreshMarkers():Void
	{
		var dataLen:Number = MarkerData.length;

		var i:Number = 0;
		var youAreHereMarkerFound:Boolean = false;
		for (var j:Number = 0; j < dataLen; j = j + REFRESH_STRIDE)
		{
			var marker:MovieClip = markers[i];

			marker._visible = MarkerData[j + REFRESH_SHOW];

			if ((marker.Type == "EnemyMarker"   && !showEnemyMarkers) ||
				(marker.Type == "HostileMarker" && !showHostileMarkers) ||
				(marker.Type == "GuardMarker"   && !showGuardMarkers) ||
				(marker.Type == "DeadMarker"    && !showDeadMarkers))
			{
				marker._visible = false;
			}

			if (marker._visible)
			{
				marker._x = MarkerData[j + REFRESH_X] * mapWidth;
				marker._y = MarkerData[j + REFRESH_Y] * mapHeight;
				marker._rotation = MarkerData[j + REFRESH_ROTATION];

				if (shape == SHAPE_ROUND)
				{
					markerToCenter._x = mapCenter._x - marker._x;
					markerToCenter._y = mapCenter._y - marker._y;

					var distance:Number = Math.sqrt(markerToCenter._x * markerToCenter._x + markerToCenter._y * markerToCenter._y);

					if (distance > mapRadius - 20)
					{
						if (marker.Type == "QuestTargetMarker" ||
							marker.Type == "QuestTargetDoorMarker" ||
							marker.Type == "MultipleQuestTargetMarker")
						{
							var angle:Number = Math.atan2(markerToCenter._y, markerToCenter._x);

							marker._x = mapCenter._x - mapRadius * Math.cos(angle);
							marker._y = mapCenter._y - mapRadius * Math.sin(angle);
							marker._rotation = angle * 180 / Math.PI + 90;
						}
						else
						{
							marker._visible = false;
						}
					}
				}

				if (marker.Icon == YouAreHereMarker)
				{
					youAreHereMarkerFound = true;
					visionCone._visible = marker._visible;
					visionCone._x = marker._x;
					visionCone._y = marker._y;
				}
			}
			++i;
		}
		if (!youAreHereMarkerFound)
		{
			visionCone._visible = false;
		}
	}
}
