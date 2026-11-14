#include "..\Shared\Maths.h"
#include "hg.h"
#include "DropManager.h"
#include "ui\winmain.h"
#include "char\Npc.h"

#include "pugixml/pugixml.hpp"
#include <stdexcept>

extern class CGame *   g_game;

DropManager::DropManager(void)
{
	_dropCount = 0;

	for(int npc=0; npc < MAXNPCTYPES; npc++)
	{
		for(int tier=0; tier < MAXNPCDROPTIERS; tier++)
		{
			_npcDrops[npc].tiersNum = 0;
			_npcDrops[npc].tierProbSum = 0;
			_npcDrops[npc].tiers[tier].prob = 0;
			_npcDrops[npc].tiers[tier].dropsNum = 0;
			_npcDrops[npc].tiers[tier].dropProbSum = 0;;
			for(int drop=0; drop < MAXNPCDROPS; drop++)
			{
				_npcDrops[npc].tiers[tier].drops[drop].SetDrop(0,0);
			}


			_npcDeleteDrops[npc].tiersNum = 0;
			_npcDeleteDrops[npc].tierProbSum = 0;
			_npcDeleteDrops[npc].tiers[tier].prob = 0;
			_npcDeleteDrops[npc].tiers[tier].dropsNum = 0;
			_npcDeleteDrops[npc].tiers[tier].dropProbSum = 0;
			for(int drop=0; drop < MAXNPCDROPS; drop++)
			{
				_npcDeleteDrops[npc].tiers[tier].drops[drop].SetDrop(0,0);
			}
		}
	}
}

DropManager::~DropManager(void)
{
}

void DropManager::InitDrops(void)
{
	try
	{
		_ParseXML();
	}
	catch (const std::exception& e)
	{
		string exc = "XML ERROR: ";
		exc += e.what();
		LogError(exc);
		return;
	}

	sstream err;
	err << "Total drop items: " << _dropCount;
	PutLogList(err.str());
}

//////////////////////////////////////////////////////////////////////
// droplist.xml reading (pugixml; replaces the Xerces-C / CodeSynthesis XSD generated parser)
//
// Format, as defined by droplist.xsd:
//   <droplist>
//     <npc_t name="..." [num="N"]>                 name required, num optional (default 1)
//       <tier_t type="primary|secondary" prob="N">  both required
//         <item_t name="..." prob="N"/>             both required
// Every number must be a positive whole number, and each npc_t / tier_t needs at least one child,
// like the schema required. Any violation rejects the whole file, as the old parser did.
//////////////////////////////////////////////////////////////////////

namespace
{
	const char * const DROPLIST_FILE = "..\\configs\\droplist.xml";

	struct DropListSource
	{
		std::vector<char> text;

		int LineOf(const pugi::xml_node & node) const
		{
			ptrdiff_t off = node.offset_debug();
			if (off < 0 || (size_t)off > text.size()) return 0;
			int line = 1;
			for (ptrdiff_t i = 0; i < off; i++) if (text[i] == '\n') line++;
			return line;
		}

		[[noreturn]] void Fail(const pugi::xml_node & node, const string & what) const
		{
			sstream msg;
			msg << DROPLIST_FILE << " line " << LineOf(node) << ": " << what;
			throw std::runtime_error(msg.str());
		}

		string RequiredText(const pugi::xml_node & node, const char * attr) const
		{
			pugi::xml_attribute a = node.attribute(attr);
			if (!a) Fail(node, string("<") + node.name() + "> is missing required attribute \"" + attr + "\"");
			return a.value();
		}

		uint32 PositiveInt(const pugi::xml_node & node, const char * attr, const char * value) const
		{
			// xsd:positiveInteger: digits only (surrounding whitespace allowed), value >= 1
			const char * p = value;
			while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
			const char * digits = p;
			unsigned long long v = 0;
			while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); if (v > 0x7FFFFFFF) break; p++; }
			const bool hadDigits = p != digits;
			while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
			if (!hadDigits || *p != '\0' || v < 1 || v > 0x7FFFFFFF)
				Fail(node, string("attribute \"") + attr + "\" of <" + node.name() + "> must be a positive whole number (got \"" + value + "\")");
			return (uint32)v;
		}

		uint32 RequiredInt(const pugi::xml_node & node, const char * attr) const
		{
			pugi::xml_attribute a = node.attribute(attr);
			if (!a) Fail(node, string("<") + node.name() + "> is missing required attribute \"" + attr + "\"");
			return PositiveInt(node, attr, a.value());
		}

		uint32 OptionalInt(const pugi::xml_node & node, const char * attr, uint32 defaultValue) const
		{
			pugi::xml_attribute a = node.attribute(attr);
			return a ? PositiveInt(node, attr, a.value()) : defaultValue;
		}

		// Element children of `parent`; anything other than <childName> is an error.
		std::vector<pugi::xml_node> Children(const pugi::xml_node & parent, const char * childName) const
		{
			std::vector<pugi::xml_node> out;
			for (pugi::xml_node c = parent.first_child(); c; c = c.next_sibling()) {
				if (c.type() != pugi::node_element) continue;
				if (strcmp(c.name(), childName) != 0)
					Fail(c, string("unexpected element <") + c.name() + "> inside <" + parent.name() + "> (expected <" + childName + ">)");
				out.push_back(c);
			}
			if (out.empty())
				Fail(parent, string("<") + parent.name() + "> must contain at least one <" + childName + ">");
			return out;
		}
	};

	// In-memory copy of the file, fully validated before anything is applied.
	struct ItemEntry { string name; uint32 prob; };
	struct TierEntry { string type; uint32 prob; std::vector<ItemEntry> items; };
	struct NpcEntry  { string name; uint32 num; std::vector<TierEntry> tiers; };

	std::vector<NpcEntry> LoadDropList()
	{
		DropListSource src;
		FILE * f = fopen(DROPLIST_FILE, "rb");
		if (f == NULL) throw std::runtime_error(string("cannot open ") + DROPLIST_FILE);
		char buf[16384];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0) src.text.insert(src.text.end(), buf, buf + n);
		fclose(f);

		pugi::xml_document doc;
		pugi::xml_parse_result res = doc.load_buffer(src.text.empty() ? "" : &src.text[0], src.text.size());
		if (!res) {
			int line = 1;
			for (ptrdiff_t i = 0; i < res.offset && (size_t)i < src.text.size(); i++) if (src.text[i] == '\n') line++;
			sstream msg;
			msg << DROPLIST_FILE << " line " << line << ": " << res.description();
			throw std::runtime_error(msg.str());
		}

		pugi::xml_node root = doc.document_element();
		if (!root || strcmp(root.name(), "droplist") != 0)
			throw std::runtime_error(string(DROPLIST_FILE) + ": root element must be <droplist>");

		std::vector<NpcEntry> npcs;
		std::vector<pugi::xml_node> npcNodes = src.Children(root, "npc_t");
		for (size_t i = 0; i < npcNodes.size(); i++) {
			const pugi::xml_node & npcNode = npcNodes[i];
			NpcEntry npc;
			npc.name = src.RequiredText(npcNode, "name");
			npc.num  = src.OptionalInt(npcNode, "num", 1);
			std::vector<pugi::xml_node> tierNodes = src.Children(npcNode, "tier_t");
			for (size_t t = 0; t < tierNodes.size(); t++) {
				TierEntry tier;
				tier.prob = src.RequiredInt(tierNodes[t], "prob");
				tier.type = src.RequiredText(tierNodes[t], "type");
				std::vector<pugi::xml_node> itemNodes = src.Children(tierNodes[t], "item_t");
				for (size_t k = 0; k < itemNodes.size(); k++) {
					ItemEntry item;
					item.name = src.RequiredText(itemNodes[k], "name");
					item.prob = src.RequiredInt(itemNodes[k], "prob");
					tier.items.push_back(item);
				}
				npc.tiers.push_back(tier);
			}
			npcs.push_back(npc);
		}
		return npcs;
	}
}

void DropManager::_ParseXML()
{
	const std::vector<NpcEntry> dl = LoadDropList();

	for (std::vector<NpcEntry>::const_iterator npc = dl.begin();
		npc != dl.end();
		++npc)
	{
		string npcName;
		npcName = npc->name;
		uint32 npcType = g_game->FindNPC(npcName);
		if(npcType == NPC_INVALID)	
		{
			sstream err;
			err << "Unknown NPC in Droplist: "<< npcName;
			LogError(err.str());
			continue;
		}
		else if(HasPrimaryDrop(npcType) || HasSecondaryDrop(npcType))
		{
			sstream err;
			err << "Duplicated NPC in Droplist: "<< npcName;
			LogError(err.str());
			continue;
		}

		for (std::vector<TierEntry>::const_iterator t = npc->tiers.begin();
			t != npc->tiers.end();
			++t)
		{
			Tier * tier;
			NpcDrop * drop;

			if(t->type == "primary")
			{
				drop = &_npcDrops[npcType];
				tier = drop->AddTier(t->prob);
			}
			else
			{
				drop = &_npcDeleteDrops[npcType];
				tier = _npcDeleteDrops[npcType].AddTier(t->prob);
			}

			drop->secDropNum = npc->num;

			if(!tier)
			{
				sstream err;
				err << "Too many a Tier in NPC("<< npcName <<')';
				LogError(err.str());	
				continue;
			}

			for (std::vector<ItemEntry>::const_iterator item = t->items.begin();
				item != t->items.end();
				++item)
			{
				string itemName = item->name;

				uint32 itemID;
				if(itemName == "nothing")
					itemID = ITEM_NONE;
				else
					itemID = g_game->FindItem(itemName);

				if(itemID == ITEM_INVALID)	
				{
					sstream err;
					err << "Unknown item("<< itemName <<") in NPC("<< npcName << ')';
					LogError(err.str());
					continue;
				}

				if(! tier->AddDrop( itemID, item->prob) )
				{
					sstream err;
					err << "Too many a Drop("<< itemName <<") in NPC("<< npcName <<')';
					LogError(err.str());	
					continue;
				}

				_dropCount++;
			}
		}
	}
}

long DropManager::Roll(const CNpc * npc, long dropType) const
{
	const NpcDrop * drop;
	const Tier * tier = 0;
	float factor = 1.0f;

	switch(dropType)
	{
	case ONNPCDEATH:
		drop = &_npcDrops[npc->m_sType];
		break;
	case ONNPCDELETE:
		drop = &_npcDeleteDrops[npc->m_sType];
		if(Unit * killer = (Unit*)npc->GetKiller())
			factor = killer->GetDropFactor();

		if(g_game->m_iMiddlelandMapIndex != -1 && g_game->m_iMiddlelandMapIndex == npc->m_cMapIndex)
		{
			factor *= 0.9f;
		}

		if(npc->IsHighlyTrained())
			factor *= 0.7f; 
		break;
	default:
		return ITEM_NONE;
	}


	long tierRoll = dice(1, drop->tierProbSum);
	for(int n=0; n < drop->tiersNum; n++)
	{
		if(tierRoll <= drop->tiers[n].prob){
			tier = &drop->tiers[n];
			break;
		}
		tierRoll -= drop->tiers[n].prob;
	}

	if(!tier || tier->dropsNum == 0) return ITEM_NONE;


	long firstDropProb = tier->drops[0].prob * factor;

	long dropRoll = dice(1, tier->dropProbSum - (tier->drops[0].prob - firstDropProb));

	if(dropRoll <= firstDropProb){
		return tier->drops[0].itemID;
	}
	dropRoll -= firstDropProb;

	for(int n=1; n < tier->dropsNum; n++)
	{
		if(dropRoll <= tier->drops[n].prob){
			return tier->drops[n].itemID;
		}
		dropRoll -= tier->drops[n].prob;
	}

	return ITEM_NONE;
}

bool DropManager::HasPrimaryDrop(NpcID npcType) const
{
	return (_npcDrops[npcType].tiersNum > 0) ? true : false;
}
bool DropManager::HasPrimaryDrop(const CNpc * npc) const
{
	return (_npcDrops[npc->m_sType].tiersNum > 0) ? true : false;
}

bool DropManager::HasSecondaryDrop(NpcID npcType) const
{
	return (_npcDeleteDrops[npcType].tiersNum > 0) ? true : false;
}
bool DropManager::HasSecondaryDrop(const CNpc * npc) const
{
	return (_npcDeleteDrops[npc->m_sType].tiersNum > 0) ? true : false;
}
