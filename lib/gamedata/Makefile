MKPATH=../../mk/
include $(MKPATH)buildsys.mk

FILES = ability.txt artefact.txt body.txt blow_methods.txt \
 blow_effects.txt brand.txt chest_trap.txt constants.txt drop.txt \
 dungeon_profile.txt female_entry_poetry.txt flavor.txt history.txt house.txt \
 male_entry_poetry.txt monster.txt monster_base.txt monster_spell.txt \
 names.txt object.txt object_base.txt object_property.txt pain.txt \
 player_timed.txt projection.txt pursuit.txt race.txt sex.txt slay.txt \
 song.txt special.txt summon.txt terrain.txt trap.txt tutorial.txt \
 ui_knowledge.txt vault.txt visuals.txt warning.txt world.txt

R1 = $(GAMEDATA_IN_LIB:true=$(FILES))
DATA = $(R1:false=)
R2 = $(GAMEDATA_IN_LIB:false=$(FILES))
CONFIG = $(R2:true=)

PACKAGE = gamedata
