import gfx.io.GameDelegate;
import Map.Display;

class Map.LocalMap extends MovieClip
{
	/* Constants */
	private static var SHAPE_SQUARED:Number = 0;
	private static var SHAPE_ROUND:Number = 1;

	/* Stage elements */
	public var LocationName:TextField;
	public var ClearedHint:TextField;
	public var LocalMapHolder:MovieClip;
	public var BackgroundArtSquare:MovieClip;
	public var BackgroundArtCircle:MovieClip;
	public var VisionCone:MovieClip;

	public var Test:TextField;

	/* API */
	public var IconDisplay:Display;

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

		IconDisplay = new Display(this);

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
	public function SetShape(a_shape:Number):Void
	{
		var backgroundArt:MovieClip = a_shape == SHAPE_ROUND ? BackgroundArtCircle : BackgroundArtSquare;
		var backgroundArtMask:MovieClip = backgroundArt.duplicateMovieClip("backgroundArtMask", getNextHighestDepth());
		VisionCone.setMask(backgroundArtMask);

		var altBackgroundArt:MovieClip = a_shape == SHAPE_ROUND ? BackgroundArtSquare : BackgroundArtCircle;
		altBackgroundArt._visible = false;

		IconDisplay.SetShape(a_shape);
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
