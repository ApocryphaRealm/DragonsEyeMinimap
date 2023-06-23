import gfx.io.GameDelegate;
import Map.LocalMap;
import Map.MapMarker;
import Shared.ButtonChange;
import Shared.GlobalFunc;

class Map.MapMenu
{
	static var REFRESH_SHOW:Number = 0;
	static var REFRESH_X:Number = 1;
	static var REFRESH_Y:Number = 2;
	static var REFRESH_ROTATION:Number = 3;
	static var REFRESH_STRIDE:Number = 4;
	static var CREATE_NAME:Number = 0;
	static var CREATE_ICONTYPE:Number = 1;
	static var CREATE_UNDISCOVERED:Number = 2;
	static var CREATE_STRIDE:Number = 3;
	static var MARKER_CREATE_PER_FRAME:Number = 10;

	var LocalMapMenu:LocalMap;
	var MapHeight:Number;
	var MapMovie:MovieClip;
	var MapWidth:Number;
	var MarkerContainer:MovieClip;
	var MarkerData:Array;
	var MarkerDescriptionHolder:MovieClip;
	var MarkerDescriptionObj:MovieClip;
	var Markers:Array;
	var NextCreateIndex:Number;
	var PlayerLocationMarkerType:String;
	var SelectedMarker:MovieClip;
	var YouAreHereMarker:MovieClip;
	var iPlatform:Number;

	var testTf:TextField;

	function MapMenu(aMapMovie:MovieClip)
	{
		MapMovie = aMapMovie;

		testTf = MapMovie.TestTf;
		testTf.text = "Hello from MapMenu";

		MarkerContainer = MapMovie.createEmptyMovieClip("MarkerClips", 1);
		Markers = new Array();
		NextCreateIndex = -1;
		LocalMapMenu = LocalMap(MapMovie);
		MapWidth = LocalMapMenu.TextureWidth;
		MapHeight = LocalMapMenu.TextureHeight;
		Mouse.addListener(this);
		MarkerDescriptionHolder = MapMovie.attachMovie("DescriptionHolder", "MarkerDescriptionHolder", MapMovie.getNextHighestDepth());
		MarkerDescriptionHolder._visible = false;
		MarkerDescriptionHolder.hitTestDisable = true;
		MarkerDescriptionObj = MarkerDescriptionHolder.Description;
		Stage.addListener(this);
		Init();
	}

		function Init():Void
	{
		MapMovie.swapDepths(3);
		MapMovie.gotoAndStop("hide");
		GameDelegate.addCallBack("RefreshMarkers", this, "RefreshMarkers");
		GameDelegate.addCallBack("SetSelectedMarker", this, "SetSelectedMarker");
		GameDelegate.addCallBack("ClickSelectedMarker", this, "ClickSelectedMarker");
	}

	function onMouseDown():Void
	{
		GameDelegate.call("ClickCallback", []);
	}

	function SetNumMarkers(aNumMarkers:Number):Void
	{
		if (MarkerContainer != undefined)
		{
			MarkerContainer.removeMovieClip();
			MarkerContainer = MapMovie.createEmptyMovieClip("MarkerClips", 1);
		}
		delete Markers;
		Markers = new Array(aNumMarkers);
		Map.MapMarker.TopDepth = aNumMarkers;
		NextCreateIndex = 0;
		SetSelectedMarker(-1);
	}

	function GetCreatingMarkers():Boolean
	{
		testTf.text = "GetCreatingMarkers";

		return NextCreateIndex != -1;
	}

	function CreateMarkers():Void
	{
		if (NextCreateIndex != -1 && MarkerContainer != undefined)
		{
			var i:Number = 0;
			var j:Number = NextCreateIndex * CREATE_STRIDE;
			var markersLen:Number = Markers.length;
			var dataLen:Number = MarkerData.length;

			while (NextCreateIndex < markersLen && j < dataLen && i < MARKER_CREATE_PER_FRAME)
			{
				var mapMarker:MovieClip = MarkerContainer.attachMovie(Map.MapMarker.IconTypes[MarkerData[j + CREATE_ICONTYPE]], "Marker" + NextCreateIndex, NextCreateIndex);
				Markers[NextCreateIndex] = mapMarker;
				if (MarkerData[j + CREATE_ICONTYPE] == PlayerLocationMarkerType)
				{
					YouAreHereMarker = mapMarker.Icon;
				}
				mapMarker.Index = NextCreateIndex;
				mapMarker.label = MarkerData[j + CREATE_NAME];
				mapMarker.textField._visible = false;
				mapMarker.visible = false;
				if (MarkerData[j + CREATE_UNDISCOVERED] && mapMarker.IconClip != undefined)
				{
					var depth:Number = mapMarker.IconClip.getNextHighestDepth();
					mapMarker.IconClip.attachMovie(Map.MapMarker.IconTypes[MarkerData[j + CREATE_ICONTYPE]] + "Undiscovered", "UndiscoveredIcon", depth);
				}
				++i;
				++NextCreateIndex;
				j = j + CREATE_STRIDE;
			}

			if (NextCreateIndex >= markersLen)
			{
				NextCreateIndex = -1;
			}
		}

		var mapMarker:MapMarker = Markers[0];

		testTf.text = "CreateMarkers: markersLen="+ markersLen + " " + mapMarker.Index + ": " + mapMarker.textField;
	}

	function RefreshMarkers():Void
	{
		var i:Number = 0;
		var j:Number = 0;
		var markersLen:Number = Markers.length;
		var dataLen:Number = MarkerData.length;
		while (i < markersLen && j < dataLen)
		{
			var marker:MapMarker = Markers[i];
			marker._visible = MarkerData[j + REFRESH_SHOW];
			if (marker._visible)
			{
				marker._x = MarkerData[j + REFRESH_X] * MapWidth;
				marker._y = MarkerData[j + REFRESH_Y] * MapHeight;
				marker._rotation = MarkerData[j + REFRESH_ROTATION];
			}
			++i;
			j = j + Map.MapMenu.REFRESH_STRIDE;

			testTf.text = "RefreshMarkers MapDim" + MapWidth + ", " + MapHeight + "  index: " + i + " ";
			if (marker._visible)
			{
				testTf.text += "x: " + marker._x + " y: " + marker._y;
			}
		}
		if (SelectedMarker != undefined)
		{
			MarkerDescriptionHolder._x = SelectedMarker._x + MarkerContainer._x;
			MarkerDescriptionHolder._y = SelectedMarker._y + MarkerContainer._y;
		}
	}

	function SetSelectedMarker(aiSelectedMarkerIndex:Number):Void
	{
		var marker:MovieClip = aiSelectedMarkerIndex < 0 ? undefined : Markers[aiSelectedMarkerIndex];
		if (marker != SelectedMarker)
		{
			if (SelectedMarker != undefined)
			{
				SelectedMarker.MarkerRollOut();
				SelectedMarker = undefined;
				MarkerDescriptionHolder.gotoAndPlay("Hide");
			}
			if (marker != undefined && marker.visible && marker.MarkerRollOver())
			{
				SelectedMarker = marker;
				MarkerDescriptionHolder._visible = true;
				MarkerDescriptionHolder.gotoAndPlay("Show");
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
