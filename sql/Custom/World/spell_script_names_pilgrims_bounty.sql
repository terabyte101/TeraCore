-- Achievement: The Turkinator
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_gen_turkey_tracker';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(62014, 'spell_gen_turkey_tracker'); -- Turkey Tracker

-- Feast On Spells
DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_gen_feast_on';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(61784, 'spell_gen_feast_on'), -- Feast On Turkey
(61785, 'spell_gen_feast_on'), -- Feast On Cranberries
(61786, 'spell_gen_feast_on'), -- Feast On Sweet Potatoes
(61787, 'spell_gen_feast_on'), -- Feast On Pie
(61788, 'spell_gen_feast_on'); -- Feast On Stuffing

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
'spell_gen_well_fed_pilgrims_bounty_ap',
'spell_gen_well_fed_pilgrims_bounty_zm',
'spell_gen_well_fed_pilgrims_bounty_hit',
'spell_gen_well_fed_pilgrims_bounty_haste',
'spell_gen_well_fed_pilgrims_bounty_spirit'
);
INSERT INTO `spell_script_names` (`spell_id` ,`ScriptName`) VALUES
(61807, 'spell_gen_well_fed_pilgrims_bounty_ap'),     -- A Serving of Turkey
(61804, 'spell_gen_well_fed_pilgrims_bounty_zm'),     -- A Serving of Cranberries
(61806, 'spell_gen_well_fed_pilgrims_bounty_hit'),    -- A Serving of Stuffing
(61808, 'spell_gen_well_fed_pilgrims_bounty_haste'),  -- A Serving of Sweet Potatoes
(61805, 'spell_gen_well_fed_pilgrims_bounty_spirit'); -- A Serving of Pie

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
'spell_gen_on_plate_pilgrims_bounty_turkey',
'spell_gen_on_plate_pilgrims_bounty_cranberries',
'spell_gen_on_plate_pilgrims_bounty_stuffing',
'spell_gen_on_plate_pilgrims_bounty_sweet_potatoes',
'spell_gen_on_plate_pilgrims_bounty_pie'
);
INSERT INTO `spell_script_names` (`spell_id` ,`ScriptName`) VALUES
(66250, 'spell_gen_on_plate_pilgrims_bounty_turkey'),            -- Pass The Turkey
(66261, 'spell_gen_on_plate_pilgrims_bounty_cranberries'),       -- Pass The Cranberries
(66259, 'spell_gen_on_plate_pilgrims_bounty_stuffing'),          -- Pass The Stuffing
(66262, 'spell_gen_on_plate_pilgrims_bounty_sweet_potatoes'),    -- Pass The Sweet Potatoes
(66260, 'spell_gen_on_plate_pilgrims_bounty_pie');               -- Pass The Pie

DELETE FROM `spell_script_names` WHERE `ScriptName`='spell_gen_bountiful_feast';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(66477, 'spell_gen_bountiful_feast'); -- Bountiful Feast

DELETE FROM `spell_script_names` WHERE `ScriptName` IN (
'spell_gen_slow_roasted_turkey',
'spell_gen_cranberry_chutney',
'spell_gen_spice_bread_stuffing',
'spell_gen_pumpkin_pie',
'spell_gen_candied_sweet_potato'
);
INSERT INTO `spell_script_names` (`spell_id` ,`ScriptName`) VALUES
(65422, 'spell_gen_slow_roasted_turkey'),   -- Slow-Roasted Turkey
(65420, 'spell_gen_cranberry_chutney'),     -- Cranberry Chutney
(65419, 'spell_gen_spice_bread_stuffing'),  -- Spice Bread Stuffing
(65421, 'spell_gen_pumpkin_pie'),           -- Pumpkin Pie
(65418, 'spell_gen_candied_sweet_potato');  -- Candied Sweet Potato
