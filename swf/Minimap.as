var All:Boolean;
var StealthMode:Boolean;
var Swimming:Boolean;
var HorseMode:Boolean;
var WarHorseMode:Boolean;

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
}

function AddToHudElements():Void
{
	_level0.HUDMovieBaseInstance.HudElements.push(this);
}
