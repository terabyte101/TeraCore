UPDATE creature_template SET AIName='SmartAI' WHERE entry=24786;
DELETE FROM `smart_scripts` WHERE (`entryorguid`=24786 AND `source_type`=0);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(24786, 0, 0, 1, 8, 0, 100, 0, 44454, 0, 0, 0, 11, 44456, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Reef Bull - On Spell Hit - Cast Spell'),
(24786, 0, 1, 0, 61, 0, 100, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 'Reef Bull - On Spell Hit - Despawn');

UPDATE creature_template SET ScriptName='npc_attracted_reef_bull' WHERE entry=24804;

DELETE FROM spell_script_names WHERE spell_id=21014;
INSERT INTO spell_script_names VALUES (21014, 'spell_anuniaqs_net');
