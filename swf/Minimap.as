// HUD mode flags. HUDMovieBaseInstance keeps a stack of modes and decides whether an element
// is drawn with hasOwnProperty(mode) - the property only has to EXIST, its value is irrelevant,
// and only `delete` removes it. An element with no property for the active mode is hidden.
//
// Favor was missing, so the minimap disappeared while commanding a follower - that is ordinary
// gameplay and the map should stay up. SleepWaitMode is deliberately NOT declared: sleeping and
// waiting are not gameplay, and the author's call is that the minimap should go away for them. The
// rest of the seventeen modes are absent for the same reason - they are menus and screens the
// minimap has no business being drawn over.
//
// DialogueMode in particular is absent DELIBERATELY and should stay that way. The minimap
// disappearing while you talk to someone is the behaviour the author wants, confirmed in testing -
// it is not a bug to be fixed by declaring the flag.
var All:Boolean;
var StealthMode:Boolean;
var Swimming:Boolean;
var HorseMode:Boolean;
var WarHorseMode:Boolean;
var Favor:Boolean;

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

	// Only ever turn visibility ON. Never off.
	//
	// 1.3.6 set _visible from this test in both directions, and that was wrong: the clip
	// re-registers whenever the HUD drops it, so any mode this clip does not declare turned the
	// minimap off again a moment after the player turned it on. the author hit exactly that - invisible
	// at startup, visible after toggling Show, invisible again after doing anything.
	//
	// Hiding is the HUD's job and it already does it through ShowElements. The only thing this
	// needs to fix is the opposite case: HUDModes starts EMPTY, so a clip that registers before
	// the first mode is pushed never gets told anything and can sit hidden forever. Turning
	// visibility on when the current mode permits it fixes that and cannot cause the regression.
	if ((mode == "All") || this.hasOwnProperty(mode))
	{
		this._visible = true;
	}
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
	// Every frame, before the registration check - not only when the clip is re-added.
	//
	// The HUD can hide this element WITHOUT dropping it from HudElements, in which case the
	// re-registration path below never runs and nothing notices. the author saw exactly that: visible
	// on load, then invisible the moment he moved or looked around, permanently.
	//
	// This is safe to run every frame precisely because it is mode-aware and one-directional. In
	// WorldMapMode or any other mode this clip does not declare, the test fails and it does
	// nothing - so it cannot reproduce 1.3.2's bug of sitting on top of the world map. It only
	// restores visibility in modes where the minimap is supposed to be visible anyway.
	SyncVisibilityToHudMode();

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

	updateScaleform = true;
}
