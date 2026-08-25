// HUD mode flags. HUDMovieBaseInstance keeps a stack of modes and decides whether an element
// is drawn with hasOwnProperty(mode) - the property only has to EXIST, its value is irrelevant,
// and only `delete` removes it. An element with no property for the active mode is hidden.
//
// Favor and SleepWaitMode were missing, so the minimap disappeared while commanding a follower
// and while waiting. The rest of the seventeen modes are deliberately absent: they are menus and
// screens the minimap should not be drawn over.
var All:Boolean;
var StealthMode:Boolean;
var Swimming:Boolean;
var HorseMode:Boolean;
var WarHorseMode:Boolean;
var Favor:Boolean;
var SleepWaitMode:Boolean;

var updateScaleform:Boolean = true;

function Minimap(a_positionX:Number, a_positionY:Number):Void
{
	var positionX0 = Stage.width * a_positionX;
	var positionY0 = Stage.height * a_positionY;

	var point:Object = { x:positionX0, y:positionY0 };
	globalToLocal(point);
	_x = point.x;
	_y = point.y;

	All = true;
	StealthMode = true;
	Swimming = true;
	HorseMode = true;
	WarHorseMode = true;
	Favor = true;
	SleepWaitMode = true;
}

// Set our own visibility from the current HUD mode, instead of registering and waiting to be
// told. This is what SkyUI's WidgetBase does at registration time, and it is the one line this
// clip was missing: HUDModes starts EMPTY, so an element that registers before the first mode is
// pushed never receives a visibility decision and can sit hidden until something else moves the
// stack. That is the startup flicker the C++ side has been re-asserting visibility to paper over.
function SyncVisibilityToHudMode():Void
{
	var hud:Object = _level0.HUDMovieBaseInstance;

	if (hud == undefined || hud.HUDModes == undefined)
	{
		return;
	}

	var mode:String = "All";

	if (hud.HUDModes.length > 0)
	{
		mode = hud.HUDModes[hud.HUDModes.length - 1];
	}

	this._visible = (mode == "All") || this.hasOwnProperty(mode);
}

function AddToHudElements():Void
{
	_level0.HUDMovieBaseInstance.HudElements.push(this);
	SyncVisibilityToHudMode();
}

// For some reason, when casting a rune, this will be removed from the HUD elements list,
// make sure that "this" is always on that list.
function onEnterFrame():Void
{
	var hudElements:Array = _level0.HUDMovieBaseInstance.HudElements;
	var hudElementsLen:Number = hudElements.length;
	for (var i:Number = 0; i < hudElementsLen; i++)
	{
		if (hudElements[i] == this)
		{
			return;
		}
	}

	hudElements.push(this);

	// Only after a re-registration, not every frame - deliberately. Forcing visibility on every
	// frame is what 1.3.2 did, and it made the minimap sit on top of the world map refusing to
	// go away. Visibility belongs to the HUD; this only ensures we are told once we rejoin it.
	SyncVisibilityToHudMode();

	updateScaleform = true;
}
