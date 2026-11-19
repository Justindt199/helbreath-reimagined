-- VALORIA enchanting: run once on an existing database (Build\Update Database.bat does it).
-- New databases created from 'Database Setup.sql' already have these columns.
ALTER TABLE `char_database` ADD COLUMN IF NOT EXISTS `ValoriaEnchant` varchar(128) DEFAULT NULL AFTER `AltChr`;
ALTER TABLE `item` ADD COLUMN IF NOT EXISTS `Valoria` varchar(64) DEFAULT NULL AFTER `ItemPos`;
-- VALORIA passive tree (Shared\ValoriaPassives.h): 48 bytes as hex, NULL = only the root taken
ALTER TABLE `char_database` ADD COLUMN IF NOT EXISTS `ValoriaPassive` varchar(96) DEFAULT NULL AFTER `ValoriaEnchant`;
-- VALORIA item rarity: an item's VALORIA data grew from 32 to 48 bytes (96 hex digits)
ALTER TABLE `item` MODIFY COLUMN `Valoria` varchar(96) DEFAULT NULL;
-- VALORIA life skills, achievements and side quests: a character's VALORIA data grew from 64 to 432 bytes (864 hex digits)
ALTER TABLE `char_database` MODIFY COLUMN `ValoriaEnchant` varchar(880) DEFAULT NULL;
-- every item modifier learnable (Helbreath's attributes and the resistances too): 560 bytes (1120 hex digits)
ALTER TABLE `char_database` MODIFY COLUMN `ValoriaEnchant` varchar(1120) DEFAULT NULL;
SELECT 'VALORIA enchanting, passive tree and rarity columns ready' AS status;
