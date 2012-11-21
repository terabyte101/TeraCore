DELETE FROM `creature_loot_template` WHERE `entry` IN (36497,36498,36502,37677,36494,37613,36476,37627,36658,36938,38112,38599,38113,38603) AND `item`=43228;
INSERT INTO `creature_loot_template` (`entry`,`item`,`ChanceOrQuestChance`,`lootmode`,`groupid`,`mincountOrRef`,`maxcount`) VALUES
(36497,43228,100,1,0,4,4), -- Bronjahm normal/heroic
(36498,43228,100,1,0,4,4),
(36502,43228,100,1,0,4,4), -- Devourer normal/heroic
(37677,43228,100,1,0,4,4),
(36494,43228,100,1,0,4,4), -- Garfrost normal/heroic
(37613,43228,100,1,0,4,4),
(36476,43228,100,1,0,4,4), -- Ick normal/heroic
(37627,43228,100,1,0,4,4),
(36658,43228,100,1,0,4,4), -- Scourgelord normal/heroic
(36938,43228,100,1,0,4,4),
(38112,43228,100,1,0,4,4), -- Falric normal/heroic
(38599,43228,100,1,0,4,4),
(38113,43228,100,1,0,4,4), -- Marwyn normal/heroic
(38603,43228,100,1,0,4,4);

DELETE FROM `gameobject_loot_template` WHERE `entry` IN (27985,27993) AND `item`=43228;
INSERT INTO `gameobject_loot_template` (`entry`,`item`,`ChanceOrQuestChance`,`lootmode`,`groupid`,`mincountOrRef`,`maxcount`) VALUES -- LK chest HoR
(27985,43228,100,1,0,4,4),
(27993,43228,100,1,0,4,4);

DELETE FROM `conditions` WHERE `SourceGroup` IN (36497,36498,36502,37677,36494,37613,36476,37627,36658,36938,38112,38599,38113,38603,27985,27993) AND `SourceEntry`=43228;
INSERT INTO `conditions` (`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `ErrorTextId`, `ScriptName`, `Comment`) VALUES
(1,36497,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36498,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36502,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,37677,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36494,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,37613,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36476,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,37627,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36658,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,36938,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,38112,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,38599,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,38113,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(1,38603,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(4,27985,43228,0,0,1,0,57940,0,0,0,0,'', NULL),
(4,27993,43228,0,0,1,0,57940,0,0,0,0,'', NULL);
