.. _Playing the Game:

================
Playing the Game
================

Most of your interaction with NarSil will take the form of "commands".
Every NarSil command consists of an "underlying command" plus a variety of
optional or required arguments, such as a repeat count, a direction, or the
index of an inventory object. Commands are normally specified by typing a
series of keypresses, from which the underlying command is extracted, along
with any encoded arguments. You may choose how the standard "keyboard keys"
are mapped to the "underlying commands" by choosing one of the two standard
"keysets", the "original" keyset or the "roguelike" keyset.

The original keyset is very similar to the "underlying" command set, with a
few additions (such as the ability to use the numeric "directions" to
"walk" or the ``5`` key to "stay still"). The Angband-like keyset provides
similar additions, and also uses keys more familiar to Angband players
(like ``v`` to throw rather than ``t``). Setting the hjkl-movement option
allows the use of the ``h``/``j``/``k``/``l``/``y``/``u``/``b``/``n`` keys to
"walk" (or, in combination with the shift key, to run), which thus
requires a variety of key mappings to allow access to the underlying
commands used for walking/running/altering. This will involve many more
"capital" and "control" keys.

Note that any keys that are not required for access to the underlying
command set may be used by the user to extend the "keyset" which is being
used, by defining new "keymaps". To avoid the use of any "keymaps", press
backslash (``\``) plus the "underlying command" key. You may enter
"control-keys" as a caret (``^``) plus the key (so ``^`` + ``p`` yields
'^p').

Some commands allow an optional "repeat count", which allows you to tell
the game that you wish to do the command multiple times, unless you press a
key or are otherwise disturbed. To enter a "repeat count", type ``R`` if
you are using the original keyset or ``0`` if you are using the Angband-
like keyset, followed by the numerical count, followed by the command.  You
must type 'space' before entering certain commands. Skipping the numerical
count yields a count of 99 for the open, tunnel, disarm, bash, alter, and
close commands. All other commands do not repeat unless requested.

Some commands will prompt for extra information, such as a direction, an
inventory or equipment item, a spell, a textual inscription, the symbol of
a monster race, a sub-command, a verification, an amount of time, a
quantity, a file name, or various other things. Normally you can hit return
to choose the "default" response, or escape to cancel the command entirely.

Some commands will prompt for an inventory item. Pressing space
(or ``*``) will give you a list of choices. Pressing ``-`` (minus) selects
the item on the floor. Pressing a lowercase letter selects the given item.
Pressing a capital letter selects the given item after verification.
Pressing a numeric digit ``#`` selects the first item (if any) whose
inscription contains '@#' or '@x#', where ``x`` is the current
"underlying command". You may only specify items which are "legal" for the
command. Whenever an item inscription contains '!*' or '!x' (with ``x``
as above) you must verify its selection.

Some commands will prompt for a direction. You may enter a "compass"
direction using any of the "direction keys" shown below. Sometimes, you may
specify that you wish to use the current "target", by pressing ``t`` or
``5``, or that you wish to select a new target, by pressing ``*`` (see
"Target" below).

        Original/Angband-like Keyset Directions
                 =  =  =
                 7  8  9
                 4     6
                 1  2  3
                 =  =  =

        Roguelike Keyset Directions
                 =  =  =
                 y  k  u
                 h     l
                 b  j  n
                 =  =  =

Each of the standard keysets provides some short-cuts over the "underlying
commands". For example, both keysets allow you to "walk" by simply pressing
an "original" direction key (or an hjkl-movement key if you are
using hjkl-movement), instead of using the "walk" command plus a
direction. The hjkl-movement also allows you to "run" by simply
holding the shift modifier key down while pressing an hjkl-movement
direction key, instead of using the "run" command plus a
direction. Both keysets allow the use of the ``5`` key to "stand still",
which is most convenient when using the original keyset.

Original Keyset Command Summary
===============================

===== ============================== ======= ============================
``a``  Use a staff                   ``A``   (unused)
``b``  Bash a door                   ``B``   (unused)
``c``  Close a door                  ``C``   Center map
``d``  Drop an item                  ``D``   Disarm a trap
``e``  List equipped items           ``E``   Eat some food
``f``  Fire from first quiver        ``F``   Fire from second quiver
``g``  Get objects on floor          ``G``   Ignore an item
``h``  (unused)                      ``H``   (unused)
``i``  List contents of pack         ``I``   (unused)
``j``  (unused)                      ``J``   (unused)
``k``  Destroy an item               ``K``   (unused)
``l``  Look around                   ``L``   Locate player on map
``m``  Fire at nearest target        ``M``   Display map of entire level
``n``  Repeat previous command       ``N``   (unused)
``o``  Open a door or chest          ``O``   Set options
``p``  Blow a horn                   ``P``   Toggle ignoring/unignoring
``q``  Quaff a potion                ``Q``   Kill character & quit
``r``  Remove equipment              ``R``   Enter a command count
``s``  Change song                   ``S``   Toggle stealth mode
``t``  Throw an item                 ``T``   Dig a tunnel
``u``  Use an item                   ``U``   (unused)
``v``  (unused)                      ``V``   Display version info
``w``  Wear/wield equipment          ``W``   (hjkl - locate player on map)
``x``  Examine an item               ``X``   Exchange places
``y``  (unused)                      ``Y``   (unused)
``z``  Stay still                    ``Z``   Rest for a period
``!``  (unused)                      ``^a``  (special - debug command)
``@``  Character description         ``^b``  (hjkl - bash a door)
``#``  (unused)                      ``^c``  (special - break)
``$``  (unused)                      ``^d``  (unused)
``%``  (unused)                      ``^e``  Toggle inven/equip window
``^``  (special - control key)       ``^f``  Fuel your light source
``&``  (unused)                      ``^g``  Do autopickup
``*``  Target monster or location    ``^h``  (unused)
``(``  (unused)                      ``^i``  (special - tab)
``)``  Dump screen to a file         ``^j``  (special - linefeed)
``{``  Inscribe an object            ``^k``  (hjkl - destroy an item)
``}``  Uninscribe an object          ``^l``  (hjkl - look around)
``[``  Display visible monster list  ``^m``  (special - return)
``]``  Display visible object list   ``^n``  (hjkl - repeat previous command)
``-``  (unused)                      ``^o``  Show previous message
``_``  Walk into a trap              ``^p``  Show previous messages
``/``  Alter grid                    ``^q``  (unused)
``=``  (unused)                      ``^r``  Redraw the screen
``;``  Walk                          ``^s``  Save and don't quit
``:``  Take notes                    ``^t``  Throw automatically
``'``  Target closest monster        ``^u``  (hjkl - use an item)
``"``  Enter a user pref command     ``^v``  (unused)
``,``  (unused)                      ``^w``  (special - wizard mode)
``<``  Go up staircase               ``^x``  Save and quit
``.``  Run                           ``^y``  (unused)
``>``  Go down staircase             ``^z``  (unused)
``\``  (special - bypass keymap)      ``~``  Check knowledge
 \`    (special - escape)             ``?``  Display help
``|``  Identify symbol               ``Tab`` Display ability menu
``0``  Forge an item
===== ============================== ======= ============================

Angband-like Keyset Command Summary
===================================

====== ============================= ======= ============================
``a``  Change song                   ``A``   (unused)
``b``  (unused)                      ``B``   Bash a door
``c``  Close a door                  ``C``   Character description
``d``  Drop an item                  ``D``   Disarm a trap
``e``  List equipped items           ``E``   Eat some food
``f``  Fire from first quiver        ``F``   Fire from second quiver
``g``  Get objects on floor          ``G``   Ignore an item
``h``  (unused)                      ``H``   (unused)
``i``  List contents of pack         ``I``   Inspect an item
``j``  (unused)                      ``J``   (unused)
``k``  Destroy an item               ``K``   (unused)
``l``  Look around                   ``L``   Locate player on map
``m``  Fire at nearest target        ``M``   Display map of entire level
``n``  Repeat previous command       ``N``   (unused)
``o``  Open a door or chest          ``O``   (unused)
``p``  Blow a horn                   ``P``   Toggle ignoring/unignoring
``q``  Quaff a potion                ``Q``   Kill character & quit
``r``  (unused)                      ``R``   Rest for a period
``s``  Stand still                   ``S``   Toggle stealth mode
``t``  Take off equipment            ``T``   Dig a tunnel
``u``  Use a staff                   ``U``   Use an item
``v``  Throw an item                 ``V``   Display version info
``w``  Wear/wield equipment          ``W``   (hjkl - locate player on map)
``x``  (unused)                      ``X``   Exchange places
``y``  (unused)                      ``Y``   (unused)
``z``  (unused)                      ``Z``   (unused)
``!``  (unused)                      ``^a``  (special - debug command)
``@``  Center map                    ``^b``  (hjkl - bash a door)
``#``  (unused)                      ``^c``  (special - break)
``$``  (unused)                      ``^d``  Forge an item
``%``  (unused)                      ``^e``  Toggle inven/equip window
``^``  (special - control key)       ``^f``  Fuel your light source
``&``  (unused)                      ``^g``  Do autopickup
``*``  Target monster or location    ``^h``  (unused)
``(``  (unused)                      ``^i``  (special - tab)
``)``  Dump screen to a file         ``^j``  (special - linefeed)
``{``  Inscribe an object            ``^k``  (hjkl - destroy an item)
``}``  Uninscribe an object          ``^l``  (hjkl - look around)
``[``  Display visible monster list  ``^m``  (special - return)
``]``  Display visible object list   ``^n``  (hjkl - repeat previous command)
``-``  (unused)                      ``^o``  Show previous message
``_``  Walk into a trap              ``^p``  Show previous messages
``+``  Alter grid                    ``^q``  (unused)
``=``  Set options                   ``^r``  Redraw the screen
``;``  Walk                          ``^s``  Save and don't quit
``:``  Take notes                    ``^t``  Throw automatically
``'``  Target closest monster        ``^u``  (hjkl - use an item)
``"``  Enter a user pref command     ``^v``  (unused)
``,``  (unused)                      ``^w``  (special - wizard mode)
``<``  Go up staircase               ``^x``  Save and quit
``.``  Run                           ``^y``  (unused)
``>``  Go down staircase             ``^z``  (unused)
``\``  (special - bypass keymap)     ``~``   Check knowledge
 \`    (special - escape)            ``?``   Display help
``/``  Identify symbol               ``Tab`` Display ability menu
``0``  Enter a command count
====== ============================= ======= ============================

Special Keys
============
 
Certain special keys may be intercepted by the operating system or the host
machine, causing unexpected results. In general, these special keys are
control keys, and often, you can disable their special effects.

If you are playing on a UNIX or similar system, then 'Ctrl-c' will
interrupt NarSil. The second and third interrupt will induce a warning
bell, and the fourth will induce both a warning bell and a special message,
since the fifth will quit the game, after killing your character. Also,
'Ctrl-z' will suspend the game, and return you to the original command
shell, until you resume the game with the 'fg' command. There is now a
compilation option to force the game to prevent the "double 'ctrl-z'
escape death trick". The 'Ctrl-\\' and 'Ctrl-d' and 'Ctrl-s' keys
should not be intercepted.
 
It is often possible to specify "control-keys" without actually pressing
the control key, by typing a caret (``^``) followed by the key. This is
useful for specifying control-key commands which might be caught by the
operating system as explained above.

Pressing backslash (``\``) before a command will bypass all keymaps, and
the next keypress will be interpreted as an "underlying command" key,
unless it is a caret (``^``), in which case the keypress after that will be
turned into a control-key and interpreted as a command in the underlying
keyset. The backslash key is useful for creating actions which are
not affected by any keymap definitions that may be in force, for example,
the sequence ``\`` + ``.`` + ``6`` will always mean "run east", even if the
``.`` key has been mapped to a different underlying command.

The ``0`` and ``^`` and ``\`` keys all have special meaning when entered at
the command prompt, and there is no "useful" way to specify any of them as
an "underlying command", which is okay, since they would have no effect.

For many input requests or queries, the special character 'ESCAPE' will
abort the command. The '[y/n]' prompts may be answered with ``y`` or
``n``, or 'escape'. The '-more-' message prompts may be cleared (after
reading the displayed message) by pressing 'ESCAPE', 'SPACE',
'RETURN', 'LINEFEED', or by any keypress, if the 'quick_messages'
option is turned on.
 
Command Counts
==============
 
Some commands can be executed a fixed number of times by preceding them
with a count. Counted commands will execute until the count expires, until
you type any character, or until something significant happens, such as
being attacked. Thus, a counted command doesn't work to attack another
creature. While the command is being repeated, the number of times left to
be repeated will flash by on the line at the bottom of the screen.

To give a count to a command, type ``R`` if you are using the original
keyset or ``0`` if you are using the Angband-like keyset, the repeat count,
and then the command. If you want to give a movement command and you are not
using hjkl movement (where the movement commands are digits), press space
after the count and you will be prompted for the command.  The open, tunnel,
disarm, bash, alter, and close commands default to having a repeat count of
99; all other commands default to not repeating at all.
 
Counted commands are very useful for time consuming commands, as they
automatically terminate on success, or if you are attacked. You may also
terminate any counted command (or resting or running), by typing any
character. This character is ignored, but it is safest to use a 'SPACE'
or 'ESCAPE' which are always ignored as commands in case you type the
command just after the count expires.

Selection of Objects
====================
 
Many commands will also prompt for a particular object to be used.
For example, the command to read a scroll will ask you which of the
scrolls that you are carrying that you wish to read.  In such cases, the
selection is made by typing a letter of the alphabet (or a number if choosing
from the quiver).  The prompt will indicate the possible letters/numbers,
and you will also be shown a list of the appropriate items.  Often you will
be able to press ``/`` to switch between inventory and equipment, or ``|`` to
select the quiver, or ``-`` to select the floor.  Using the right arrow also
rotates selection between equipment, inventory, quiver, floor and back to
equipment; the left arrow rotates in the opposite direction.
 
The particular object may be selected by an upper case or a lower case
letter. If lower case is used, the selection takes place immediately. If
upper case is used, then the particular option is described, and you are
given the option of confirming or retracting that choice. Upper case
selection is thus safer, but requires an extra key stroke.

