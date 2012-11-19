-- Achievement: The Turkinator
UPDATE `creature_template` SET `ScriptName` = 'npc_wild_turkey' WHERE `entry` =32820; -- Wild Turkey

-- Pilgrims Bounty: Chair
UPDATE `creature_template` SET `spell1`=66261, `spell2`=61784, `spell3`=61785, `spell4`=61788, `spell5`=61786, `spell6`=61787 WHERE `entry`=34823; -- The Cranberry Chair
UPDATE `creature_template` SET `spell1`=66250, `spell2`=61784, `spell3`=61785, `spell4`=61788, `spell5`=61786, `spell6`=61787 WHERE `entry`=34812; -- The Turkey Chair
UPDATE `creature_template` SET `spell1`=66259, `spell2`=61784, `spell3`=61785, `spell4`=61788, `spell5`=61786, `spell6`=61787 WHERE `entry`=34819; -- The Stuffing Chair
UPDATE `creature_template` SET `spell1`=66260, `spell2`=61784, `spell3`=61785, `spell4`=61788, `spell5`=61786, `spell6`=61787 WHERE `entry`=34822; -- The Pie Chair
UPDATE `creature_template` SET `spell1`=66262, `spell2`=61784, `spell3`=61785, `spell4`=61788, `spell5`=61786, `spell6`=61787 WHERE `entry`=34824; -- The Sweet Potato Chair

-- Item: Turkey Caller
UPDATE `creature_template` SET `faction_A`=35, `faction_H`=35, `ScriptName` = 'npc_lonely_turkey' WHERE `entry` =32956; -- Lonely Turkey
