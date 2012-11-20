-- Update loot ids for(Ymirjar Deathbringer,Ymirjar Flamebearer,Ymirjar Skycaller,Ymirjar Wrathbringer,Stonespine Gargoyle) - Same as other ICC 5man hc trash
UPDATE `creature_template` SET `lootid`=100001 WHERE `entry` IN (37641,37642,37643,37644,37622);
DELETE FROM `creature_loot_template` WHERE `entry` IN (37641,37642,37643,37644,37622);
- New Battered Hilt Ref
DELETE FROM `reference_loot_template` WHERE `entry`=35075;
INSERT INTO `reference_loot_template` (`entry`, `item`, `ChanceOrQuestChance`, `lootmode`, `groupid`, `mincountOrRef`, `maxcount`) VALUES 
(35075, 50379, 0, 1, 1, 1, 1), -- Alliance
(35075, 50380, 0, 1, 1, 1, 1); -- Horde
- Conditions for Battered hilt drop
DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId`=10 AND `SourceGroup`=35075;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES 
(10, 35075, 50379, 0, 0, 6, 0, 469, 0, 0, 0, 0, '', 'Battered Hilt Must Be Alliance'),
(10, 35075, 50380, 0, 0, 6, 0, 67, 0, 0, 0, 0, '', 'Battered Hilt Must Be Horde');
- Add new ref to ICC 5man hc Trash Ref (0.08 percent is average of values on wowhead 4.16/52)
DELETE FROM `reference_loot_template` WHERE `entry`=35073 AND `item`=9 AND `mincountOrRef`=-35075;
INSERT INTO `reference_loot_template` (`entry`, `item`, `ChanceOrQuestChance`, `lootmode`, `groupid`, `mincountOrRef`, `maxcount`) VALUES 
(35073, 9, 0.08, 1, 0, -35075, 1);
