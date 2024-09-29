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

	private static var EXTRA_REFRESH_X:Number = 0;
	private static var EXTRA_REFRESH_Y:Number = 1;
	private static var EXTRA_REFRESH_STRIDE:Number = 2;
	private static var EXTRA_CREATE_NAME:Number = 0;
	private static var EXTRA_CREATE_ICONTYPE:Number = 1;
	private static var EXTRA_CREATE_STRIDE:Number = 2;

	/* API */
	public var MarkerData:Array;
	public var YouAreHereMarker:MovieClip;
	public var ExtraMarkerData:Array;
	public var PlayerLocationMarkerType:String;
	public var MarkerDescriptionObj:MovieClip;

	/* Properties */
	private var localMap:MovieClip;
	private var mapHeight:Number;
	private var mapWidth:Number;
	private var markerContainer:MovieClip;

	private var markerDescriptionHolder:MovieClip;
	private var markers:Array;

	private var SelectedMarker:MovieClip;

	private var iPlatform:Number;

	private var testTf:TextField;

	function MapMenu(a_localMap:LocalMap)
	{
		localMap = a_localMap;

		mapWidth = localMap.textureWidth;
		mapHeight = localMap.textureHeight;

		markerDescriptionHolder = localMap.attachMovie("DescriptionHolder", "markerDescriptionHolder", localMap.getNextHighestDepth());
		markerDescriptionHolder._visible = false;
		markerDescriptionHolder.hitTestDisable = true;
		MarkerDescriptionObj = markerDescriptionHolder.Description;

		testTf = localMap.TestTf;
		testTf.text = "Hello from MapMenu";

		localMap.gotoAndStop("hide");

		markerContainer = localMap.createEmptyMovieClip();
		markers = new Array();
	}

	/* API */
	public function SetNumMarkers(a_numMarkers:Number):Void
	{
		MapMarker.TopDepth = a_numMarkers;
		SetSelectedMarker(-1);
	}

	/* API */
	public function CreateMarkers():Void
	{
		markerContainer.removeMovieClip();
		markerContainer = localMap.createEmptyMovieClip("MarkerClips", 1);

		var i:Number = 0;
		var dataLen:Number = MarkerData.length;
		var extraDataLen:Number = ExtraMarkerData.length;

		delete markers;
		markers = new Array(dataLen / CREATE_STRIDE + extraDataLen / EXTRA_CREATE_STRIDE);

		for (var j:Number = 0; j < dataLen; j = j + CREATE_STRIDE)
		{
			var createIconType = MarkerData[j + CREATE_ICONTYPE];

			var mapMarker:MovieClip = markerContainer.attachMovie(MapMarker.IconTypes[createIconType], "Marker" + i, i);
			markers[i] = mapMarker;
			if (createIconType == PlayerLocationMarkerType)
			{
				YouAreHereMarker = mapMarker.Icon;
			}
			mapMarker.Index = i;
			mapMarker.label = MarkerData[j + CREATE_NAME];
			mapMarker.textField._visible = false;
			mapMarker.visible = false;
			if (MarkerData[j + CREATE_UNDISCOVERED] && mapMarker.IconClip != undefined)
			{
				var depth:Number = mapMarker.IconClip.getNextHighestDepth();
				mapMarker.IconClip.attachMovie(MapMarker.IconTypes[MarkerData[j + CREATE_ICONTYPE]] + "Undiscovered", "UndiscoveredIcon", depth);
			}
			++i;
		}

		for (var j:Number = 0; j < extraDataLen; j = j + EXTRA_CREATE_STRIDE)
		{
			var mapMarker:MovieClip = markerContainer.attachMovie(MapMarker.ExtraIconTypes[ExtraMarkerData[j + EXTRA_CREATE_ICONTYPE]], "Marker" + i, i);
			markers[i] = mapMarker;
			mapMarker.Index = i;
			mapMarker.label = ExtraMarkerData[j + EXTRA_CREATE_NAME];
			mapMarker.textField._visible = false;
			mapMarker.visible = true;
			++i;
		}

		var mapMarker:MapMarker = markers[0];
		var markersLen:Number = markers.length;

		testTf.text = "CreateMarkers: markersLen="+ markersLen + " " + mapMarker.Index + ": " + mapMarker.textField;
	}

	function RefreshMarkers():Void
	{
		var i:Number = 0;
		var dataLen:Number = MarkerData.length;
		var extraDataLen:Number = ExtraMarkerData.length;

		var markersLen:Number = markers.length;

		for (var j:Number = 0; j < dataLen; j = j + REFRESH_STRIDE)
		{
			var marker:MovieClip = markers[i];
			marker._visible = MarkerData[j + REFRESH_SHOW];
			if (marker._visible)
			{
				marker._x = MarkerData[j + REFRESH_X] * mapWidth;
				marker._y = MarkerData[j + REFRESH_Y] * mapHeight;
				marker._rotation = MarkerData[j + REFRESH_ROTATION];
			}
			++i;

			testTf.text = "m: " + markersLen + " i: " + dataLen + " xi: " + extraDataLen + " ";
			if (marker._visible)
			{
				testTf.text += "x: " + marker._x + " y: " + marker._y;
			}
		}

		for (var j:Number = 0; j < extraDataLen; j = j + EXTRA_REFRESH_STRIDE)
		{
			var marker:MapMarker = markers[i];
			marker._visible = true;
			if (marker._visible)
			{
				marker._x = ExtraMarkerData[j + EXTRA_REFRESH_X] * mapWidth;
				marker._y = ExtraMarkerData[j + EXTRA_REFRESH_Y] * mapHeight;
			}
			++i;
		}

		if (SelectedMarker != undefined)
		{
			markerDescriptionHolder._x = SelectedMarker._x + markerContainer._x;
			markerDescriptionHolder._y = SelectedMarker._y + markerContainer._y;
		}
	}

	function SetSelectedMarker(aiSelectedMarkerIndex:Number):Void
	{
		var marker:MovieClip = aiSelectedMarkerIndex < 0 ? undefined : markers[aiSelectedMarkerIndex];
		if (marker != SelectedMarker)
		{
			if (SelectedMarker != undefined)
			{
				SelectedMarker.MarkerRollOut();
				SelectedMarker = undefined;
				markerDescriptionHolder.gotoAndPlay("Hide");
			}
			if (marker != undefined && marker.visible && marker.MarkerRollOver())
			{
				SelectedMarker = marker;
				markerDescriptionHolder._visible = true;
				markerDescriptionHolder.gotoAndPlay("Show");
				return;
			}
			SelectedMarker = undefined;
		}
	}

	function ClickSelectedMarker():Void
	{
		if (SelectedMarker != undefined)
		{
			SelectedMarker.MarkerClick();
		}
	}
}
