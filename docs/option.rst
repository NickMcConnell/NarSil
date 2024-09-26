===================
Option Descriptions
===================

Options are accessible through the ``O`` or ``=`` command, which provides an
interface to the various sets of options available to the player.

In the descriptions below, each option is listed as the textual summary
which is shown on the "options" screen, plus the internal name of the
option in brackets, followed by a textual description of the option.

Various concepts are mentioned in the descriptions below, including
"disturb", (cancel any running, resting, or repeated commands, which are in
progress), "flush" (forget any keypresses waiting in the keypress queue),
"fresh" (dump any pending output to the screen), and "sub-windows" (see
below).

.. contents:: Option pages
   :local:
   :depth: 1

User Interface Options
======================

When setting the user interface options, there are a handful of commands
to make it easier to get to a well-known state for all of those options.
They are:  's' to save the current selections so that they will be used
as the starting point for future characters, 'r' to reset the current
selections to the defaults for a new character, and 'x' to reset the
current selections to the Angband maintainer's defaults for the user
interface options.

Rogue-like movement ``hjkl_movement``
  Uses the hjklyubn keys for movement.

Use sound ``use_sound``
  Turns on sound effects, if your system supports them.

Quick messages ``quick_messages``
  Dismisses '-more-' and 'y/n' prompt with any key.  

Angband-like commands ``angband_keyset``
  Selects the "Angband-like" command set.  See :ref:`Playing the Game` for
  some descriptions of the comamnd sets.

Stop singing at the rest command ``stop_singing_on_rest``
  The character will Stop singing the rest command starts.

Don't attack unwary monsters automatically ``forgo_attacking_unwary``
  Bonus attacks on non-alert enemies aren't taken. This is particularly useful
  for players who value stealth highly and are trying to remain unobserved.
  
Audible beep ``beep``
  A beep sound will be made on errors such as giving an illegal direction.

Highlight player with cursor between turns ``highlight_player``
  Highlights the player with a cursor.  Useful if you have trouble finding
  the player.

Highlight target with cursor ``show_target``
  Highlights the current targeted monster with a cursor.  Useful when 
  combined with "use old target by default".

Highlight sleeping and unwary creatures ``highlight_unwary``
  Sleeping and unwary creatures will have a grey background.

Show walls as solid blocks ``solid_walls``
  Walls are solid blocks instead of # for granite and % for veins.  Veins
  are coloured darker than granite for differentiation purposes.

Show walls with shaded backgrounds ``hybrid_walls``
  Walls appear as # and % symbols overlaid on a gray background block.  
  This overrides ``solid_walls``.

Color: Shimmer multi-colored things ``animate_flicker``
  Certain powerful monsters and items will shimmer in real time, i.e.
  between keypresses.  

Center map continuously ``center_player``
  The map always centres on the player with this option on. With it off, it
  is divided into 25 sections, with coordinates (0,0) to (4,4), and will
  show one section at a time - the display will "flip" to the next section
  when the player nears the edge.

Avoid centering while running ``run_avoid_center``
  The game will wait until a run is finished before re-centering the map.  This
  is more efficient.

Automatically clear -more- prompts ``auto_more``
  The game does not wait for a keypress when it comes to a '-more-'
  prompt, but carries on going.  

Allow mouse clicks to move the player  ``mouse_movement``
  Clicking on the main window will be interpreted as a move command to that
  spot.

Display damage on hit ``display_hits``
  Display the damage done to a monster when it is hit.

Always pickup items ``pickup_always``
  Automatically picks up items when you walk upon them, provided it is safe
  to do so.

Always pickup items matching inventory ``pickup_inven``
  Like ``pickup_always``, but picks up an item only if it is a copy of an
  item that you already have in your inventory.

Show flavors in object descriptions ``show_flavors``
  Display "flavors" (color or variety) in object descriptions, even for
  objects whose type is known. This does not affect objects in stores.  



Birth options
=============

The birth options may only be changed when creating a character or using
the quick restart option for a dead character.  When setting the birth
options, there are a handful of commands to make it easier to get to a
well-known state for all of the birth options.  They are:  's' to save the
current selections so that they will be used as the starting point for
future characters, 'r' to reset the current selections to the defaults
for a new character, and 'x' to reset the current selections to the
Angband maintainer's defaults for the birth options.

Generate disconnected stairs ``birth_discon_stairs``
  With this option turned off, if you go down stairs, you start the new level
  on an up staircase, and vice versa (if you go up stairs, you start the
  next level on a down staircase).

  With this option on, you will never start on a staircase - but other
  staircases up and down elsewhere on the level will still be generated.

Force player descent (never make up stairs) ``birth_force_descend``
  Upwards staircases do not work.  All downward staircases, transport the
  character one level down (two for shafts).  

Restrict creation of artifacts ``birth_no_artifacts``
  No artifacts will be created. Ever. Just *how* masochistic are you?


Cheating options
================

Peek into monster creation ``cheat_hear``
  Cheaters never win. But they can peek at monster creation.

Peek into dungeon creation ``cheat_room``
  Cheaters never win. But they can peek at room creation.

Peek into something else ``cheat_xtra``
  Cheaters never win. But they can see debugging messages.

Allow player to avoid death ``cheat_live``
   Cheaters never win. But they can cheat death.

Window flags
============

Some platforms support "sub-windows", which are windows which can be used
to display useful information generally available through other means. The
best thing about these windows is that they are updated automatically
(usually) to reflect the current state of the world. The "window options"
can be used to specify what should be displayed in each window. The 
possible choices should be pretty obvious.

Display inven/equip
  Display the player inventory (and sometimes the equipment).

Display equip/inven
  Display the player equipment (and sometimes the inventory).

Display player (basic)
  Display a brief description of the character, including a breakdown of
  the current player "skills" (including attacks/shots per round).

Display player (extra)
  Display a special description of the character, including some of the
  "flags" which pertain to a character, broken down by equipment item.

Display player (compact)
  Display a brief description of the character, including a breakdown of
  the contributions of each equipment item to various resistances and
  stats.

Display map view
  Display the area around the player or around the target while targeting.
  This allows using graphical tiles in their original size.

Display messages
  Display the most recently generated "messages".

Display overhead view
  Display an overhead view of the entire dungeon level.

Display monster recall
  Display a description of the monster which has been most recently
  attacked, targeted, or examined in some way.

Display object recall
  Display a description of the most recently selected object. Currently
  this only affects spellbooks and prayerbooks. This window flag may be
  usefully combined with others, such as "monster recall".

Display monster list
  Display a list of monsters you know about and their distance from you.

Display status
  Display the current status of the player, with permanent or temporary boosts,
  resistances and status ailments (also available on the main window).

Display item list
  Display a list of items you know about and their distance from you.

Left Over Information
=====================

The ``hitpoint_warn`` value, if non-zero, is the percentage of maximal
hitpoints at which the player is warned that they may die. It is also used as
the cut-off for using the color red to display both hitpoints and mana.

The ``delay_factor`` value, if non-zero, will slow down the visual effects
used for missile, bolt, beam, and ball attacks. The actual time delay is
equal to ``delay_factor`` squared, in milliseconds.

The ``lazymove_delay`` value, if non-zero, will allow the player to move
diagonally by pressing the two appropriate arrow keys within the delay time.
This may be useful particularly when using a keyboard with no numpad.
