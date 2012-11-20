DELETE FROM `item_loot_template` WHERE `entry` = 54210;
INSERT INTO `item_loot_template` (`entry`, `item`, `ChanceOrQuestChance`, `lootmode`, `groupid`, `mincountOrRef`, `maxcount`) VALUES
(54218, 1, 100, 1, 50, -424242, 1),
(54218, 2, 1, 1, 1, -424243, 1);
 
DELETE FROM `reference_loot_template` WHERE `entry` IN (424242, 424243);
INSERT INTO `reference_loot_template` (`entry`, `item`, `ChanceOrQuestChance`, `lootmode`, `groupid`, `mincountOrRef`, `maxcount`) VALUES
(424242, 35223, 0, 1, 50, 50, 50),
(424242, 45047, 0, 1, 50, 50, 50),
(424242, 46779, 0, 1, 50, 50, 50),
(424243, 23720, 0, 1, 1, 1, 1),
(424243, 49284, 0, 1, 1, 1, 1),
(424243, 49283, 0, 1, 1, 1, 1),
(424243, 49286, 0, 1, 1, 1, 1),
(424243, 49285, 0, 1, 1, 1, 1),
(424243, 49282, 0, 1, 1, 1, 1);