*navigation

There needs to be a cursor for inserting in between numbers in a pattern, and inserting/wrapping nodes
Shift>Insert should go all the way to the top of the tree for that block
up and down when at the rrot should move up and down blocks

*Structure

There needs to be possibility for more than one block for the same dN
Insert>(pad(N)) should be used to add a new block for dN above the current one when at the root

*screen buttons

These launch menus for inserting/wrapping functions
F1 $
F2 #
F3 []
F4 <>
F5 {}
F6 ()
F7 |
F8 |+

*other buttons
Note - jump to n pattern section of that block
Instrument - jump to s pattern section of that block
FX1 - menu with all pattern modifiers
FX2 - menu with all FX modifiers
Pattern - evaluate block
Song - menu with all transition functions e.g. jumpMod

*Notes

setcps - can't selct '4' from '130/60/4'
setcps - shift>insert should go to top level and allow navigation to next blocks. up from the first block should get to setcps

on a single number, {} [] etc should automatically insert ""
selecting 'often' etc should allow at least 1 of the menus to list the available selections for replacements

*PolyendTrac_TidalEditor1

text is garbled, all uppercase letters, and puntuation and numbers all wrong

menus are drawing under the main code

menus seem responsive

evaluating mute and hush all work

shift-pad works

*PolyendTrac_TidalEditor2

making an empty slot with shift padN looses the cursor, so  no way to start a new block.

note and sound entry with pads works

should change the note entry now that we have cursor spaces beweeen numbers - a pad entry can just move to the curser space after the inserted note, so no need to hit del at the end

dl N B is always present even with menus

menus draw on top ok now


*PolyendTrac_TidalEditor3

Instrument parameters button needs to be kept free. currently opens comments. that should be shift>file

currently no way to make chords. what do you suggest?

at the lowest level, left and right should make the curser should continue navigating across the code

*PolyendTrac_TidalEditor7

 highlighting works, steps light at the right time, even when editing

 the steps seem to light for too long, e.g. d5 in the test pattern, 2 of the chord steps light at a time, old one doesn't unlight when the next one plays

 status view shows fine although one on the lines extends off the end of the menu box slightly

 bug - Loading a new sample to the tracker's list doesn't add it to the list of available samples in the editor

 feature request - shift arrows should in mini-notation pattern should select a group of numbers to be able to wrap them in [] etc

 *PolyendTrac_TidalEditor8

 highlights work as they should

 status looks ok, could be slightly smaller, or even better would be to replace the info box above the screen buttons with status

 selecting steps now works

feature request - there's currently no way to add something at the front of a mininotation pattern. e.g. I'd like to be able to change
n "0 1 2 3 4 5 6 7"
to
n "0,7,10" +| "0 1 2 3 4 5 6 7"

feature request - cursor should be able to select # etc to change it to |< |+ etc

 *PolyendTrac_TidalEditor9

 status works better here, nice one

operator select and change works well now

place in front works

bug - didn't notice before, but highlighting on d1 and d2 misses some highlights unless d3 is running

feature request - can we lower the brightness of the cursor box and the event highlighting to increase contrast with the white code text

*PolyendTrac_TidalEditor10

this is very good! excellent work

feature request (park for now) - would be good to be able to toggle the cursor info box, tidal title bar, d1-d16 indicators on the left > done through the config menu if possible.

*PolyendTrac_TidalEditor11

typo in project.tidal gives parse error but the text overlaps on the status strip

save and reload works

new project empty editor works

bug - load a new project with samples already loaded, start a new block, select the s pattern numbers, try to enter a sound using wheel or pads > "606-kick doesn't fit here"


*PolyendTrac_TidalEditor12

OK that worked

had one crash, somehow got into a position with an isolated block that had no code and no (empty ...) message. when i evaluated it > nothing to evaluate and system hangs

space and brackets works too



*PolyendTrac_TidalEditor13

changes work well

change request > can we change how | is inserted - if the cursor is in a gap, options should be to wrap the neighboring values in [ | ], or add | between all values in that step. if several notes are selected, automatically wrap [] with | between


*PolyendTrac_TidalEditor14

shift>up and shift>down should navigate between code blocks

left when all the way at the left of the code should move up the tree. right when at the right of the code should move down the tree

when a code block contents is deleted, shouldn't leave ??. should go back to newly created block state

shift>padN makes a code block, but can't navigate away until something is added. up/down should clear the empty block

shift>padN goes to the block, but shift>padN again should create a new dN block. shift>padN>copy should duplicate the selected dN block

status bar at the bottom should be slimmer so more room for code

floating screen buttons should be slimmer too

remove the 'tidal' title bar it takes up space and brings no value

remove the d1-d16 indicators too - the code shows what's there. replace with slim scroll bar

add a way to split the code lines for ease of reading. replace shift>screen4 with ↩ symbol if possible or <CR> if not.

rename project.tidal with the saved project name when it's saved


*PolyendTrac_TidalEditor18

folder importing when lots of things playing - menu won't load with 8 patterns playing, with 6 playing can just open but is sluggish. loading sitar caused audio hangs but eventually loaded


*PolyendTrac_TidalEditor22

very close now!

status bar is slighly too slim, curs off the bottom 4 pixels of the font on the top line

status bar should be same colour as the screen buttons

highlight strip is redundant. can see where the cursor is without it so get rid to keep the screen as dark as possible.

I think I wasn't clear about the left/right behaviour. I want L / R to navigate up / down the tree within the block, but never to move up or down the code blocks themselves. only up/down or shift+Up/Down should
