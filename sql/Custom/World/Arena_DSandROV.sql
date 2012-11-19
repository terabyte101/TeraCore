-- Dalaran Sewers and Ring of Valor
UPDATE `gameobject_template` SET `flags` = '36' WHERE `entry` IN (192642,192643);
DELETE FROM disables WHERE sourceType = 3 AND entry IN (10,11);