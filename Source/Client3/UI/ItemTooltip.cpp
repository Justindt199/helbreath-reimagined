// ItemTooltip.cpp: one item tooltip for the whole game (bag, character window, banks, trade
// window, chat links).
//
// BuildItemTip turns an item into labelled parts (name, type, bonuses, gems, endurance,
// requirements, weight / value, stack count); DrawItemTip draws them in the same see-through
// panel style as the chat panel and character window, in the item's quality colour.
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"

#include <algorithm>
#include <string>
#include <vector>

#if LANGUAGE == ENGLISH
#include "..\lan_eng.h"
#elif LANGUAGE == SPANISH
#include "..\lan_spa.h"
#endif

namespace {

const COLORREF kGreen	= RGB(126, 227, 138);
const COLORREF kGold	= RGB(242, 207, 115);
const COLORREF kGrey	= RGB(185, 192, 201);
const COLORREF kDim		= RGB(111, 119, 130);
const COLORREF kWhite	= RGB(240, 242, 245);
const COLORREF kRed		= RGB(255, 122, 122);
const COLORREF kWarn	= RGB(224, 112, 58);

int TextW(CRenderer & r, const std::string & s, int iPx = 0)
{
	if (s.empty()) return 0;
	return (iPx > 0) ? r.TextWidthSized(s.c_str(), iPx) : (int)r.GetTextExtent(s.c_str()).cx;
}

void Bold(CRenderer & r, int x, int y, const std::string & s, COLORREF c)
{
	r.TextOut(x, y, s.c_str(), c);
	r.TextOut(x + 1, y, s.c_str(), c);
}

std::string Trim(const std::string & s)
{
	size_t a = 0, b = s.size();
	while (a < b && s[a] == ' ') a++;
	while (b > a && s[b - 1] == ' ') b--;
	return s.substr(a, b - a);
}

std::string Commas(long long v)
{
	std::string s = std::to_string(v < 0 ? -v : v);
	for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
	return (v < 0) ? "-" + s : s;
}

// a socket gem's dot colour, from its family
COLORREF GemColor(uint8 gem)
{
	if (gem >= SG_MAX || gem == 0) return kGold;
	const std::string n = SocketGems[gem];
	if (n.find("Reju") == 0) return RGB(110, 210, 120);
	if (n.find("Blood") == 0) return RGB(226, 85, 85);
	if (n.find("Mind") == 0) return RGB(100, 150, 240);
	if (n.find("Armor") == 0) return RGB(170, 178, 190);
	if (n.find("Enchanted") == 0) return RGB(186, 120, 236);
	if (n.find("Tactical") == 0) return RGB(236, 160, 70);
	return kGold;
}

const char * SlotName(int iPos)
{
	switch (iPos) {
	case EQUIPPOS_HEAD:		return "Helmet";
	case EQUIPPOS_BODY:		return "Body armour";
	case EQUIPPOS_ARMS:		return "Arm armour";
	case EQUIPPOS_PANTS:	return "Leggings";
	case EQUIPPOS_BOOTS:	return "Boots";
	case EQUIPPOS_NECK:		return "Necklace";
	case EQUIPPOS_LHAND:	return "Shield";
	case EQUIPPOS_RHAND:	return "One-handed weapon";
	case EQUIPPOS_TWOHAND:	return "Two-handed weapon";
	case EQUIPPOS_RFINGER:	return "Ring";
	case EQUIPPOS_LFINGER:	return "Angel";
	case EQUIPPOS_BACK:		return "Cape";
	case EQUIPPOS_FULLBODY:	return "Robe";
	}
	return NULL;
}

const char * TypeName(int iType)
{
	switch (iType) {
	case ITEMTYPE_USE_DEPLETE:
	case ITEMTYPE_EAT:				return "Consumable";
	case ITEMTYPE_ARROW:			return "Arrows";
	case ITEMTYPE_MATERIAL:			return "Material";
	case ITEMTYPE_USE_DEPLETE_DEST:	return "Use on an item";
	case ITEMTYPE_USE_SKILL:
	case ITEMTYPE_USE_SKILL_ENABLEDIALOGBOX:	return "Tool";
	case ITEMTYPE_RELIC:			return "Relic";
	}
	return NULL;
}

} // namespace

// pItem gives everything; without it, pName / attr / pSockets / count describe the item (chat
// links, trades) and the fixed facts (type, weight, ...) are borrowed from an item of the same
// name the player owns, if any.
void CGame::BuildItemTip(ItemTip & t, const CItem * pItem, const char * pName, DWORD attr, const uint8 * pSockets, DWORD count)
{
	t = ItemTip();
	char cName[24] = {};
	uint8 sockets[MAXITEMSOCKETS];
	for (int k = 0; k < MAXITEMSOCKETS; k++) sockets[k] = SG_NONE;
	if (pItem != NULL) {
		strncpy(cName, pItem->m_cName, 20);
		attr = pItem->m_dwAttribute;
		memcpy(sockets, pItem->m_sockets, sizeof(sockets));
		count = pItem->m_dwCount;
	}
	else {
		if (pName == NULL) return;
		strncpy(cName, pName, 20);
		if (pSockets != NULL) memcpy(sockets, pSockets, sizeof(sockets));
	}

	// fixed facts about this kind of item: from the item itself, or one of the same name we own
	const CItem * pKind = pItem;
	if (pKind == NULL) {
		for (int i = 0; i < MAXITEMS && pKind == NULL; i++)
			if (m_pItemList[i] != NULL && strcmp(m_pItemList[i]->m_cName, cName) == 0) pKind = m_pItemList[i];
		for (int i = 0; i < MAXBANKITEMS && pKind == NULL; i++)
			if (m_pBankList[i] != NULL && strcmp(m_pBankList[i]->m_cName, cName) == 0) pKind = m_pBankList[i];
	}

	// display name
	std::string base = cName;
	for (int i = 0; i < MAXITEMNAMES; i++)
		if (m_pItemNameList[i] != NULL && strcmp(m_pItemNameList[i]->m_cOriginName, cName) == 0) { base = m_pItemNameList[i]->m_cName; break; }

	if (strcmp(cName, "ScrollofRenown") == 0) {
		t.name = base;
		char cBuf[128];
		wsprintf(cBuf, GET_ITEM_NAME39, attr);
		t.info.push_back(cBuf);
		t.type = "Scroll";
		if (pKind != NULL) { t.sprite = pKind->m_sSprite; t.frame = pKind->m_sSpriteFrame; t.itemColor = pKind->m_cItemColor; }
		return;
	}

	const bool bManued = (attr & 1) != 0;
	const bool bVortex = (sockets[0] == SG_VORTEXGEM);
	std::string quality;
	if (bVortex) { t.color = RGB(132, 132, 180); quality = "Vortexed"; }

	// prefix (Sharp, Ancient, ...) and its bonus
	static const char * const kPrefix[13] = { "", GET_ITEM_NAME3, GET_ITEM_NAME4, GET_ITEM_NAME5, "", GET_ITEM_NAME6,
		GET_ITEM_NAME7, GET_ITEM_NAME8, GET_ITEM_NAME9, GET_ITEM_NAME10, GET_ITEM_NAME11, GET_ITEM_NAME12, GET_ITEM_NAME13 };
	const int iPrefix = (int)GetNibble(attr, 5);
	const std::string prefix = (iPrefix > 0 && iPrefix < 13) ? Trim(kPrefix[iPrefix]) : std::string();
	t.name = prefix.empty() ? base : prefix + " " + base;
	t.upgrade = (int)GetNibble(attr, 7);

	if (bManued) { t.color = RGB(172, 172, 95); if (quality.empty()) quality = "Crafted"; }
	if (iPrefix != 0 && !bVortex) { t.color = RGB(90, 220, 90); if (quality.empty()) quality = "Enchanted"; }

	char cBuf[160];
	{
		const int v = (int)GetNibble(attr, 4);
		cBuf[0] = 0;
		switch (iPrefix) {
		case 1: wsprintf(cBuf, GET_ITEM_NAME14, v); break;
		case 2: wsprintf(cBuf, GET_ITEM_NAME15, v * 5); break;
		case 5: strcpy(cBuf, GET_ITEM_NAME16); break;
		case 6: wsprintf(cBuf, GET_ITEM_NAME17, v * 4); break;
		case 7: strcpy(cBuf, GET_ITEM_NAME18); break;
		case 8: wsprintf(cBuf, GET_ITEM_NAME19, v * 7); break;
		case 9: strcpy(cBuf, GET_ITEM_NAME20); break;
		case 10: wsprintf(cBuf, GET_ITEM_NAME21, v * 3); break;
		case 11: wsprintf(cBuf, GET_ITEM_NAME22, v); break;
		case 12: wsprintf(cBuf, GET_ITEM_NAME23, v); break;
		}
		if (cBuf[0]) t.bonus.push_back({ cBuf, prefix.empty() ? "prefix" : prefix, kGreen });
	}
	{
		const int v = (int)GetNibble(attr, 2);
		cBuf[0] = 0;
		switch (GetNibble(attr, 3)) {
		case 1:  wsprintf(cBuf, GET_ITEM_NAME24, v * 7); break;
		case 2:  wsprintf(cBuf, GET_ITEM_NAME25, v * 7); break;
		case 3:  wsprintf(cBuf, GET_ITEM_NAME26, v * 7); break;
		case 4:  wsprintf(cBuf, GET_ITEM_NAME27, v * 7); break;
		case 5:  wsprintf(cBuf, GET_ITEM_NAME28, v * 7); break;
		case 6:  wsprintf(cBuf, GET_ITEM_NAME29, v * 7); break;
		case 7:  wsprintf(cBuf, GET_ITEM_NAME30, v * 7); break;
		case 8:  wsprintf(cBuf, GET_ITEM_NAME31, v * 3); break;
		case 9:  wsprintf(cBuf, GET_ITEM_NAME32, v * 3); break;
		case 10: wsprintf(cBuf, GET_ITEM_NAME33, v); break;
		case 11: wsprintf(cBuf, GET_ITEM_NAME34, v * 10); break;
		case 12: wsprintf(cBuf, GET_ITEM_NAME35, v * 10); break;
		}
		if (cBuf[0]) t.bonus.push_back({ cBuf, "bonus", kGreen });
	}
	// rings with a fixed bonus
	{
		static const struct { const char * name; int v; bool magic; } kRings[] = {
			{ "RingofOgrepower", 2, false }, { "RingofDemonpower", 4, false }, { "RingofDragonpower", 5, false },
			{ "RingoftheXelima", 7, false }, { "RingoftheAbaddon", 10, false }, { "RingofWizard", 2, true },
			{ "RingofMage", 4, true }, { "RingofGrandMage", 5, true }, { "RingofArchmage", 7, true } };
		for (size_t i = 0; i < sizeof(kRings) / sizeof(kRings[0]); i++)
			if (strcmp(cName, kRings[i].name) == 0) {
				wsprintf(cBuf, "+%d %s", kRings[i].v, kRings[i].magic ? LNG_MAGICDAMAGE : LNG_DAMAGE);
				t.bonus.push_back({ cBuf, "ring", kGreen });
			}
	}
	// crafting result
	if (bManued && pItem != NULL) {
		if (pItem->m_cItemType == ITEMTYPE_MATERIAL) wsprintf(cBuf, GET_ITEM_NAME1, pItem->m_sItemSpecEffectValue2);
		else if (pItem->m_cEquipPos == EQUIPPOS_LFINGER) wsprintf(cBuf, GET_ITEM_NAME2, pItem->m_sItemSpecEffectValue2);
		else wsprintf(cBuf, GET_ITEM_NAME2, pItem->m_sItemSpecEffectValue2 + 100);
		t.bonus.push_back({ cBuf, "crafted", RGB(214, 206, 140) });
	}

	EnchantTipLines(t, pItem);	// VALORIA enchantments and empowering (UI\EnchantPanel.cpp)
	ValoriaUseTipLines(t, pItem);	// VALORIA runes, elixirs, scrolls, Blessed Upgrade Stone (UI\ValoriaUse.cpp)

	// socketed gems, and sockets still free
	for (int k = 0; k < MAXITEMSOCKETS; k++) {
		const uint8 g = sockets[k];
		if (g == SG_NONE || g == SG_VORTEXGEM || g == 0 || g >= SG_MAX) continue;
		std::string gemName = SocketGems[g];
		for (int j = 0; j < MAXITEMNAMES; j++)
			if (m_pItemNameList[j] != NULL && strcmp(m_pItemNameList[j]->m_cOriginName, SocketGems[g]) == 0) { gemName = m_pItemNameList[j]->m_cName; break; }
		CItem tmp;
		char cAttr[128] = {};
		tmp.GetGemAttr(cAttr, g);
		t.gems.push_back({ cAttr[0] ? gemName + ": " + cAttr : gemName, GemColor(g) });
	}
	if (pItem != NULL) {
		int iMax = (int)pItem->GetMaxSockets();
		if (bVortex && iMax > 0) iMax--;
		t.emptySockets = (std::max)(0, iMax - (int)pItem->GetNUsedSockets());
	}

	// gems and other use-on-an-item things say what they do
	const int iType = (pKind != NULL) ? pKind->m_cItemType : -1;
	if (iType == ITEMTYPE_USE_DEPLETE_DEST) {
		if (strcmp(cName, "VortexGem") == 0) t.info.push_back(GET_ITEM_NAME38);
		else {
			CItem tmp;
			strncpy(tmp.m_cName, cName, 20);
			char cAttr[128] = {};
			tmp.GetGemAttr(cAttr);
			if (cAttr[0]) { t.info.push_back(cAttr); t.info.push_back(GET_ITEM_NAME37); }
		}
	}
	if (strcmp(cName, "ZemstoneofSacrifice") == 0 && pItem != NULL) {
		wsprintf(cBuf, GET_ITEM_NAME36, pItem->m_wCurLifeSpan);
		t.info.push_back(cBuf);
	}

	// type line
	const int iPos = (pKind != NULL) ? pKind->m_cEquipPos : EQUIPPOS_NONE;
	const char * pSlot = SlotName(iPos);
	const char * pType = (pSlot == NULL && iType >= 0) ? TypeName(iType) : NULL;
	std::string type = pSlot ? pSlot : (pType ? pType : "");
	if (!quality.empty()) type = type.empty() ? quality : type + " - " + quality;
	t.type = type;

	// endurance / uses (only when we know this very item)
	if (pItem != NULL) {
		if (pItem->m_cEquipPos != EQUIPPOS_NONE && pItem->m_wMaxLifeSpan > 0) {
			t.durCur = pItem->m_wCurLifeSpan;
			t.durMax = pItem->m_wMaxLifeSpan;
		}
		else if (pItem->m_cItemType == ITEMTYPE_USE_DEPLETE_DEST && strcmp(cName, "ZemstoneofSacrifice") != 0) {
			wsprintf(cBuf, UPDATE_SCREEN_ONGAME15, pItem->m_wCurLifeSpan);
			t.info.push_back(cBuf);
		}
	}

	// requirements, weight, value, icon
	if (pKind != NULL) {
		if (pKind->m_sLevelLimit != 0 && !bManued) t.reqLevel = pKind->m_sLevelLimit;
		if (pKind->m_cEquipPos != EQUIPPOS_NONE && pKind->m_wWeight >= 1100)
			t.reqStr = pKind->m_wWeight / 100 + ((pKind->m_wWeight % 100) ? 1 : 0);
		t.reqGender = pKind->m_cGenderLimit;
		t.weight = pKind->m_wWeight;
		if (pKind->m_wPrice > 0) t.value = pKind->m_wPrice;
		t.sprite = pKind->m_sSprite;
		t.frame = pKind->m_sSpriteFrame;
		t.itemColor = (pItem != NULL) ? pItem->m_cItemColor : pKind->m_cItemColor;
		t.weaponTint = (pKind->m_cEquipPos == EQUIPPOS_LHAND || pKind->m_cEquipPos == EQUIPPOS_RHAND || pKind->m_cEquipPos == EQUIPPOS_TWOHAND);
	}

	// stacks: how many here, and how many the player has in the bag
	if (count > 1 && !bManued) t.count = count;
	if (pItem != NULL && pItem->m_cEquipPos == EQUIPPOS_NONE) {	// potions, arrows, materials, ...
		DWORD total = 0;
		for (int i = 0; i < MAXITEMS; i++)
			if (m_pItemList[i] != NULL && strcmp(m_pItemList[i]->m_cName, cName) == 0) total += (std::max)((DWORD)1, m_pItemList[i]->m_dwCount);
		if (total > t.count) t.total = total;
	}
}

void CGame::DrawItemTipFor(const CItem * pItem, int mx, int my)
{
	if (pItem == NULL) return;
	ItemTip t;
	BuildItemTip(t, pItem);
	DrawItemTip(t, mx, my);
}

// The tooltip next to the mouse (below-right, or above when bAbove), kept on screen.
void CGame::DrawItemTip(const ItemTip & t, int mx, int my, bool bAbove)
{
	CRenderer & R = m_Renderer;
	const int kPad = 10, kLH = 17;
	const int stripH = t.header.empty() ? 0 : 22;	// the header strip above the name
	bool bIcon = t.showIcon && t.sprite >= 0 && t.sprite < 200 && m_pSprite[SPRID_ITEMPACK_PIVOTPOINT + t.sprite] != NULL;
	// the picture's real size, so the name and stats are laid out around it (no box behind it)
	int picW = 0, picH = 0;
	const stBrush * pBrush = NULL;
	if (bIcon) {
		CSprite * pSpr = m_pSprite[SPRID_ITEMPACK_PIVOTPOINT + t.sprite];
		if (pSpr->m_stBrush != NULL && t.frame >= 0 && t.frame < pSpr->m_iTotalFrame && pSpr->m_stBrush[t.frame].szx > 0) {
			pBrush = &pSpr->m_stBrush[t.frame];
			picW = (std::min)((int)pBrush->szx, 120);
			picH = (std::min)((int)pBrush->szy, 140);
		}
		else bIcon = false;
	}
	const bool bWorn = (t.durMax > 0 && t.durCur * 100 < t.durMax * 15);	// under 15%: nearly broken
	const COLORREF border = bWorn ? kWarn : t.color;
	const bool bFemale = (m_sPlayerType >= 4 && m_sPlayerType <= 6);

	// pieces of text, to measure the width
	char cUp[8] = "";
	if (t.upgrade > 0) wsprintf(cUp, " +%d", t.upgrade);
	std::string countTag;
	if (t.count > 1) countTag = "x" + Commas(t.count);

	std::string req;
	std::vector<std::pair<std::string, bool>> reqParts;	// text, met
	if (t.reqLevel > 0) reqParts.push_back({ "Level " + std::to_string(t.reqLevel), m_iLevel >= t.reqLevel });
	if (t.reqStr > 0) reqParts.push_back({ "STR " + std::to_string(t.reqStr), m_stat[STAT_STR] >= t.reqStr });
	if (t.reqGender == 1) reqParts.push_back({ "Male only", !bFemale });
	else if (t.reqGender == 2) reqParts.push_back({ "Female only", bFemale });
	for (size_t i = 0; i < reqParts.size(); i++) req += (i ? ", " : "") + reqParts[i].first;

	std::string weightTxt, valueTxt;
	if (t.weight >= 0) {
		char c[32];
		wsprintf(c, "Weight %d.%d%s", t.weight / 100, (t.weight % 100) / 10, t.count > 1 ? " each" : "");
		weightTxt = c;
	}
	if (t.value > 0) valueTxt = "Value " + Commas(t.value) + " gold";

	int w = R.TitleWidth(t.name.c_str()) + TextW(R, cUp) + (countTag.empty() ? 0 : TextW(R, countTag, 12) + 16);
	w = (std::max)(w, TextW(R, t.type, 12));
	if (bIcon) w += picW + 12;
	for (size_t i = 0; i < t.bonus.size(); i++) w = (std::max)(w, TextW(R, t.bonus[i].text) + 16 + TextW(R, t.bonus[i].source, 12));
	for (size_t i = 0; i < t.gems.size(); i++) w = (std::max)(w, TextW(R, t.gems[i].text) + 18);
	for (size_t i = 0; i < t.info.size(); i++) w = (std::max)(w, TextW(R, t.info[i]));
	if (!req.empty()) w = (std::max)(w, TextW(R, "Requires") + 16 + TextW(R, req));
	if (t.durMax > 0) w = (std::max)(w, TextW(R, "Endurance") + 16 + TextW(R, Commas(t.durCur) + " / " + Commas(t.durMax)));
	if (t.total > 0) w = (std::max)(w, TextW(R, "You have") + 16 + TextW(R, Commas(t.total) + " in total"));
	const char * const pWornMsg = "Nearly broken - repair it at the blacksmith";
	if (bWorn) w = (std::max)(w, TextW(R, pWornMsg));
	if (!weightTxt.empty() || !valueTxt.empty()) w = (std::max)(w, TextW(R, weightTxt, 12) + 20 + TextW(R, valueTxt, 12));
	if (!t.header.empty()) w = (std::max)(w, TextW(R, t.header, 12));
	w = (std::max)(w, 190) + kPad * 2;
	w = (std::min)(w, 400);

	// height: header, then sections with a 1px line between them
	const int textH = t.type.empty() ? 30 : 42;
	const int headH = stripH + (bIcon ? (std::max)(textH, picH + 14) : textH);
	int h = headH;
	auto sec = [&](int lines) { return lines > 0 ? lines * kLH + 10 : 0; };
	h += sec((int)t.bonus.size());
	h += sec((int)t.gems.size() + t.emptySockets);
	h += sec((int)t.info.size());
	int iLow = (t.durMax > 0 ? 1 : 0) + (req.empty() ? 0 : 1) + (t.total > 0 ? 1 : 0) + (bWorn ? 1 : 0);
	h += sec(iLow) + (t.durMax > 0 ? 7 : 0);
	const bool bFoot = !weightTxt.empty() || !valueTxt.empty();
	if (bFoot) h += 24;

	int x = mx + 18, y = bAbove ? my - h - 8 : my + 10;
	if (x + w > GAME_W - 2) x = mx - w - 12;
	if (x < 2) x = 2;
	if (y + h > GAME_H - 2) y = GAME_H - 2 - h;
	if (y < 2) y = 2;

	R.FillPanel(x, y, x + w, y + h, 8, RGB(9, 10, 13), 242, border, 220);
	// a faint tint of the quality colour behind the header
	R.FillPanel(x + 1, y + 1, x + w - 1, y + headH, 7, border, 22);

	// header strip (chat links): "ITEM LINK - from ..."
	if (stripH > 0) {
		R.FillPanel(x + 1, y + 1, x + w - 1, y + stripH, 7, border, 40);
		R.TextOutSized(x + kPad, y + 4, t.header.c_str(), RGB(214, 200, 160), 12);
		R.FillPanel(x + 1, y + stripH, x + w - 1, y + stripH + 1, 0, border, 110);
	}
	const int y0 = y + stripH;		// top of the name block

	// name block
	int tx = x + kPad;
	if (bIcon && pBrush != NULL) {
		// the picture's top-left at (bx, by), centred in the header block's height
		CSprite * pSpr = m_pSprite[SPRID_ITEMPACK_PIVOTPOINT + t.sprite];
		const int bx = x + kPad, by = y0 + (headH - stripH - picH) / 2;
		{
			const stBrush & b = *pBrush;
			const int dx = bx - b.pvx, dy = by - b.pvy;
			const char c = t.itemColor;
			if (c == 0) pSpr->PutSpriteFast(dx, dy, t.frame, m_dwCurTime);
			else if (t.weaponTint) pSpr->PutSpriteRGB(dx, dy, t.frame, m_wWR[c] - m_wR[0], m_wWG[c] - m_wG[0], m_wWB[c] - m_wB[0], m_dwCurTime);
			else pSpr->PutSpriteRGB(dx, dy, t.frame, m_wR[c] - m_wR[0], m_wG[c] - m_wG[0], m_wB[c] - m_wB[0], m_dwCurTime);
		}
		tx = bx + picW + 12;
	}
	const int nameY = y0 + (headH - stripH - textH) / 2 + 7;	// name and type centred beside the picture
	R.TitleOut(tx, nameY, t.name.c_str(), t.color);		// in the display font
	int ux = tx + R.TitleWidth(t.name.c_str());
	if (cUp[0]) { Bold(R, ux, nameY, cUp, kGold); ux += TextW(R, cUp) + 1; }
	if (!countTag.empty()) {
		const int cw = TextW(R, countTag, 12) + 10;
		R.FillPanel(ux + 6, nameY + 1, ux + 6 + cw, nameY + 16, 4, RGB(58, 63, 72), 255);
		R.TextOutSized(ux + 11, nameY + 2, countTag.c_str(), RGB(212, 217, 223), 12);
	}
	if (!t.type.empty()) R.TextOutSized(tx, nameY + 19, t.type.c_str(), RGB(143, 151, 161), 12);

	int cy = y + headH;
	auto line = [&]() { R.FillPanel(x + 1, cy, x + w - 1, cy + 1, 0, RGB(255, 255, 255), 18); };

	if (!t.bonus.empty()) {
		line(); cy += 5;
		for (size_t i = 0; i < t.bonus.size(); i++, cy += kLH) {
			R.TextOut(x + kPad, cy, t.bonus[i].text.c_str(), t.bonus[i].color);
			R.TextOutSized(x + w - kPad - TextW(R, t.bonus[i].source, 12), cy + 2, t.bonus[i].source.c_str(), kDim, 12);
		}
		cy += 5;
	}
	if (!t.gems.empty() || t.emptySockets > 0) {
		line(); cy += 5;
		for (size_t i = 0; i < t.gems.size(); i++, cy += kLH) {
			R.FillPanel(x + kPad, cy + 4, x + kPad + 10, cy + 14, 5, t.gems[i].dot, 255);
			R.TextOut(x + kPad + 16, cy, t.gems[i].text.c_str(), RGB(212, 217, 223));
		}
		for (int i = 0; i < t.emptySockets; i++, cy += kLH) {
			R.FillPanel(x + kPad, cy + 4, x + kPad + 10, cy + 14, 5, RGB(0, 0, 0), 0, kDim, 255);
			R.TextOut(x + kPad + 16, cy, "Empty socket", kDim);
		}
		cy += 5;
	}
	if (!t.info.empty()) {
		line(); cy += 5;
		for (size_t i = 0; i < t.info.size(); i++, cy += kLH)
			R.TextOut(x + kPad, cy, t.info[i].c_str(), kGrey);
		cy += 5;
	}
	if (iLow > 0) {
		line(); cy += 5;
		if (t.durMax > 0) {
			const std::string d = Commas(t.durCur) + " / " + Commas(t.durMax);
			R.TextOut(x + kPad, cy, "Endurance", kGrey);
			R.TextOut(x + w - kPad - TextW(R, d), cy, d.c_str(), bWorn ? kRed : kWhite);
			cy += kLH;
			const int bw = w - kPad * 2;
			const int fw = (int)((long long)bw * (std::min)(t.durCur, t.durMax) / t.durMax);
			const COLORREF bc = bWorn ? kWarn : (t.durCur * 2 < t.durMax ? RGB(230, 190, 70) : kGreen);
			R.FillPanel(x + kPad, cy + 1, x + kPad + bw, cy + 6, 2, RGB(255, 255, 255), 26);
			if (fw >= 2) R.FillPanel(x + kPad, cy + 1, x + kPad + fw, cy + 6, 2, bc, 255);
			cy += 7;
		}
		if (bWorn) { R.TextOut(x + kPad, cy, pWornMsg, RGB(255, 176, 138)); cy += kLH; }
		if (!req.empty()) {
			R.TextOut(x + kPad, cy, "Requires", kGrey);
			int rx = x + w - kPad - TextW(R, req);
			for (size_t i = 0; i < reqParts.size(); i++) {
				if (i) { R.TextOut(rx, cy, ", ", kDim); rx += TextW(R, ", "); }
				R.TextOut(rx, cy, reqParts[i].first.c_str(), reqParts[i].second ? kWhite : kRed);
				rx += TextW(R, reqParts[i].first);
			}
			cy += kLH;
		}
		if (t.total > 0) {
			const std::string s = Commas(t.total) + " in total";
			R.TextOut(x + kPad, cy, "You have", kGrey);
			R.TextOut(x + w - kPad - TextW(R, s), cy, s.c_str(), kWhite);
			cy += kLH;
		}
		cy += 5;
	}
	if (bFoot) {
		R.FillPanel(x + 1, y + h - 24, x + w - 1, y + h - 1, 7, RGB(0, 0, 0), 70);
		R.FillPanel(x + 1, y + h - 24, x + w - 1, y + h - 23, 0, RGB(255, 255, 255), 18);
		if (!weightTxt.empty()) R.TextOutSized(x + kPad, y + h - 20, weightTxt.c_str(), RGB(143, 151, 161), 12);
		if (!valueTxt.empty()) R.TextOutSized(x + w - kPad - TextW(R, valueTxt, 12), y + h - 20, valueTxt.c_str(), RGB(143, 151, 161), 12);
	}
}

// The bag item under the mouse (same test as picking one up), or -1.
int CGame::InventoryItemAt(short msX, short msY)
{
	if (!m_bIsDialogEnabled[DIALOG_INVENTORY]) return -1;
	const short sX = m_dialogBoxes[DIALOG_INVENTORY].m_X, sY = m_dialogBoxes[DIALOG_INVENTORY].m_Y;
	for (int i = 0; i < MAXITEMS; i++) {
		const char id = m_cItemOrder[MAXITEMS - 1 - i];
		if (id == -1 || m_pItemList[id] == NULL || m_bIsItemEquipped[id]) continue;
		CSprite * pSpr = m_pSprite[SPRID_ITEMPACK_PIVOTPOINT + m_pItemList[id]->m_sSprite];
		if (pSpr == NULL) continue;
		const int ix = sX + 32 + m_pItemList[id]->m_sX, iy = sY + 44 + m_pItemList[id]->m_sY;
		pSpr->_GetSpriteRect(ix, iy, m_pItemList[id]->m_sSpriteFrame);
		if (msX <= pSpr->m_rcBound.left || msX >= pSpr->m_rcBound.right || msY <= pSpr->m_rcBound.top || msY >= pSpr->m_rcBound.bottom) continue;
		if (pSpr->_bCheckCollison(ix, iy, m_pItemList[id]->m_sSpriteFrame, msX, msY)) return id;
	}
	return -1;
}
