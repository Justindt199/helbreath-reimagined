-- VALORIA enchanting: run once on an existing database (Build\Update Database.bat does it).
-- New databases created from 'Database Setup.sql' already have these columns.
ALTER TABLE `char_database` ADD COLUMN IF NOT EXISTS `ValoriaEnchant` varchar(128) DEFAULT NULL AFTER `AltChr`;
ALTER TABLE `item` ADD COLUMN IF NOT EXISTS `Valoria` varchar(64) DEFAULT NULL AFTER `ItemPos`;
-- VALORIA passive tree (Shared\ValoriaPassives.h): 48 bytes as hex, NULL = only the root taken
ALTER TABLE `char_database` ADD COLUMN IF NOT EXISTS `ValoriaPassive` varchar(96) DEFAULT NULL AFTER `ValoriaEnchant`;
-- VALORIA item rarity: an item's VALORIA data grew from 32 to 48 bytes (96 hex digits)
ALTER TABLE `item` MODIFY COLUMN `Valoria` varchar(96) DEFAULT NULL;
SELECT 'VALORIA enchanting, passive tree and rarity columns ready' AS status;
