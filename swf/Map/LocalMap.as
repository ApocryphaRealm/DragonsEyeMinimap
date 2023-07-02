import gfx.io.GameDelegate;
import Map.MapMenu;

class Map.LocalMap extends MovieClip
{
	/* Stage elements */
	public var LocationName:TextField;
	public var ClearedHint:TextField;
	public var LocalMapHolder:MovieClip;

	/* API */
	public var IconDisplay:MapMenu;

	private var mapImageLoader:MovieClipLoader;
	private var textureHeight:Number;
	private var textureWidth:Number;
	private var bUpdated:Boolean;

	private var isShown:Boolean;

	var TestTf:TextField;

	function LocalMap()
	{
		super();

		TestTf.text = "Hello from local map";

		textureWidth = LocalMapHolder._width;
		textureHeight = LocalMapHolder._height;

		IconDisplay = new MapMenu(this);

		mapImageLoader = new MovieClipLoader();
		mapImageLoader.addListener(this);

		LocationName.noTranslate = true;
		ClearedHint.noTranslate = true;
	}

	function get TextureWidth():Number
	{
		return textureWidth;
	}

	function get TextureHeight():Number
	{
		return textureHeight;
	}

	// For mapImageLoader after calling `MovieClipLoader.loadClip'
	function onLoadInit(a_targetClip:MovieClip):Void
	{
		a_targetClip._width = textureWidth;
		a_targetClip._height = textureHeight;
	}

	function onEnterFrame():Void
	{
		GameDelegate.call("UpdateOnEnterFrame", []);
	}

	function InitMap():Void
	{
		if (!bUpdated)
		{
			mapImageLoader.loadClip("img://Local_Map", LocalMapHolder);
			bUpdated = true;
		}
		var textureTopLeft:Object = {x: _x, y: _y};
		var textureBottomRight:Object = {x: _x + textureWidth, y: _y + textureHeight};
		_parent.localToGlobal(textureTopLeft);
		_parent.localToGlobal(textureBottomRight);
		GameDelegate.call("SetLocalMapExtents", [textureTopLeft.x, textureTopLeft.y, textureBottomRight.x, textureBottomRight.y]);
	}

	function Show(a_show:Boolean):Void
	{
		if (a_show != isShown)
		{
			_parent.gotoAndPlay(a_show ? "fadeIn" : "fadeOut");
			isShown = a_show;
		}
	}

	function SetTitle(a_name:String, a_cleared:String):Void
	{
		LocationName.text = a_name == undefined ? "" : a_name;
		ClearedHint.text = a_cleared == undefined ? "" : "(" + a_cleared + ")";
	}

}
