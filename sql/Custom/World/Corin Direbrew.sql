DELETE FROM`creature_text` WHERE `entry` = 23872;
INSERT INTO `creature_text` (`entry`, `groupid`, `id`, `text`, `type`, `language`, `probability`, `emote`, `duration`, `sound`, `comment`) VALUES 
('23872', '0', '0', 'This is an insult! An affront! They deny us participation in time-honored dwarven traditions!', '12', '0', '0', '0', '0', '0', 'Coren Rant 1'),
('23872', '1', '0', 'Are we going to hide in our mountain and let those swill-peddlers have their little shindig without us?', '12', '0', '0', '0', '0', '0', 'Coren Rant 2'),
('23872', '2', '0', 'DAMN RIGHT! We''ll show ''em why you don''t cross the Dark Iron dwarves!', '12', '0', '0', '0', '0', '0', 'Coren Rant 3'),
('23872', '3', '0', 'You''ll pay for this insult!', '12', '0', '0', '0', '0', '0', 'Coren Intro'),
('23872', '4', '0', 'Smash their kegs! DRAIN BREWFEST DRY!', '12', '0', '0', '0', '0', '0', 'Coren something');

DELETE FROM`creature_text` WHERE `entry` = 23795;
INSERT INTO `creature_text` (`entry`, `groupid`, `id`, `text`, `type`, `language`, `probability`, `emote`, `duration`, `sound`, `comment`) VALUES 
('23795', '0', '0', 'Yeah!', '12', '0', '100', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '0', '1', 'Right!', '12', '0', '100', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '0', '2', 'You said it!', '12', '0', '100', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '0', '3', 'Damn straight!', '12', '0', '100', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '1', '0', 'NO!', '0', '0', '100', '12', '0', '0', 'Dark Iron Antagonist'),
('23795', '1', '1', 'No way!', '0', '0', '100', '12', '0', '0', 'Dark Iron Antagonist'),
('23795', '1', '2', 'Not on your life!', '12', '0', '100', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '2', '0', 'Time to die, $C.', '12', '0', '50', '0', '0', '0', 'Dark Iron Antagonist'),
('23795', '2', '1', 'Never cross a Dark Iron, $C.', '12', '0', '50', '0', '0', '0', 'Dark Iron Antagonist');

UPDATE  `creature_template` SET  `faction_A` =  '35', `faction_H` =  '35', `npcflag` =  '1' WHERE `entry` =23872;
UPDATE  `creature_template` SET  `faction_A` =  '35', `faction_H` =  '35' WHERE  `entry` =23795;

DELETE FROM `creature` WHERE `id` IN (23795, 23872);
INSERT INTO `creature` (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `modelid`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `spawndist`, `currentwaypoint`, `curhealth`, `curmana`, `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`) VALUES 
('250640', '23795', '230', '1', '1', '0', '0', '896.401', '-130.886', '-49.7459', '2.56785', '300', '0', '0', '12600', '0', '0', '0', '0', '0'), -- Antagonist
('250642', '23795', '230', '1', '1', '0', '0', '895.341', '-132.526', '-49.7473', '2.56785', '300', '0', '0', '12600', '0', '0', '0', '0', '0'),
('250644', '23795', '230', '1', '1', '0', '0', '894.091', '-134.46', '-49.7488', '2.56785', '300', '0', '0', '12600', '0', '0', '0', '0', '0'),
('250400', '23872', '230', '1', '1', '0', '0', '895.679', '-127.46', '-49.7433', '3.63599', '43200', '0', '0', '302400', '0', '2', '1', '0', '0'); -- Spawn Coren

DELETE FROM `waypoint_data` WHERE `id` = 2504000;
INSERT INTO `waypoint_data` (`id`, `point`, `position_x`, `position_y`, `position_z`, `orientation`, `delay`, `move_flag`, `action`, `action_chance`, `wpguid`) VALUES 
('2504000', '1', '888.65', '-131.418', '-49.7426', '0', '500', '0', '0', '100', '0'),
('2504000', '2', '895.869', '-127.393', '-49.7432', '0', '500', '0', '0', '100', '0');

DELETE FROM `game_event_creature` WHERE `guid` IN (250640, 250642, 250644, 250400);
INSERT INTO  `game_event_creature` (`eventEntry`,`guid`)VALUES 
('24', '250640'),
('24', '250642'),
('24', '250644'),
('24', '250400');

DELETE FROM `creature_addon` WHERE `guid` = 250400;
INSERT INTO `creature_addon` (`guid`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `auras`) VALUES ('250400', '2504000', '0', '0', '0', '0', NULL);

UPDATE  `creature_template` SET  `ScriptName` =  'npc_dark_iron_antagonist' WHERE  `entry` =23795;
UPDATE  `creature_template` SET  `ScriptName` =  'boss_coren_direbrew' WHERE  `entry` =23872;
