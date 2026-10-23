# MiniMap

Version 1.7.5

WHAT CHANGED
------------

Version 1.7.5
A missing or wrong Address Library now shows the mod's own message naming the file it needs, instead of a bare error box from the library the mod is built on. No change to how the minimap works.

Version 1.7.4
The DLL's file properties now name its real licence, GPL-3.0-or-later, instead of MIT.

Version 1.7.1
Removed the frame theme system: the Frame theme dropdown, the sTheme setting and the DragonsEyeMinimapThemes folder are gone. The minimap's look now comes from whichever Minimap.swf and MinimapArt.swf win in your load order; a UI overhaul that ships restyled copies of them, such as Norden UI's Dragon's Eye Minimap option, goes above this mod.

Version 1.6.6
Fixed the map drawing mangled, scaled or skewed inside interiors. The camera has a limit on how far it may zoom in, and nothing was setting it, so a small room was framed by a view many times its size. The limit now follows the area being drawn whenever that area is the smaller of the two, which leaves outdoor maps exactly as they were. This is also why opening the map menu put the map right until the next door you walked through.
Added a one-off diagnostic line for a second part of the same report, where the map often does not draw in interiors at all. It records what the game's scene looks like the first time the mod decides the world is not ready to be drawn, so the cause can be identified rather than guessed at.

Version 1.6.5
The settings page is shown in the game's language: Japanese, Korean, Chinese, Russian, German, French, Spanish, Italian, Polish and Czech translation files ship beside the DLL, and the page follows the Apocrypha Menu Framework's Language setting; English is the fallback.
Version 1.6.4
Fixed the mod stopping the game from starting on Skyrim AE 1.6.1130 and newer, including 1.6.1170; a value it read from the game no longer exists in those versions and it was never actually used, so it is gone.
Made the map wait for the surrounding area to finish building before it draws the world, not just for a fixed moment after a load, so it can no longer draw a scene the game is still assembling.
Version 1.6.3
Reworked the theme system. A theme now replaces the minimap's frame artwork instead of recolouring it, so a theme can change the frame's shape and ornament rather than just its hue.
Themes are SWF files in Data/Interface/DragonsEyeMinimapThemes. Drop one in and it appears in the Frame theme dropdown on the next game start; "Built-in frame" uses the artwork the mod ships.
Removed the four colour-only themes and the frame tint setting they used. They carried a colour, and a theme is now artwork, so they could not be converted.
Frame reskin mods are unaffected and keep working exactly as before - they replace the built-in artwork, which is what "Built-in frame" shows.
Fixed the map markers lagging behind the map when you turn quickly, which made them look like they detached from it and then snapped back into place.
Stopped the map redrawing the world while the game is loading a new area, so it no longer walks a scene the game is still building.
Added a Rendering section to the INI that sets how often the map redraws the world, for anyone who wants that done less than once a frame.
Version 1.6.2
Added a Show location name toggle on the settings page. Turn it off if your game's language shows missing characters or text running past the frame under the map - the title uses the game's own interface font, which this mod cannot change.
The minimap now opens already zoomed in, instead of at whatever zoom the game's camera happened to be at.
Changed the two zoom levels the zoom key alternates between to fully out and fully in.
Version 1.6.1
Fixed the tween menu flickering when you tap Tab, which also made the minimap appear to flicker with it. The minimap was claiming every key press while no menu was open, including keys it has nothing to do with; it now responds only to its own hide key, zoom key and controller button.
Version 1.6.0
Added minimap themes. A Theme dropdown on the settings page recolours the minimap's frame; drop a theme file into the mod's themes folder to add your own. Ships four themes: Vanilla White (untinted), Untarnished Ivory, Ebony Gold, Njord Ice.
Version 1.5.9
Fixed every keybind and controller bind sometimes being dead on startup - an input handler's internal registration flag could start in a state that skipped hooking up input entirely. It is now initialised properly every time.
Fixed hold-to-pan never actually panning, on mouse or controller - the camera was recentring itself every frame, wiping the pan movement as it was made. Panning works now, and the map recentres on you when you release the button.
The controller hold-to-pan button now defaults to the left bumper instead of the right stick click.
Added compass controls to the settings page: switch the compass ring, the quest pointer and metric units on or off in game (previously INI-only).
The compass is restyled to the compact design from the mod's original screenshots: a thinner ring, smaller letters and pointer, and smoother circles.
Version 1.5.8
Added a built-in compass. While the minimap is hidden, its corner shows a compass ring instead: cardinal letters, tick marks and a rotating backing disc. A quest pointer rides the ring toward the nearest objective with a distance readout, and rides the visible map's inscribed circle while the minimap is shown. Every piece can be switched off. INI-only in this version.
Hold-to-pan is back on the hide key: tap toggles, holding pans the map with the mouse, and the wheel zooms.
Added optional controller support, off by default: holding a configurable button pans with the right stick.
Fixed the settings menu not registering under Apocrypha Menu Framework.
Fixed numeric INI values not parsing hex correctly.
Debug symbols now ship inside the main download so Crash Logger can resolve this mod's crash frames.
Version 1.5.7
Fixed settings reverting to their values from game start on reload, and being overwritten between sessions, when PrivateProfileRedirector is installed.
Fixed the INI path shown at the bottom of the settings page appearing dim and hard to read.
Version 1.5.6
Fixed hotkeys firing while a menu or the console is open.
Made the settings page text readable throughout.
The published 1.5.6 download was built before these fixes were finished and does not contain them; take 1.5.7 instead.
Version 1.5.2
Removed a leftover diagnostic-only startup log line that had stayed in the build after the issue it was added for was already fixed.
Version 1.5.1
Reverted an arrow-key change from the previous build that stopped a slider nudge from also moving gamepad menu navigation elsewhere on screen. Sliders can still be nudged with all four arrow keys.
Changed the on/off toggle switch to turn green when on and red when off.
Version 1.4.8
Removed leftover workaround code that an earlier fix made unnecessary, for noticeably fewer Scaleform calls per frame while the HUD is open. No change in behaviour.
Version 1.4.7
Fixed the flicker and disappearing-minimap problem, including reports under HUD-management mods like ImmersiveHUD, moreHUD and TrueHUD. The minimap's HUD visibility flags were declared but never actually assigned, so the game's own HUD manager hid the minimap on every visibility check it performed.
Confirmed compatible with ImmersiveHUD, moreHUD and TrueHUD, all three tested enabled together with no issue.
Version 1.4.5
Reduced the flicker by restoring the minimap's visibility in the same frame it was lost rather than one frame late. A workaround, superseded by the real fix in 1.4.7.
Version 1.4.3
Attempted fix for the disappearing minimap by re-syncing its visibility to the HUD's own mode every frame. This stopped the minimap disappearing for good but introduced visible flicker as a side effect, addressed in 1.4.5 and properly fixed at the root in 1.4.7.
Version 1.4.2
The hide key and the zoom-toggle key now ship pre-bound to K and L instead of unbound. Existing INIs keep whatever you already had bound; the new defaults only apply to a fresh install or after Restore defaults.
Version 1.4.1
Fixed the hide key only working one way - it could hide the minimap, but once hidden the key stopped responding. It is a proper two-way toggle now.
The startup visibility fix now waits for visibility to hold steady for about a second before standing down, instead of standing down the first time it merely looked successful.
Version 1.3.9
Fixed a regression where the minimap could turn itself back off shortly after being shown, because a HUD-mode sync added the same cycle ran in both directions instead of only ever turning visibility on.
Version 1.3.7
The minimap now stays up while commanding a follower, matching how it already behaved for dialogue, but hides properly for sleeping and waiting.
Version 1.3.6
Fixed the minimap being able to start invisible, or stay invisible until something else happened to make it show. The HUD only tells an element whether it should be visible at the moment that element registers with it, and the minimap was not listening for that.
Version 1.3.5
Fixed a startup log line describing entirely normal behaviour being logged as a warning, which could look like a fault when nothing was wrong.
Version 1.3.4
Brighter, cleaner markers on the minimap. They were pale, semi-transparent and faded toward their edges, which made them hard to pick out against the map; they are now fully opaque, vividly coloured and evenly filled with hard edges. Each marker keeps its own colour.
If you also use Local Map Upgrade, the same change landed there in 1.1.2, so the local map and the minimap now match.
Version 1.3.3
Fixed the minimap staying on screen on top of the world map and local map. The previous Anniversary Edition fix kept the minimap visible too eagerly, overriding the game hiding the HUD.
Version 1.3.2
Fixed the minimap being invisible on Anniversary Edition. On AE it was created, positioned and fully configurable, but the clip it draws into was never made visible, so nothing ever appeared.
If you are on Anniversary Edition and this mod seemed to do nothing, this is the version that fixes it. Tested working on 1.6.1170.
Version 1.3.0
Fixed the minimap loading slightly off screen on a fresh install. After a load, the map artwork keeps resizing for a few seconds while the map texture arrives; the mod now keeps re-adjusting until the position actually stops moving, instead of stopping after a fixed number of attempts.
Version 1.2.7
Fixed the minimap ignoring the game's HUD Opacity slider. The setting was being looked up under the wrong name, so the background always stayed fully opaque.
Every log now opens by stating what detail level it is recording at.
Logging now defaults to its most detailed level, so a first bug report is useful without reproducing the problem again.
Version 1.2.6
Fixed markers never appearing on the minimap. The mod was checking for the marker layer in a way that could never succeed, so no actor, location or quest marker had ever been drawn on the minimap in any previous version, whatever your settings said.
Version 1.2.5
Fixed the minimap growing and sliding across the screen for a moment after loading, instead of simply appearing where you put it. It now settles in a single step before anything is drawn.
If you are on 1.1.7, take this version - it also carries the settings-saving fix, the minimap marker fix and the HUD Opacity fix from 1.2.1 through 1.2.4, none of which are in 1.1.7.
Version 1.2.4
Fixed the game failing to start with a message that Local Map Upgrade did not provide the interface this mod needs. Local Map Upgrade was fine; a check added in 1.2.3 ran before Local Map Upgrade had a chance to answer, so it could never pass.
Do not use 1.2.3, it will not start. Use 1.2.4 or newer.
Version 1.2.3
Fixed markers never appearing on the minimap. The mod looked for the marker layer once while the interface was still being built, and gave up for the rest of the session if it was not there yet.
Fixed the player direction arrow never updating, which had the same cause.
Fixed the minimap ignoring the game's HUD Opacity slider, which was being read from the wrong section of SkyrimPrefs.ini.
Version 1.2.2
Compatibility: this mod no longer refuses to load based on the version number Local Map Upgrade reports. It now checks whether Local Map Upgrade actually provides the interface it needs, so any compatible build works whatever version it calls itself.
Fixed a crash that could happen at startup if Local Map Upgrade was detected but its plugin information could not be read.
Version 1.2.1
Fixed settings never being saved. Pressing Save reported success but wrote nothing to disk, so every change was lost the next time you loaded the game. This affected every previous version; set your preferences once more after updating.
Fixed the same problem in the separate save that remembers whether the minimap starts visible.
Version 1.2.0
Full safety audit of the whole plugin. Found and fixed a real crash risk where three internal hooks could run before the minimap existed.
Version 1.1.9
Fixed a crash reading the game's HUD Opacity setting. If the setting is missing, the minimap now stays fully visible instead of crashing.
Added detailed diagnostic logging throughout.
Version 1.1.8
The minimap background and frame now follow the game's own HUD Opacity slider instead of ignoring it.
Version 1.1.7
Fixed the minimap still appearing slightly out of position on some loads. Its position is now re-applied over several frames after loading rather than only once.
Version 1.1.6
Position is now measured from the visible map frame rather than the whole clip, so the minimap sits where you would expect.
Version 1.1.5
Removed the on-screen position clamp so the minimap can be placed freely, and added a re-apply after the first frame.
Version 1.1.4
Each screen corner now remembers its own position offset.
Version 1.1.3
The location name is placed on the opposite side to the screen edge the minimap is anchored to, so it no longer runs off screen.
Version 1.1.2
Hid the leftover on-screen control prompt, which was visible by default.
Version 1.1.1
Fixed positioning and zoom, and removed the old hold-to-control and control-tip system in favour of dedicated keys.
Version 1.0.9
Fixed a crash on startup introduced in 1.0.1.
Version 1.0.8
The minimap sits flush in the corner by default.
Version 1.0.7
The minimap is capped at a quarter of the screen.
Version 1.0.6
Removed the old x/y offsets and kept the map on screen as it scales.
Version 1.0.5
Added dedicated press-to-hide and press-to-zoom keys.
Version 1.0.4
A separate offset pair for each screen corner.
Version 1.0.3
Position the minimap by corner and pixel offset.
Version 1.0.2
Added arrow-key adjustment on sliders, and map zoom control in the menu.
Version 1.0.1
The position is computed rather than fixed, shape switching fixed, typed numeric input and a hide key added.
Version 1.0.0
First release. Adds an in-game settings page through SKSE Menu Framework, so the minimap can be configured while playing instead of only by editing the INI before launching.
