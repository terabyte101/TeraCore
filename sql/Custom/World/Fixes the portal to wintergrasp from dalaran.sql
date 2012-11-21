UPDATE  `gameobject_template` SET  `data0` =  '59096' WHERE  `entry` =193772; -- Dal to WG portal, correct spell
DELETE FROM spell_target_position WHERE id IN ('59096', '58632', '58633');
INSERT INTO spell_target_position (id, target_map, target_position_x, target_position_y, target_position_z, target_orientation) VALUES
('59096', '571', '5325.06', '2843.36', '409.285', '3.20278'),
('58632', '571', '5097.79', '2180.29', '365.61', '2.41'),
('58633', '571', '5026.80', '3676.69', '362.58', '3.94');
