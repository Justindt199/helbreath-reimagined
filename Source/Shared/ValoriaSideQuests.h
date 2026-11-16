// ValoriaSideQuests.h: side quests in Helbreath Reimagined, on VALORIA's quest system (VALORIA
// scripts/side_quests.gd): townspeople give them, each has up to four objectives, and each is done
// once per character. Compiled into the client (UI\SideQuestPanel.cpp) and the game server
// (HG Server\SideQuests.cpp).
//
// The rules (the same for every quest):
//   * Givers are the three townspeople who walk both Aresden and Elvine: McGaffin (hunting),
//     Perry (gathering) and Devlin (the arcane). Each one's quests unlock in order, and each
//     needs a level (minLevel).
//   * Objectives, as VALORIA's: KILL a monster (its Npc.cfg name), COLLECT something that a kill
//     of the listed monsters drops with a chance (kept with the quest, not in the bag), VISIT a map.
//     Only the one who lands the kill counts it.
//   * Rewards: XP = a third of a level at the quest's level, gold = 250 x its level, and one
//     VALORIA item.
//   * Up to SIDE_ACTIVE quests at a time. Abandoning one clears its progress; it can be taken again.
//   * Quests keep their slot in kQuests: only ever add to the end, or saved progress moves.
#pragma once

namespace SideQ {

enum { GIVER_MCGAFFIN = 67, GIVER_PERRY = 68, GIVER_DEVLIN = 69 };
enum ObjType { OBJ_NONE = 0, OBJ_KILL, OBJ_COLLECT, OBJ_VISIT };
// NOTIFY_SIDEEVENT codes, and the reasons of EV_ERROR
enum { EV_PROGRESS = 1, EV_READY, EV_TAKEN, EV_ABANDONED, EV_COMPLETE, EV_ERROR };
enum { ERR_LEVEL = 1, ERR_FAR, ERR_FULL };
enum { SQ_TAKE = 1, SQ_HANDIN, SQ_ABANDON };	// MSGID_REQUEST_SIDEQUEST actions
const int MAX_OBJ = 4;

struct Obj {
	unsigned char type;
	const char *  target;	// KILL: monster; COLLECT: monsters that drop it ("A|B"); VISIT: maps ("a|b")
	unsigned short count;
	unsigned char chance;	// COLLECT: percent per kill
	const char *  label;	// what it is called ("Werewolf Claw", "the Middleland")
};

struct Quest {
	unsigned char giver;
	unsigned char minLevel;
	const char * title;
	const char * text;		// the offer
	const char * done;		// the hand-in
	Obj obj[MAX_OBJ];
	const char * item;		// reward item (Helbreath name)
	unsigned char itemCount;
};

static const Quest kQuests[] = {
	// ---- McGaffin: hunting
	{ GIVER_MCGAFFIN, 1, "Pests in the Fields",
	  "The farms outside the walls are crawling again. Slimes in the furrows, ants in the grain. Clear them out before the harvest rots, and I'll see you're paid.",
	  "The farmers will sleep tonight. Here, you've earned this.",
	  { { OBJ_VISIT, "arefarm|elvfarm|istfarm", 1, 0, "the town farm" }, { OBJ_KILL, "Slime", 15, 0, "Slime" }, { OBJ_KILL, "Giant-Ant", 10, 0, "Giant Ant" } },
	  "HealthPotion", 5 },
	{ GIVER_MCGAFFIN, 10, "Stingers",
	  "Scorpions have been coming up out of the sand near town and stinging the carters. Kill them, and bring me their stingers - the apothecary pays well for the venom.",
	  "Good stingers, these. The carters thank you.",
	  { { OBJ_KILL, "Scorpion", 15, 0, "Scorpion" }, { OBJ_COLLECT, "Scorpion", 4, 35, "Scorpion Stinger" } },
	  "VerdantEssence", 1 },
	{ GIVER_MCGAFFIN, 20, "The Middleland Road",
	  "The road into the Middleland isn't safe any more. Orc raiders by day, the dead by night. Go see it for yourself, and thin both out.",
	  "The caravans can roll again. Well done.",
	  { { OBJ_VISIT, "middleland", 1, 0, "the Middleland" }, { OBJ_KILL, "Orc", 20, 0, "Orc" }, { OBJ_KILL, "Zombie", 12, 0, "Zombie" } },
	  "GreaterHealthPotion", 5 },
	{ GIVER_MCGAFFIN, 35, "Wolves at Moonrise",
	  "Werewolves hunt the Middleland when the moon is up, and stalkers follow behind them for the scraps. Bring me three claws so the Council believes me.",
	  "Three claws, still sharp. The Council can't argue with these.",
	  { { OBJ_KILL, "WereWolf", 12, 0, "Werewolf" }, { OBJ_COLLECT, "WereWolf", 3, 30, "Werewolf Claw" }, { OBJ_KILL, "Stalker", 6, 0, "Stalker" } },
	  "AzureEssence", 1 },
	{ GIVER_MCGAFFIN, 55, "Down the Old Stair",
	  "There's a stair under the Middleland that goes down to the second dungeon level. Cyclopes and stone golems hold it now. Go down and break their hold.",
	  "You came back up. Not everyone does.",
	  { { OBJ_VISIT, "dglv2", 1, 0, "the second dungeon level" }, { OBJ_KILL, "Cyclops", 15, 0, "Cyclops" }, { OBJ_KILL, "Stone-Golem", 10, 0, "Stone Golem" } },
	  "FluxStone", 3 },
	{ GIVER_MCGAFFIN, 75, "Troll Blood",
	  "Trolls have come down into the hunting grounds. Kill them where they stand, and bring me their blood. Troll blood heals near anything, if you can stand the smell.",
	  "Phew. That's troll blood all right.",
	  { { OBJ_KILL, "Troll", 15, 0, "Troll" }, { OBJ_COLLECT, "Troll", 3, 30, "Troll Blood" } },
	  "TemperingOil", 2 },
	{ GIVER_MCGAFFIN, 100, "The Labyrinth",
	  "The Maze swallows hunters whole. Minotaurs walk its halls and beholders watch every turning. Go in, come out, and bring their horns as proof.",
	  "You found your way out of the Maze. I owe you a drink.",
	  { { OBJ_VISIT, "maze", 1, 0, "the Maze" }, { OBJ_KILL, "Minotaurs", 12, 0, "Minotaur" }, { OBJ_KILL, "Beholder", 8, 0, "Beholder" }, { OBJ_COLLECT, "Minotaurs", 3, 25, "Minotaur Horn" } },
	  "VioletEssence", 1 },
	{ GIVER_MCGAFFIN, 140, "Lair of Abaddon",
	  "Last hunt, and the worst. Abaddon's lair. Barlogs at the gate, a hellclaw and a tigerworm in the deep. Nobody's asked this of you lightly.",
	  "You walked into Abaddon's lair and back out. I'll tell this story until I die.",
	  { { OBJ_VISIT, "abaddon", 1, 0, "Abaddon's lair" }, { OBJ_KILL, "Barlog", 10, 0, "Barlog" }, { OBJ_KILL, "Hellclaw", 2, 0, "Hellclaw" }, { OBJ_KILL, "Tigerworm", 1, 0, "Tigerworm" } },
	  "RadiantOrb", 1 },

	// ---- Perry: gathering
	{ GIVER_PERRY, 5, "Hides for the Tanner",
	  "The tanner's out of leather and winter's coming. Amphis hides make the softest boots. Would you fetch me a few?",
	  "Oh, they're lovely! Here, take some of my honey cakes.",
	  { { OBJ_KILL, "Amphis", 10, 0, "Amphis" }, { OBJ_COLLECT, "Amphis", 4, 40, "Amphis Hide" } },
	  "HoneyCake", 5 },
	{ GIVER_PERRY, 15, "Royal Jelly",
	  "Giant ants keep a jelly for their queen. A spoonful in tea and the healers swear by it. Bring me three jars' worth?",
	  "Sweet as anything. The healers will be so pleased.",
	  { { OBJ_KILL, "Giant-Ant", 20, 0, "Giant Ant" }, { OBJ_COLLECT, "Giant-Ant", 3, 25, "Royal Jelly" } },
	  "ManaPotion", 5 },
	{ GIVER_PERRY, 25, "Bone Meal",
	  "The town garden needs bone meal, and the skeletons outside town have plenty to spare. It's grim work, I know.",
	  "Grim, but the roses will love it. Thank you.",
	  { { OBJ_KILL, "Skeleton", 15, 0, "Skeleton" }, { OBJ_COLLECT, "Skeleton", 5, 40, "Bone Dust" } },
	  "SpiritSalt", 2 },
	{ GIVER_PERRY, 45, "Frog Song",
	  "The big frogs down in the dungeon have glands that make a fine salve for burns. Mind the cyclopes down there.",
	  "Slippery things! Here - these will serve you better than they serve me.",
	  { { OBJ_KILL, "Giant-Frog", 15, 0, "Giant Frog" }, { OBJ_COLLECT, "Giant-Frog", 4, 35, "Frog Gland" } },
	  "GreaterManaPotion", 5 },
	{ GIVER_PERRY, 80, "Lizard Scale",
	  "In Procella the giant lizards shed scales as hard as steel. The armourers would give anything for a few. Careful - the minotaurs roam there too.",
	  "Look how they shine! The armourers will be beside themselves.",
	  { { OBJ_VISIT, "procella", 1, 0, "Procella" }, { OBJ_KILL, "Giant-Lizard", 15, 0, "Giant Lizard" }, { OBJ_COLLECT, "Giant-Lizard", 4, 30, "Lizard Scale" } },
	  "MoonwellDew", 2 },
	{ GIVER_PERRY, 90, "Weeds With Teeth",
	  "Procella's cannibal plants bleed a sap that cures fever. They bite, mind you. Gloves won't help.",
	  "Sticky and smelly and exactly right. You're a treasure.",
	  { { OBJ_KILL, "Cannibal-Plant", 15, 0, "Cannibal Plant" }, { OBJ_COLLECT, "Cannibal-Plant", 4, 30, "Living Sap" } },
	  "SulfurBloom", 3 },
	{ GIVER_PERRY, 110, "Shells of Druncn",
	  "Druncn City's half under water now. Claw turtles and crayfish everywhere. A turtle shell or two would make the finest shields in town.",
	  "These shells! The smith will make something wonderful.",
	  { { OBJ_VISIT, "druncncity", 1, 0, "Druncn City" }, { OBJ_KILL, "Claw-Turtle", 12, 0, "Claw Turtle" }, { OBJ_KILL, "Giant-Crayfish", 12, 0, "Giant Crayfish" }, { OBJ_COLLECT, "Claw-Turtle", 2, 25, "Turtle Shell" } },
	  "StarmetalShard", 1 },
	{ GIVER_PERRY, 130, "The Nizie Pearl",
	  "They say the nizies of Druncn grow a pearl inside them, once in a long while. I've always wanted to see one. Just one.",
	  "It's beautiful. I'll never forget you brought me this.",
	  { { OBJ_KILL, "Nizie", 10, 0, "Nizie" }, { OBJ_COLLECT, "Nizie", 1, 15, "Nizie Pearl" } },
	  "ReforgingOrb", 1 },

	// ---- Devlin: the arcane
	{ GIVER_DEVLIN, 8, "Restless Graves",
	  "The dead outside town won't stay down. Zombies, skeletons - someone is stirring them. Put them back in the ground while I find out who.",
	  "Quieter already. Here, for your trouble.",
	  { { OBJ_KILL, "Zombie", 10, 0, "Zombie" }, { OBJ_KILL, "Skeleton", 10, 0, "Skeleton" } },
	  "ManaPotion", 5 },
	{ GIVER_DEVLIN, 30, "Clay That Walks",
	  "A clay golem walks because a core inside it tells it to. I want to know who wrote those cores. Bring me two.",
	  "Two cores, and both still warm with the spell. Fascinating.",
	  { { OBJ_KILL, "Clay-Golem", 15, 0, "Clay Golem" }, { OBJ_COLLECT, "Clay-Golem", 2, 25, "Golem Core" } },
	  "MysticShard", 1 },
	{ GIVER_DEVLIN, 50, "Hellhounds",
	  "Hellbounds carry fire from somewhere below. The ash they leave behind is still burning. Collect it for me - carefully.",
	  "Still smouldering. Something down there feeds them.",
	  { { OBJ_KILL, "Hellbound", 12, 0, "Hellbound" }, { OBJ_COLLECT, "Hellbound", 3, 35, "Ember Ash" } },
	  "SulfurBloom", 2 },
	{ GIVER_DEVLIN, 70, "The Mastermages",
	  "The orc mastermages of Procella bind their spells with a sigil. If I could read one, I could unmake them. Take two from their bodies.",
	  "Crude work, but clever. I'll have them read by morning.",
	  { { OBJ_KILL, "MasterMage-Orc", 10, 0, "Mastermage Orc" }, { OBJ_COLLECT, "MasterMage-Orc", 2, 30, "Spellbinder Sigil" } },
	  "AzureEssence", 2 },
	{ GIVER_DEVLIN, 95, "The Tower of Hell",
	  "The sigils point to the Tower of Hell. Climb it. Dark elves guard the stair and liches the top. Find out what they're guarding.",
	  "Liches. Of course it was liches. You did well to come back.",
	  { { OBJ_VISIT, "toh1", 1, 0, "the Tower of Hell" }, { OBJ_VISIT, "toh3", 1, 0, "the Tower's top floor" }, { OBJ_KILL, "Dark-Elf", 10, 0, "Dark Elf" }, { OBJ_KILL, "Liche", 6, 0, "Liche" } },
	  "SoulShard", 1 },
	{ GIVER_DEVLIN, 105, "Ice in the Bones",
	  "The liches draw cold from Icebound. Break their source: the ice golems and the frost there. And bring me crystals, so I can see the spell for myself.",
	  "Cold enough to burn. The liches will feel this.",
	  { { OBJ_VISIT, "icebound", 1, 0, "Icebound" }, { OBJ_KILL, "Ice-Golem", 12, 0, "Ice Golem" }, { OBJ_KILL, "Frost", 10, 0, "Frost" }, { OBJ_COLLECT, "Ice-Golem|Frost", 3, 30, "Frost Crystal" } },
	  "PrismaticShard", 1 },
	{ GIVER_DEVLIN, 120, "Horns of the Abyss",
	  "Demons and gargoyles are coming up through the old dungeons. Demon horn is the one thing that holds a ward against them. Bring me three.",
	  "Three horns. My wards will hold now.",
	  { { OBJ_KILL, "Demon", 12, 0, "Demon" }, { OBJ_KILL, "Gagoyle", 8, 0, "Gargoyle" }, { OBJ_COLLECT, "Demon", 3, 25, "Demon Horn" } },
	  "VioletEssence", 2 },
	{ GIVER_DEVLIN, 150, "Fires of Infernia",
	  "It all ends in Infernia. Walk both halves of it, put down its barlogs, and kill a fire wyvern. Then whoever stirs the dead will have nothing left to stir.",
	  "It's over. Whatever woke them, it sleeps now. Thank you, truly.",
	  { { OBJ_VISIT, "inferniaA", 1, 0, "Infernia (west)" }, { OBJ_VISIT, "inferniaB", 1, 0, "Infernia (east)" }, { OBJ_KILL, "Barlog", 8, 0, "Barlog" }, { OBJ_KILL, "Fire-Wyvern", 1, 0, "Fire Wyvern" } },
	  "AscensionOrb", 1 },
};
const int QUEST_COUNT = (int)(sizeof(kQuests) / sizeof(kQuests[0]));

inline const char * GiverName(int g)
{
	switch (g) { case GIVER_MCGAFFIN: return "McGaffin"; case GIVER_PERRY: return "Perry"; case GIVER_DEVLIN: return "Devlin"; }
	return "?";
}
inline unsigned int GoldReward(const Quest & q) { return 250u * q.minLevel; }

// Does "A|B|C" list name (case-insensitive)?
inline bool ListHas(const char * list, const char * name)
{
	if (list == 0 || name == 0) return false;
	const char * p = list;
	while (*p) {
		const char * e = p;
		while (*e && *e != '|') e++;
		int i = 0;
		while (p + i < e && name[i] && ((p[i] | 0x20) == (name[i] | 0x20))) i++;
		if (p + i == e && name[i] == 0) return true;
		p = (*e == '|') ? e + 1 : e;
	}
	return false;
}

// The quest before this one from the same giver (-1: none); it must be done first.
inline int Previous(int i)
{
	for (int k = i - 1; k >= 0; k--) if (kQuests[k].giver == kQuests[i].giver) return k;
	return -1;
}

} // namespace SideQ
