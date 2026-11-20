// GmCommands.h: every command a GM can type, for the GM console's Commands tab and its builder
// (UI\GmConsole.cpp). The game server (HG Server HG.cpp, GmConsole.cpp ...) runs them; "lv" is the
// admin level the server asks for. Commands without a leading / are the console's own.
#pragma once

namespace GmCmd {

// What a command's value is, so the builder can offer the right control.
enum Type {
	T_TEXT,		// free text, typed
	T_PLAYER,	// a character name, typed (or Myself)
	T_MAP,		// a map (GmData::kMaps)
	T_MONSTER,	// a monster (GmData::kMonsters)
	T_ITEM,		// an item name (the item list)
	T_SPELL,	// a spell name (the spell list)
	T_FORM,		// a /polymorph shape
	T_INT,		// a number between lo and hi
	T_CHOICE,	// one of choices: "value=label|value=label|..."
};

struct Arg {
	const char * name;
	Type type;
	bool optional;
	int lo, hi, def;			// T_INT
	const char * choices;		// T_CHOICE
};

const int MAX_ARGS = 6;

struct Cmd {
	const char * cat;
	const char * cmd;			// what is sent first ("/summon", or "heal" for the console's own)
	int lv;						// admin level the server needs
	const char * desc;
	Arg args[MAX_ARGS];
};

#define NOARG { NULL, T_TEXT, false, 0, 0, 0, NULL }
#define A_PLAYER(n)			{ n, T_PLAYER, false, 0, 0, 0, NULL }
#define A_PLAYER_OPT(n)		{ n, T_PLAYER, true, 0, 0, 0, NULL }
#define A_TEXT(n)			{ n, T_TEXT, false, 0, 0, 0, NULL }
#define A_MAP(n)			{ n, T_MAP, false, 0, 0, 0, NULL }
#define A_MAP_OPT(n)		{ n, T_MAP, true, 0, 0, 0, NULL }
#define A_MONSTER(n)		{ n, T_MONSTER, false, 0, 0, 0, NULL }
#define A_ITEM(n)			{ n, T_ITEM, false, 0, 0, 0, NULL }
#define A_ITEM_OPT(n)		{ n, T_ITEM, true, 0, 0, 0, NULL }
#define A_SPELL(n)			{ n, T_SPELL, false, 0, 0, 0, NULL }
#define A_INT(n, lo, hi, d)		{ n, T_INT, false, lo, hi, d, NULL }
#define A_INT_OPT(n, lo, hi, d)	{ n, T_INT, true, lo, hi, d, NULL }
#define A_CHOICE(n, c)		{ n, T_CHOICE, false, 0, 0, 0, c }
#define A_CHOICE_OPT(n, c)	{ n, T_CHOICE, true, 0, 0, 0, c }

static const char * const kOnOff = "1=On|0=Off";
static const char * const kWeather = "0=Sunny|1=Light rain|2=Rain|3=Heavy rain|4=Light snow|5=Snow|6=Heavy snow";
static const char * const kSide = "0=Nobody|1=Aresden|2=Elvine";
static const char * const kStat = "str=STR|dex=DEX|vit=VIT|int=INT|mag=MAG|chr=CHR";
static const char * const kRarity = "0=Common|1=Uncommon|2=Rare|3=Epic|4=Legendary";
static const char * const kEvent = "1=Capture the Relic|2=Destroy the Shield|3=Crusade|4=Apocalypse";

static const Cmd kCmds[] = {
	// ---- you
	{ "You", "heal", 1, "Full HP, MP and SP.", { NOARG } },
	{ "You", "town", 1, "Back to your town (Middleland for travellers).", { NOARG } },
	{ "You", "levelup", 1, "Gain levels (up to the level cap).", { A_INT("levels", 1, 140, 1), NOARG } },
	{ "You", "stat", 1, "Raise or lower a stat (10 to the stat limit).", { A_CHOICE("stat", kStat), A_INT("change", -200, 200, 10), NOARG } },
	{ "You", "learn", 1, "Learn every spell.", { NOARG } },
	{ "You", "/sethp", 1, "Set your HP.", { A_INT("HP", 1, 99999, 1000), NOARG } },
	{ "You", "/setmp", 1, "Set your MP.", { A_INT("MP", 0, 99999, 1000), NOARG } },
	{ "You", "/setmag", 1, "Set your MAG.", { A_INT("MAG", 10, 500, 100), NOARG } },
	{ "You", "/invincible", 4, "God mode on / off.", { NOARG } },
	{ "You", "/noaggro", 4, "Monsters ignore you, on / off.", { NOARG } },
	{ "You", "/invi", 1, "Invisible to players, on / off.", { NOARG } },
	{ "You", "/eth", 1, "Ethereal: walk through everything, on / off.", { NOARG } },
	{ "You", "/obs", 3, "Observer mode on / off.", { NOARG } },
	{ "You", "/polymorph", 5, "Take a monster's or NPC's shape (off = yourself).", { { "shape", T_FORM, false, 0, 0, 0, NULL }, NOARG } },
	{ "You", "/afk", 0, "Away from keyboard on / off.", { NOARG } },
	{ "You", "/setpf", 0, "Set your profile text.", { A_TEXT("profile"), NOARG } },

	// ---- travel
	{ "Travel", "/tp", 1, "Teleport to a map; x and y are optional (the map's own spot).", { A_MAP("map"), A_INT_OPT("x", 0, 1000, 100), A_INT_OPT("y", 0, 1000, 100), NOARG } },
	{ "Travel", "/goto", 1, "Jump to a player.", { A_PLAYER("player"), NOARG } },
	{ "Travel", "/summonplayer", 2, "Bring a player to you.", { A_PLAYER("player"), NOARG } },
	{ "Travel", "/send", 1, "Send a player to a map (x and y optional).", { A_PLAYER("player"), A_MAP("map"), A_INT_OPT("x", 0, 1000, 100), A_INT_OPT("y", 0, 1000, 100), NOARG } },

	// ---- players
	{ "Players", "/who", 0, "How many players are online.", { NOARG } },
	{ "Players", "/fi", 1, "Is a player online, and where.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/pf", 0, "Read a player's profile.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/checkrep", 0, "A player's reputation.", { A_PLAYER_OPT("player"), NOARG } },
	{ "Players", "/rep+", 0, "Give a player reputation.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/rep-", 0, "Take a player's reputation.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/shutup", 2, "Mute a player for some minutes (0 unmutes).", { A_PLAYER("player"), A_INT("minutes", 0, 1440, 10), NOARG } },
	{ "Players", "/checkip", 2, "Who is playing from an IP address.", { A_TEXT("IP address"), NOARG } },
	{ "Players", "/closeconn", 2, "Disconnect a player.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/attack", 2, "Make the guards attack a player.", { A_PLAYER("player"), NOARG } },
	{ "Players", "/to", 0, "Whisper to a player (no name: back to normal chat).", { A_PLAYER_OPT("player"), NOARG } },
	{ "Players", "/disconnectall", 5, "Disconnect every player.", { NOARG } },

	// ---- items
	{ "Items", "give", 4, "Give yourself an item (stacks come as one stack).", { A_ITEM("item"), A_INT("amount", 1, 10000, 1), NOARG } },
	{ "Items", "/ci", 4, "Create an item with an attribute and endurance (advanced).", { A_ITEM("item"), A_INT("attribute", 0, 2147483647, 0), A_INT("endurance", 0, 200, 0), A_INT("amount", 1, 10000, 1), NOARG } },
	{ "Items", "gold", 4, "Give yourself gold.", { A_INT("gold", 1, 100000000, 10000), NOARG } },
	{ "Items", "/vuse", 4, "Use a VALORIA item from your bag (on another bag item for runes and stones).", { A_ITEM("item"), A_ITEM_OPT("on item"), NOARG } },
	{ "Items", "/vcraft", 4, "Craft from the bag items named (Crafting Vessel recipes).", { A_ITEM("item 1"), A_ITEM_OPT("item 2"), A_ITEM_OPT("item 3"), A_ITEM_OPT("item 4"), A_ITEM_OPT("item 5"), A_ITEM_OPT("item 6") } },
	{ "Items", "/vrarity", 4, "Give a bag item a rarity and new bonus stats.", { A_ITEM("item"), A_CHOICE_OPT("rarity", kRarity), NOARG } },
	{ "Items", "/createfish", 2, "Put a fish in the water at x, y.", { A_INT("x", 0, 1000, 100), A_INT("y", 0, 1000, 100), A_INT("fish type", 1, 30, 1), NOARG } },

	// ---- monsters
	{ "Monsters", "/summon", 3, "Spawn monsters next to you (ability: 0 none, 1-9 a special ability).", { A_MONSTER("monster"), A_INT("count", 1, 50, 1), A_INT_OPT("ability", 0, 9, 0), NOARG } },
	{ "Monsters", "/vkill", 4, "Kill the monsters within 8 tiles, as your kills.", { NOARG } },
	{ "Monsters", "killall", 1, "Kill every monster on this map, as your kills.", { NOARG } },
	{ "Monsters", "/unsummonall", 5, "Remove every summoned monster.", { NOARG } },
	{ "Monsters", "/unsummonboss", 5, "Remove the summoned bosses.", { NOARG } },
	{ "Monsters", "/mcount", 0, "How many monsters are on the map.", { NOARG } },
	{ "Monsters", "/hold", 0, "Your summons stay where they are.", { NOARG } },
	{ "Monsters", "/tgt", 0, "Your summons attack a player.", { A_PLAYER("player"), NOARG } },
	{ "Monsters", "/free", 0, "Your summons follow you again.", { NOARG } },

	// ---- world
	{ "World", "/setweather", 4, "Weather on this map, for some minutes.", { A_CHOICE("weather", kWeather), A_INT_OPT("minutes", 1, 600, 10), NOARG } },
	{ "World", "/clearmap", 4, "Remove every item on a map's ground.", { A_MAP("map"), NOARG } },
	{ "World", "/setattackmode", 3, "Fight zones: attacking allowed or not.", { A_CHOICE("attacks", kOnOff), NOARG } },
	{ "World", "/setforcerecalltime", 3, "Minutes before players are recalled from fight zones.", { A_INT("minutes", 0, 600, 10), NOARG } },
	{ "World", "/reservefightzone", 2, "Reserve a fight zone for your guild.", { A_INT("fight zone", 1, 9, 1), NOARG } },
	{ "World", "/getticket", 2, "A ticket to your guild's fight zone.", { NOARG } },

	// ---- events (fight zones)
	{ "Events", "/eventspell", 1, "Fight zone: allow or ban a spell.", { A_SPELL("spell"), A_CHOICE_OPT("allowed", kOnOff), NOARG } },
	{ "Events", "/eventarmor", 1, "Fight zone: armour allowed or not (switches).", { NOARG } },
	{ "Events", "/eventshield", 1, "Fight zone: shields allowed or not (switches).", { NOARG } },
	{ "Events", "/eventchat", 1, "Fight zone: chat on or off.", { A_CHOICE_OPT("chat", kOnOff), NOARG } },
	{ "Events", "/eventparty", 1, "Fight zone: parties allowed or not.", { A_CHOICE_OPT("parties", kOnOff), NOARG } },
	{ "Events", "/eventillusion", 1, "Fight zone: everyone looks the same, on or off.", { A_CHOICE_OPT("illusion", kOnOff), NOARG } },
	{ "Events", "/eventtp", 1, "Fight zone: open or close the gates in (1 = donation event, level 4).", { A_CHOICE_OPT("donation", "0=Normal|1=Donation event"), NOARG } },
	{ "Events", "/eventreset", 1, "Fight zone: back to normal rules.", { NOARG } },
	{ "Events", "/soccer", 2, "Start soccer on this map.", { NOARG } },
	{ "Events", "/endsoccer", 2, "End soccer.", { NOARG } },

	// ---- wars
	{ "Wars", "/crusade", 4, "Start the Crusade.", { NOARG } },
	{ "Wars", "/endcrusade", 4, "End the Crusade.", { NOARG } },
	{ "Wars", "/heldenian", 4, "Start the Heldenian.", { NOARG } },
	{ "Wars", "/endheldenian", 4, "End the Heldenian with a winner.", { A_CHOICE("winner", kSide), NOARG } },
	{ "Wars", "/apocalypse", 4, "Start the Apocalypse.", { NOARG } },
	{ "Wars", "/endapocalypse", 4, "End the Apocalypse.", { NOARG } },
	{ "Wars", "/astoria", 4, "Start an Astoria event.", { A_CHOICE("event", kEvent), NOARG } },
	{ "Wars", "/endastoria", 4, "End the Astoria event.", { NOARG } },

	// ---- VALORIA testing
	{ "VALORIA", "/vach", 4, "Add to an achievement counter.", { A_INT("counter", 0, 67, 0), A_INT_OPT("amount", 1, 100000, 1), NOARG } },
	{ "VALORIA", "/vside", 4, "Complete an active side quest's objectives.", { A_INT("quest", 0, 23, 0), NOARG } },

	// ---- guild and party
	{ "Guild", "/ban", 0, "Guild master: remove a member.", { A_PLAYER("player"), NOARG } },
	{ "Guild", "/dissmiss", 0, "Guild master: disband your guild.", { NOARG } },
	{ "Guild", "/dismissparty", 0, "Leave your party.", { NOARG } },

	// ---- server
	{ "Server", "/shutdownthisserverrightnow", 5, "Shut the game server down now (click Run twice).", { A_TEXT("reason"), NOARG } },
};
const int CMD_COUNT = (int)(sizeof(kCmds) / sizeof(kCmds[0]));

static const char * const kCats[] = { "You", "Travel", "Players", "Items", "Monsters", "World", "Events", "Wars", "VALORIA", "Guild", "Server" };
const int CAT_COUNT = (int)(sizeof(kCats) / sizeof(kCats[0]));

// /polymorph's shapes (HG Server AdminOrder_Polymorph)
static const char * const kForms[] = { "off", "Slime", "Skeleton", "Stone-Golem", "Cyclops", "Orc", "ShopKeeper", "Giant-Ant", "Scorpion",
	"Zombie", "Gandlf", "Howard", "Gaurd", "Amphis", "Clay-Golem", "Tom", "William", "Kennedy", "Hellbound", "Troll", "Orge", "Liche",
	"Demon", "Unicorn", "WereWolf", "Dummy", "Energy-Sphere", "AGT", "CGT", "MS", "DT", "ESG", "GMG", "ManaStone", "LWB", "GHK", "GHC",
	"TK", "BG", "Stalker", "Hellclaw", "Tigerworm", "CP", "Gagoyle", "Beholder", "Dark-Elf", "Rabbit", "Cat", "Giant-Frog",
	"Mountain-Giant", "Ettin", "Cannibal-Plant", "Rudolph", "DireBoar", "Frost", "Crops", "Ice-Golem", "Wyvern", "McGaffin", "Perry",
	"Devlin", "Barlog", "Centaurus", "Claw-Turtle", "Fire-Wyvern", "Giant-Crayfish", "Giant-Lizard", "Giant-Plant", "MasterMage-Orc",
	"Minotaurs", "Nizie", "Tentocle", "Abaddon", "Sor", "ATK", "Elf", "DSK", "HBT", "CT", "Bar", "AGC", "Gail", "gate" };
const int FORM_COUNT = (int)(sizeof(kForms) / sizeof(kForms[0]));

#undef NOARG
#undef A_PLAYER
#undef A_PLAYER_OPT
#undef A_TEXT
#undef A_MAP
#undef A_MAP_OPT
#undef A_MONSTER
#undef A_ITEM
#undef A_ITEM_OPT
#undef A_SPELL
#undef A_INT
#undef A_INT_OPT
#undef A_CHOICE
#undef A_CHOICE_OPT

} // namespace GmCmd
