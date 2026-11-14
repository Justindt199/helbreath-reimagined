-- Most Wanted (enemy-kill streaks): run once on an existing database.
-- New databases created from 'Database Setup.sql' already have these columns.
ALTER TABLE `char_database`
  ADD COLUMN `WantedStreak` int(11) unsigned NOT NULL DEFAULT '0' AFTER `TotalEK`,
  ADD COLUMN `WantedLastKill` int(11) unsigned NOT NULL DEFAULT '0' AFTER `WantedStreak`,
  ADD COLUMN `BestStreak` int(11) unsigned NOT NULL DEFAULT '0' AFTER `WantedLastKill`,
  ADD COLUMN `StreaksEnded` int(11) unsigned NOT NULL DEFAULT '0' AFTER `BestStreak`;
