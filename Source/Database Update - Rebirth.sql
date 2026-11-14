-- Rebirth: run once on an existing database.
-- New databases created from 'Database Setup.sql' already have these columns.
ALTER TABLE `char_database`
  ADD COLUMN `Rebirth` int(11) unsigned NOT NULL DEFAULT '0' AFTER `StreaksEnded`,
  ADD COLUMN `RebirthMode` int(11) unsigned NOT NULL DEFAULT '0' AFTER `Rebirth`,
  ADD COLUMN `AltLevel` int(11) unsigned NOT NULL DEFAULT '0' AFTER `RebirthMode`,
  ADD COLUMN `AltExp` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltLevel`,
  ADD COLUMN `AltStr` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltExp`,
  ADD COLUMN `AltVit` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltStr`,
  ADD COLUMN `AltDex` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltVit`,
  ADD COLUMN `AltInt` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltDex`,
  ADD COLUMN `AltMag` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltInt`,
  ADD COLUMN `AltChr` int(11) unsigned NOT NULL DEFAULT '0' AFTER `AltMag`;
