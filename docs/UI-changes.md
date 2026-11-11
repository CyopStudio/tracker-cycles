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

 *PolyendTrac_TidalEditor7

 highlights work as they should

 status looks ok, could be slightly smaller, or even better would be to replace the info box above the screen buttons with status

 selecting steps now works

feature request - there's currently no way to add something at the front of a mininotation pattern. e.g. I'd like to be able to change
n "0 1 2 3 4 5 6 7"
to
n "0,7,10" +| "0 1 2 3 4 5 6 7"

feature request - cursor should be able to select # etc to change it to |< |+ etc
