-- Temp Hack Fixes
DELETE FROM `npc_spellclick_spells` WHERE `npc_entry` IN (32823,32830,32840);
DELETE FROM `vehicle_template_accessory` WHERE `entry` IN (32823,32830,32840);

UPDATE `creature_template` SET `modelid2` = '0', `speed_walk` = '0', `speed_run` = '0' WHERE `entry` IN (34823,34812,34824,34822,34819);
