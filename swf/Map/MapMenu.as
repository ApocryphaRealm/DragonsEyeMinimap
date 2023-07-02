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

	static var EXTRA_REFRESH_X:Number = 0;
	static var EXTRA_REFRESH_Y:Number = 1;
	static var EXTRA_REFRESH_STRIDE:Number = 2;
	static var EXTRA_CREATE_NAME:Number = 0;
	static var EXTRA_CREATE_ICONTYPE:Number = 1;
	static var EXTRA_CREATE_STRIDE:Number = 2;

	var LocalMapMenu:LocalMap;
	var MapHeight:Number;
	var MapMovie:MovieClip;
	var MapWidth:Number;
	var MarkerContainer:MovieClip;
	var MarkerData:Array;
	var ExtraMarkerData:Array;
	var MarkerDescriptionHolder:MovieClip;
	var MarkerDescriptionObj:MovieClip;
	var Markers:Array;
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

		Map.MapMarker.TopDepth = aNumMarkers;
		SetSelectedMarker(-1);
	}

	function CreateMarkers():Void
	{
		if (MarkerContainer != undefined)
		{
			var i:Number = 0;
			var dataLen:Number = MarkerData.length;
			var extraDataLen:Number = ExtraMarkerData.length;

			delete Markers;
			Markers = new Array(dataLen / CREATE_STRIDE + extraDataLen / EXTRA_CREATE_STRIDE);

			for (var j:Number = 0; j < dataLen; j = j + CREATE_STRIDE)
			{
				var mapMarker:MovieClip = MarkerContainer.attachMovie(MapMarker.IconTypes[MarkerData[j + CREATE_ICONTYPE]], "Marker" + i, i);
				Markers[i] = mapMarker;
				if (MarkerData[j + CREATE_ICONTYPE] == PlayerLocationMarkerType)
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
				var mapMarker:MovieClip = MarkerContainer.attachMovie(MapMarker.ExtraIconTypes[ExtraMarkerData[j + EXTRA_CREATE_ICONTYPE]], "Marker" + i, i);
				Markers[i] = mapMarker;
				mapMarker.Index = i;
				mapMarker.label = ExtraMarkerData[j + EXTRA_CREATE_NAME];
				mapMarker.textField._visible = false;
				mapMarker.visible = true;
				++i;
			}
		}

		var mapMarker:MapMarker = Markers[0];
		var markersLen:Number = Markers.length;

		testTf.text = "CreateMarkers: markersLen="+ markersLen + " " + mapMarker.Index + ": " + mapMarker.textField;
	}

	function RefreshMarkers():Void
	{
		var i:Number = 0;
		var dataLen:Number = MarkerData.length;
		var extraDataLen:Number = ExtraMarkerData.length;

		var markersLen:Number = Markers.length;

		for (var j:Number = 0; j < dataLen; j = j + REFRESH_STRIDE)
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

			testTf.text = "m: " + markersLen + " i: " + dataLen + " xi: " + extraDataLen + " ";
			if (marker._visible)
			{
				testTf.text += "x: " + marker._x + " y: " + marker._y;
			}
		}

		for (var j:Number = 0; j < extraDataLen; j = j + EXTRA_REFRESH_STRIDE)
		{
			var marker:MapMarker = Markers[i];
			marker._visible = true;
			if (marker._visible)
			{
				marker._x = ExtraMarkerData[j + EXTRA_REFRESH_X] * MapWidth;
				marker._y = ExtraMarkerData[j + EXTRA_REFRESH_Y] * MapHeight;
			}
			++i;
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
