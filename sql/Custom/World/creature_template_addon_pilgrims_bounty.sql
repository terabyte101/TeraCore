-- Pilgrims Bounty: Chair
DELETE FROM `creature_template_addon` WHERE `entry` IN (34823,34812,34824,34822,34819);
INSERT INTO `creature_template_addon` (`entry`, `path_id`, `mount`, `bytes1`, `bytes2`, `emote`, `auras`) VALUES
(34823, 0, 0, 0, 1, 0, '61793 61798'), -- The Cranberry Chair / (Cranberry Server | Can Eat - Cranberries)
(34812, 0, 0, 0, 1, 0, '61796 61801'), -- The Turkey Chair / (Turkey Server | Can Eat - Turkey)
(34824, 0, 0, 0, 1, 0, '61797 61802'), -- The Sweet Potato Chair / (Sweet Potatoes Server | Can Eat - Sweet Potatoes)
(34822, 0, 0, 0, 1, 0, '61794 61799'), -- The Pie Chair / (Pie Server | Can Eat - Pie)
(34819, 0, 0, 0, 1, 0, '61795 61800'); -- The Stuffing Chair / (Stuffing Server | Can Eat - Stuffing)
