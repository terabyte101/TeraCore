DELETE FROM `achievement_criteria_data` WHERE `type`=11 AND `criteria_id`=3693;
INSERT INTO `achievement_criteria_data` VALUES
UPDATE `achievement_criteria_data` SET `ScriptName` = 'achievement_bg_control_all_nodes' WHERE `type` =11 AND `criteria_id` =3693;
-- Achievement: Pilgrims Peril
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` IN (11134,11135,11136,11137,11138,11139,11140,11141);
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`) VALUES
-- Alliance
(11134, 5, 66303, 0),   -- Orgrimmar
(11134, 6, 14, 0),      -- Orgrimmar
(11134, 16, 404, 0),    -- Orgrimmar
(11135, 5, 66303, 0),   -- Silvermoon City
(11135, 6, 3470, 0),    -- Silvermoon City
(11135, 16, 404, 0),    -- Silvermoon City
(11136, 5, 66303, 0),   -- Thunder Bluff
(11136, 6, 1638, 0),    -- Thunder Bluff
(11136, 16, 404, 0),    -- Thunder Bluff
(11137, 5, 66303, 0),   -- Undercity
(11137, 6, 1497, 0),    -- Undercity
(11137, 16, 404, 0),    -- Undercity
-- Horde
(11138, 5, 66303, 0),   -- Exodar
(11138, 6, 3557, 0),    -- Exodar
(11138, 16, 404, 0),    -- Exodar
(11139, 5, 66303, 0),   -- Darnassus
(11139, 6, 1657, 0),    -- Darnassus
(11139, 16, 404, 0),    -- Darnassus
(11140, 5, 66303, 0),   -- Ironforge
(11140, 6, 809, 0),     -- Ironforge
(11140, 16, 404, 0),    -- Ironforge
(11141, 5, 66303, 0),   -- Stormwind
(11141, 6, 12, 0),      -- Stormwind
(11141, 16, 404, 0);    -- Stormwind

-- Achievement: Now Were Cookin
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` IN (11118,11119,11120,11121,11122,11123,11124,11125,11126,11127);
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`) VALUES
-- Alliance
(11118, 16, 404, 0), -- Now Were Cookin / Cranberry Chutney
(11119, 16, 404, 0), -- Now Were Cookin / Candied Sweet Potato
(11120, 16, 404, 0), -- Now Were Cookin / Pumpkin Pie
(11121, 16, 404, 0), -- Now Were Cookin / Slow-Roasted Turkey
(11122, 16, 404, 0), -- Now Were Cookin / Spice Bread Stuffing
-- Horde
(11123, 16, 404, 0), -- Now Were Cookin / Candied Sweet Potato
(11124, 16, 404, 0), -- Now Were Cookin / Cranberry Chutney
(11125, 16, 404, 0), -- Now Were Cookin / Pumpkin Pie
(11126, 16, 404, 0), -- Now Were Cookin / Slow-Roasted Turkey
(11127, 16, 404, 0); -- Now Were Cookin / Spice Bread Stuffing

-- Achievement: Terokkar Turkey Time
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` = 11142;
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`) VALUES
(11142, 5, 66303, 0), -- Pilgrim Costume
(11142, 16, 404, 0);

-- Achievement: Pilgrim's Paunch
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` IN (11078,11079,11080,11081,11082,11083,11084,11085);
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`) VALUES
-- Alliance
(11078, 6, 1657, 0),  -- Darnassus
(11078, 16, 404, 0),  -- Darnassus
(11079, 6, 809, 0),   -- Ironforge
(11079, 16, 404, 0),  -- Ironforge
(11080, 6, 3557, 0),  -- Exodar
(11080, 16, 404, 0),  -- Exodar
(11081, 6, 12, 0),    -- Stormwind
(11081, 16, 404, 0),  -- Stormwind
-- Horde
(11082, 6, 14, 0),    -- Orgrimmar
(11082, 16, 404, 0),  -- Orgrimmar
(11083, 6, 3470, 0),  -- Silvermoon City
(11083, 16, 404, 0),  -- Silvermoon City
(11084, 6, 1638, 0),  -- Thunder Bluff
(11084, 16, 404, 0),  -- Thunder Bluff
(11085, 6, 1497, 0),  -- Undercity
(11085, 16, 404, 0);  -- Undercity

-- Achievement: "FOOD FIGHT!"
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` IN (11168,11178,11179,11180,11181);
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`, `ScriptName`) VALUES
(11168, 11, 0, 0, 'achievement_food_fight'),
(11168, 16, 404, 0, ''),
(11178, 11, 0, 0, 'achievement_food_fight'),
(11178, 16, 404, 0, ''),
(11179, 11, 0, 0, 'achievement_food_fight'),
(11179, 16, 404, 0, ''),
(11180, 11, 0, 0, 'achievement_food_fight'),
(11180, 16, 404, 0, ''),
(11181, 11, 0, 0, 'achievement_food_fight'),
(11181, 16, 404, 0, '');

-- Achievement: Sharing is Caring
DELETE FROM `achievement_criteria_data` WHERE `criteria_id` IN (11086,11088,11089,11090,11167);
INSERT INTO `achievement_criteria_data` (`criteria_id`, `type`, `value1`, `value2`) VALUES
(11086, 16, 404, 0),
(11088, 16, 404, 0),
(11089, 16, 404, 0),
(11090, 16, 404, 0),
(11167, 16, 404, 0);
