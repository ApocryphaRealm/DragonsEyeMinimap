import gfx.io.GameDelegate;
import Map.LocalMap;
import Map.MapMarker;
import Shared.ButtonChange;
import Shared.GlobalFunc;

class Map.MapMenu
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

	/* API */
	public var MarkerData:Array;
	public var YouAreHereMarker:MovieClip;
	public var PlayerLocationMarkerType:String;
	public var MarkerDescriptionObj:MovieClip;

	public var ExtraMarkerData:Array = new Array();

	/* Properties */
	private var localMap:MovieClip;
	private var mapHeight:Number;
	private var mapWidth:Number;
	private var markerContainer:MovieClip;

	private var markerDescriptionHolder:MovieClip;

	private var markers:Array;

	private var iPlatform:Number;

	/* Test */
	private var Test:TextField;

	function MapMenu(a_localMap:LocalMap)
	{
		localMap = a_localMap;

		mapWidth = localMap.textureWidth;
		mapHeight = localMap.textureHeight;

		Test = localMap.Test;

		markerContainer = localMap.createEmptyMovieClip();
		markers = new Array();
	}

	/* API */
	public function SetNumMarkers(a_numMarkers:Number):Void
	{
		MapMarker.TopDepth = a_numMarkers;
	}

	/* API */
	public function CreateMarkers():Void
	{
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
			marker.Type = createIconType;

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

		var center_x:Number = mapWidth / 2;
		var center_y:Number = mapHeight / 2;
		var radius:Number = center_x - 15;

		var i:Number = 0;
		for (var j:Number = 0; j < dataLen; j = j + REFRESH_STRIDE)
		{
			var marker:MovieClip = markers[i];
			marker._visible = MarkerData[j + REFRESH_SHOW];
			if (marker._visible)
			{
				marker._x = MarkerData[j + REFRESH_X] * mapWidth;
				marker._y = MarkerData[j + REFRESH_Y] * mapHeight;
				marker._rotation = MarkerData[j + REFRESH_ROTATION];

				var rel_x:Number = center_x - marker._x;
				var rel_y:Number = center_y - marker._y;

				var distance:Number = Math.sqrt(rel_x * rel_x + rel_y * rel_y);

				if (distance > radius)
				{
					if (marker.Type == "QuestTargetMarker" ||
						marker.Type == "QuestTargetDoorMarker" ||
						marker.Type == "MultipleQuestTargetMarker")
					{
						var angle:Number = Math.atan2(rel_y, rel_x);

						marker._x = center_x - radius * Math.cos(angle);
						marker._y = center_y - radius * Math.sin(angle);
						marker._rotation = angle * 180 / Math.PI + 90;
					}
					else
					{
						marker._visible = false;
					}
				}

				if (marker.Icon == YouAreHereMarker)
				{
					localMap.VisionCone._visible = marker._visible;
					localMap.VisionCone._x = marker._x;
					localMap.VisionCone._y = marker._y;
				}
			}
			++i;
		}
	}
}
