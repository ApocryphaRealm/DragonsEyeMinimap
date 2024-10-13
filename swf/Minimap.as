var All:Boolean;
var StealthMode:Boolean;
var Swimming:Boolean;
var HorseMode:Boolean;
var WarHorseMode:Boolean;

var hudElements:Array;

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

	var hudElements:Array = _level0.HUDMovieBaseInstance.HudElements;
}

function AddToHudElements():Void
{
	hudElements.push(this);
}

// For some reason, when casting a rune, this will be removed from the HUD elements list, make sure that we re add it
function onEnterFrame():Void
{
	var hudElementsLen:Number = hudElements.length;
	for (var i:Number = 0; i < hudElementsLen; i++)
	{
		if (hudElements[i] == this)
		{
			return;
		}
	}

	AddToHudElements();

}
