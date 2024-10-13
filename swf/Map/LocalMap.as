import gfx.io.GameDelegate;
import Map.MapMenu;

class Map.LocalMap extends MovieClip
{
	/* Stage elements */
	public var LocationName:TextField;
	public var ClearedHint:TextField;
	public var LocalMapHolder:MovieClip;
	public var VisionCone:MovieClip;
	public var Test:TextField;

	/* API */
	public var IconDisplay:MapMenu;

	/* Properties */
	public var textureHeight:Number;
	public var textureWidth:Number;
	private var mapTextureLoader:MovieClipLoader;
	private var isShown:Boolean;

	function LocalMap()
	{
		super();

		Test = _parent.Test;

		textureWidth = LocalMapHolder._width;
		textureHeight = LocalMapHolder._height;

		IconDisplay = new MapMenu(this);

		mapTextureLoader = new MovieClipLoader();
		mapTextureLoader.addListener(this);

		LocationName.noTranslate = true;
		ClearedHint.noTranslate = true;
	}

	// For mapTextureLoader after calling `MovieClipLoader.loadClip'
	function onLoadInit(a_targetClip:MovieClip):Void
	{
		a_targetClip._width = textureWidth;
		a_targetClip._height = textureHeight;
	}

	/* API */
	public function InitMap():Void
	{
		mapTextureLoader.loadClip("img://Local_Map", LocalMapHolder);

		var textureTopLeft:Object = {x: LocalMapHolder._x, y: LocalMapHolder._y};
		var textureBottomRight:Object = {x: textureTopLeft.x + textureWidth, y: textureTopLeft.y + textureHeight};
		_parent.localToGlobal(textureTopLeft);
		_parent.localToGlobal(textureBottomRight);

		GameDelegate.call("SetLocalMapExtents", [textureTopLeft.x, textureTopLeft.y, textureBottomRight.x, textureBottomRight.y]);
	}

	/* API */
	public function Show(a_show:Boolean):Void
	{
		if (a_show != isShown)
		{
			_parent.gotoAndPlay(a_show ? "fadeIn" : "fadeOut");
			isShown = a_show;
			if (isShown)
			{
				_level0.HUDMovieBaseInstance.LocationLockBase._alpha = 0.0;
			}
			else{
				_level0.HUDMovieBaseInstance.LocationLockBase._alpha = 1.0;
			}
		}
	}

	/* API */
	public function SetTitle(a_name:String, a_cleared:String):Void
	{
		LocationName.text = a_name == undefined ? "" : a_name;
		ClearedHint.text = a_cleared == undefined ? "" : "(" + a_cleared + ")";
	}
}
