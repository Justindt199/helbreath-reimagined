// EnchantPanel.cpp: VALORIA enchanting on the client. The rules are in Shared\ValoriaEnchant.h (the
// same file the game server uses) and HG Server\Enchanting.cpp.
//
//   - the Enchanting window (DIALOG_ENCHANT), opened with the Enchanter's Tome (double-click it, or
//     the Enchant button in the spellbook): your gear on the left, the chosen item on the right,
//     and three tabs, Enchant / Disenchant / Empower, each showing what it would do and cost
//   - messages from the server: your Enchanting (NOTIFY_ENCHANTSTATS) and the enchantments of your
//     items (NOTIFY_ENCHANTITEMS)
//   - the tooltip lines for enchanted and empowered items, and the gear totals (ValoriaGear.cpp)
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Valoria;

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(120, 126, 136);
const COLORREF kPurple	= RGB(196, 160, 255);
const COLORREF kOk		= RGB(110, 210, 120);
const COLORREF kBad		= RGB(255, 120, 110);

enum { ENCH_ENCHANT = 1, ENCH_DISENCHANT, ENCH_EMPOWER, ENCH_LESSON };
enum { TAB_ENCHANT, TAB_DISENCHANT, TAB_EMPOWER };
const int kListRows = 13, kRowH = 20;
char kTome[] = "EnchantersTome", kShard[] = "MysticShard", kPrism[] = "PrismaticShard", kGoldName[] = "Gold";

void Bold(CRenderer & r, int x, int y, const char * s, COLORREF c, int iPx)
{
	r.TextOutSized(x, y, s, c, iPx);
	r.TextOutSized(x + 1, y, s, c, iPx);
}

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

bool IsGearItem(const CItem * it) { return it != NULL && it->m_cItemType == ITEMTYPE_EQUIP && IsGearPos(it->m_cEquipPos); }

std::string CostText(const Cost & c)
{
	std::string s = std::to_string(c.gold) + "g, " + std::to_string(c.shards) + " shard" + (c.shards == 1 ? "" : "s");
	if (c.prismatic > 0) s += ", " + std::to_string(c.prismatic) + " prismatic";
	return s;
}

// One clickable thing in the window, built the same way for drawing and clicking.
struct Btn { RECT rc; int action; int idx; int key; bool enabled; std::string label; };

} // namespace

//////////////////////////////////////////////////////////////////////
// Messages from the server

void CGame::NotifyMsg_EnchantStats(char * pData)
{
	const Valoria::CharData old = m_enchant;
	m_enchant.Read((const unsigned char *)(pData + INDEX2_MSGTYPE + 2));
	if (m_bAchSeen)		// VALORIA achievements: announce what the new counters unlocked (UI\AchievePanel.cpp)
		for (int c = 0; c < Valoria::ACH_SAVED; c++)
			if (m_enchant.ach[c] > old.ach[c]) AchAnnounce(c, old.ach[c], m_enchant.ach[c]);
	m_bAchSeen = true;
}

void CGame::NotifyMsg_EnchantItems(char * pData)
{
	for (int i = 0; i < MAXITEMS; i++) if (m_pItemList[i] != NULL) m_pItemList[i]->m_valoria.Clear();
	for (int i = 0; i < MAXBANKITEMS; i++) if (m_pBankList[i] != NULL) m_pBankList[i]->m_valoria.Clear();
	const unsigned char * cp = (const unsigned char *)(pData + INDEX2_MSGTYPE + 2);
	const int n = *cp++;
	for (int e = 0; e < n; e++) {
		const int w = cp[0], idx = cp[1];
		cp += 2;
		CItem * it = NULL;
		if (w == 0 && idx < MAXITEMS) it = m_pItemList[idx];
		else if (w == 1 && idx < MAXBANKITEMS) it = m_pBankList[idx];
		if (it != NULL) it->m_valoria.Read(cp);
		cp += ITEM_BYTES;
	}
}

//////////////////////////////////////////////////////////////////////
// Helpers

void CGame::OpenEnchantWindow()
{
	if (GetItemCount(kTome) == 0) {
		AddEventList("You need an Enchanter's Tome (sold by the Shopkeeper).", 10);
		return;
	}
	if (m_iEnchantSel >= 0 && !IsGearItem(m_pItemList[m_iEnchantSel])) m_iEnchantSel = -1;
	m_dwEnchantConfirmAt = 0;
	EnableDialogBox(DIALOG_ENCHANT, NULL, NULL, NULL);
	PlaySound('E', 14, 5);
}

std::string CGame::ItemDisplayName(const char * pName) const
{
	for (int i = 0; i < MAXITEMNAMES; i++)
		if (m_pItemNameList[i] != NULL && strcmp(m_pItemNameList[i]->m_cOriginName, pName) == 0) return m_pItemNameList[i]->m_cName;
	return pName;
}

// The modifiers an item has (what Empower can raise and Disenchant teaches).
void CGame::EnchantItemMods(const CItem * it, bool has[LEARN_COUNT]) const
{
	int base[MOD_CRIT + 1];
	const bool b = GearRowOf(it->m_cName, base);
	ItemMods(it->m_valoria, b ? base : 0, it->m_cEquipPos, it->m_dwAttribute, has);
}

// Tooltip lines for an item's enchantments and empowering.
void CGame::EnchantTipLines(ItemTip & t, const CItem * it)
{
	if (it == NULL || it->m_valoria.Empty()) return;
	const ItemData & d = it->m_valoria;
	const int tier = ItemTier(it->m_sLevelLimit);
	char cBuf[96];
	if (d.rarity > RARITY_COMMON) {	// rarity bonus stats, in the rarity's colour
		const unsigned int c = RarityRGB(d.rarity);
		for (int i = 0; i < MAX_BONUSES; i++) {
			if (d.bKey[i] >= BONUS_COUNT) continue;
			wsprintf(cBuf, "+%d %s", (int)d.bVal[i], BonusName(d.bKey[i]));
			t.bonus.push_back({ cBuf, RarityName(d.rarity), RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF) });
		}
	}
	for (int i = 0; i < MAX_ENCHANTS; i++) {
		if (d.enKey[i] >= LEARN_COUNT) continue;
		wsprintf(cBuf, "+%d %s", (int)d.enVal[i], ModName(d.enKey[i]));
		t.bonus.push_back({ cBuf, "enchanted", kPurple });
	}
	for (int k = 0; k < MOD_COUNT; k++) {
		if (d.em[k] == 0) continue;
		int v = ItemBonus(d, k, tier) - ((k < ENCHANTABLE) ? d.EnchantValue(k) : 0) - (k <= MOD_CRIT ? d.BonusValue(k) : 0);
		for (int i = 0; i < MAX_RUNES; i++) if (d.rune[i] == k) v -= RuneAmount(k, tier);	// runes: UI\ValoriaUse.cpp
		wsprintf(cBuf, "+%d %s (empowered %d/%d)", v, ModName(k), (int)d.em[k], EMPOWER_MAX);
		t.bonus.push_back({ cBuf, "empowered", RGB(160, 200, 255) });
	}
}

//////////////////////////////////////////////////////////////////////
// The window

namespace {

const int kModRows = 6;		// learned modifiers shown at once on the Enchant tab

// The learned modifiers that can go on this piece of gear (the Enchant tab's rows).
std::vector<int> EnchantChoices(const CGame & g, const CItem * it)
{
	std::vector<int> v;
	for (int k = 0; k < LEARN_COUNT; k++)
		if (ModActive(k) && g.m_enchant.known[k] != 0 && ModFits(k, it->m_cEquipPos)) v.push_back(k);
	return v;
}

// What enchanting modifier k gives this item, as shown on the Enchant tab.
std::string EnchantPreview(const CharData & c, int k, int tier)
{
	char b[64];
	if (IsHbMod(k)) wsprintf(b, "%s %d", ModName(k), HbEnchantValue(c, k, tier));
	else wsprintf(b, "+%d %s", EnchantValueFor(c, k, tier), ModName(k));
	return b;
}

// Everything clickable, for the current state (shared by drawing and clicking).
std::vector<Btn> EnchantButtons(CGame & g, int X, int Y)
{
	std::vector<Btn> out;
	const int W = CGame::ENCHANTWIN_W, H = CGame::ENCHANTWIN_H;
	const CharData & c = g.m_enchant;
	const int lv = EnchantLevel(c.xp);
	Btn b;

	SetRect(&b.rc, X + W - 28, Y + 4, X + W - 6, Y + 26);
	b.action = -1; b.idx = 0; b.key = 0; b.enabled = true; b.label = "x";
	out.push_back(b);																	// close

	if (lv == 0) {
		SetRect(&b.rc, X + 14, Y + 62, X + W - 14, Y + 86);
		b.action = ENCH_LESSON; b.label = "Take the Enchanter's Lesson";
		out.push_back(b);
	}

	const char * const kTabs[3] = { "Enchant", "Disenchant", "Empower" };
	const int tw = (W - 24) / 3;
	for (int i = 0; i < 3; i++) {
		SetRect(&b.rc, X + 12 + i * tw, Y + 94, X + 12 + (i + 1) * tw - 4, Y + 116);
		b.action = -10 - i; b.label = kTabs[i]; b.enabled = true;
		out.push_back(b);
	}

	// the gear list
	std::vector<int> gear;
	for (int i = 0; i < MAXITEMS; i++) if (IsGearItem(g.m_pItemList[i])) gear.push_back(i);
	if (g.m_iEnchantScroll > (int)gear.size() - kListRows) g.m_iEnchantScroll = (std::max)(0, (int)gear.size() - kListRows);
	for (int r = 0; r < kListRows && g.m_iEnchantScroll + r < (int)gear.size(); r++) {
		const int i = gear[g.m_iEnchantScroll + r];
		SetRect(&b.rc, X + 12, Y + 124 + r * kRowH, X + 196, Y + 124 + (r + 1) * kRowH);
		b.action = -20; b.idx = i; b.label = ""; b.enabled = true;
		out.push_back(b);
	}
	if ((int)gear.size() > kListRows) {
		SetRect(&b.rc, X + 12, Y + H - 32, X + 100, Y + H - 12);  b.action = -30; b.label = "Up";   b.enabled = g.m_iEnchantScroll > 0; out.push_back(b);
		SetRect(&b.rc, X + 108, Y + H - 32, X + 196, Y + H - 12); b.action = -31; b.label = "Down"; b.enabled = g.m_iEnchantScroll + kListRows < (int)gear.size(); out.push_back(b);
	}

	// the actions for the chosen item
	const int sel = g.m_iEnchantSel;
	if (sel < 0 || !IsGearItem(g.m_pItemList[sel])) return out;
	const CItem * it = g.m_pItemList[sel];
	const ItemData & d = it->m_valoria;
	const int tier = ItemTier(it->m_sLevelLimit);
	bool has[LEARN_COUNT];
	g.EnchantItemMods(it, has);
	const unsigned long gold = g.GetItemCount(kGoldName), shards = g.GetItemCount(kShard), prism = g.GetItemCount(kPrism);
	auto Afford = [&](const Cost & k) { return gold >= (unsigned long)k.gold && shards >= (unsigned long)k.shards && prism >= (unsigned long)k.prismatic; };
	const int x0 = X + W - 104, x1 = X + W - 14;
	int y = Y + 236;

	switch (g.m_iEnchantTab) {
	case TAB_ENCHANT: {
		const std::vector<int> ks = EnchantChoices(g, it);
		if (g.m_iEnchantModScroll > (int)ks.size() - kModRows) g.m_iEnchantModScroll = (std::max)(0, (int)ks.size() - kModRows);
		for (int r = 0; r < kModRows && g.m_iEnchantModScroll + r < (int)ks.size(); r++) {
			const int k = ks[g.m_iEnchantModScroll + r];
			// a Helbreath attribute replaces the item's own (no slot); the rest take an enchant slot
			const bool bHas = IsHbMod(k) || d.EnchantSlotOf(k) >= 0;
			const Cost cost = EnchantCost(tier, d.Enchants());
			SetRect(&b.rc, x0, y, x1, y + 18);
			b.action = ENCH_ENCHANT; b.idx = sel; b.key = k; b.label = (!IsHbMod(k) && bHas) ? "Re-enchant" : "Enchant";
			b.enabled = lv > 0 && tier <= EnchantTierCap(lv) && (bHas || d.Enchants() < EnchantSlots(lv)) && Afford(cost);
			out.push_back(b);
			y += 22;
		}
		if ((int)ks.size() > kModRows) {
			SetRect(&b.rc, x0 - 96, Y + 236 + kModRows * 22 + 2, x0 - 50, Y + 236 + kModRows * 22 + 20); b.action = -32; b.idx = 0; b.key = 0; b.label = "Up";
			b.enabled = g.m_iEnchantModScroll > 0; out.push_back(b);
			SetRect(&b.rc, x0 - 44, Y + 236 + kModRows * 22 + 2, x0 + 2, Y + 236 + kModRows * 22 + 20); b.action = -33; b.label = "Down";
			b.enabled = g.m_iEnchantModScroll + kModRows < (int)ks.size(); out.push_back(b);
		}
		break; }
	case TAB_DISENCHANT:
		SetRect(&b.rc, x0 - 30, Y + H - 40, x1, Y + H - 16);
		b.action = ENCH_DISENCHANT; b.idx = sel; b.key = 0; b.label = "Disenchant";
		b.enabled = lv > 0 && !g.m_bIsItemEquipped[sel];
		out.push_back(b);
		break;
	case TAB_EMPOWER:
		for (int k = 0; k < MOD_COUNT; k++) {
			if (!has[k]) continue;
			SetRect(&b.rc, x0, y, x1, y + 18);
			b.action = ENCH_EMPOWER; b.idx = sel; b.key = k; b.label = d.em[k] >= EMPOWER_MAX ? "Maxed" : "Empower";
			b.enabled = d.em[k] < EMPOWER_MAX && Afford(EmpowerCost(tier, d.em[k] + 1));
			out.push_back(b);
			y += 22;
		}
		break;
	}
	return out;
}

} // namespace

void CGame::DrawDialogBox_Enchant()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_ENCHANT];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = ENCHANTWIN_W, H = ENCHANTWIN_H;
	const bool bTop = dlg.IsTop();
	const CharData & c = m_enchant;
	const int lv = EnchantLevel(c.xp);
	char cTxt[160];

	R.StonePanel(X, Y, X + W, Y + H);
	R.TitleOut(X + 14, Y + 8, "ENCHANTING", kGold, 15);
	R.FillPanel(X + 1, Y + 32, X + W - 1, Y + 33, 0, RGB(255, 255, 255), 26);

	// your Enchanting
	if (lv > 0) {
		if (lv < ENCHANT_MAX) wsprintf(cTxt, "Enchanting level %d  -  %u / %u XP to level %d", lv, c.xp, EnchantXPTotal(lv + 1), lv + 1);
		else wsprintf(cTxt, "Enchanting level %d (highest)", lv);
		Bold(R, X + 16, Y + 42, cTxt, kWhite, 12);
		wsprintf(cTxt, "Up to %d enchantment%s per item, gear up to tier %d. Disenchanting gear is how you learn and level.", EnchantSlots(lv), EnchantSlots(lv) == 1 ? "" : "s", EnchantTierCap(lv));
		R.TextOutSized(X + 16, Y + 62, cTxt, kGrey, 10);
		int nKnown = 0;
		for (int k = 0; k < LEARN_COUNT; k++) if (ModActive(k) && c.known[k] != 0) nKnown++;
		wsprintf(cTxt, "Learned %d of %d modifiers (tier T and level L of each on the Enchant tab)", nKnown, (int)LEARN_COUNT);
		R.TextOutSized(X + 16, Y + 76, nKnown == 0 ? "Learned: nothing yet" : cTxt, nKnown == 0 ? kDim : kPurple, 10);
	}
	else {
		R.TextOutSized(X + 16, Y + 42, "You haven't learned Enchanting. The lesson teaches level 1 and gives a blade to disenchant.", kGrey, 10);
	}

	std::vector<Btn> btns = EnchantButtons(*this, X, Y);
	for (size_t i = 0; i < btns.size(); i++) {
		const Btn & b = btns[i];
		const bool bOn = bTop && b.enabled && InRect(b.rc, ms::X, ms::Y);
		if (b.action == -1) {
			if (bOn) R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 5, RGB(255, 255, 255), 34);
			R.TextOutSized(b.rc.left + 7, b.rc.top, "x", bOn ? kWhite : kGrey, 17);
		}
		else if (b.action <= -10 && b.action >= -12) {			// tabs
			const bool bSel = (m_iEnchantTab == -10 - b.action);
			R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 6, bSel ? kGold : RGB(255, 255, 255), bSel ? 220 : (bOn ? 40 : 18));
			const int tw = R.TitleWidth(b.label.c_str(), 12);
			R.TitleOut((b.rc.left + b.rc.right - tw) / 2, b.rc.top + 4, b.label.c_str(), bSel ? RGB(34, 26, 10) : kWhite, 12);
		}
		else if (b.action == -20) {								// a gear row
			const CItem * it = m_pItemList[b.idx];
			const bool bSel = (b.idx == m_iEnchantSel);
			if (bSel || bOn) R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 4, bSel ? kPurple : RGB(255, 255, 255), bSel ? 60 : 24);
			std::string name = ItemDisplayName(it->m_cName);
			if (name.size() > 22) name = name.substr(0, 21) + ".";
			R.TextOutSized(b.rc.left + 6, b.rc.top + 3, name.c_str(), it->m_valoria.Empty() ? RGB(214, 218, 226) : kPurple, 11);
			wsprintf(cTxt, "%sT%d", m_bIsItemEquipped[b.idx] ? "E " : "", ItemTier(it->m_sLevelLimit));
			R.TextOutSized(b.rc.right - 6 - R.TextWidthSized(cTxt, 10), b.rc.top + 4, cTxt, kDim, 10);
		}
		else {													// buttons
			R.StoneButton(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, b.enabled ? 1 : 0);
			std::string label = b.label;
			if (b.action == ENCH_DISENCHANT && m_dwEnchantConfirmAt != 0 && m_dwCurTime - m_dwEnchantConfirmAt < 4000) label = "Click again to confirm";
			const int px = (b.rc.bottom - b.rc.top) > 20 ? 13 : 11;
			const int tw = R.TitleWidth(label.c_str(), px);
			R.TitleOut((b.rc.left + b.rc.right - tw) / 2, b.rc.top + ((b.rc.bottom - b.rc.top) > 20 ? 5 : 3), label.c_str(), b.enabled ? (bOn ? RGB(255, 242, 199) : RGB(245, 219, 158)) : RGB(150, 144, 130), px);
		}
	}

	// the gear list frame
	R.FillPanel(X + 204, Y + 124, X + 205, Y + H - 12, 0, RGB(255, 255, 255), 20);
	bool bAnyGear = false;
	for (int i = 0; i < MAXITEMS; i++) if (IsGearItem(m_pItemList[i])) bAnyGear = true;
	if (!bAnyGear) R.TextOutSized(X + 16, Y + 128, "No gear in your bag.", kDim, 11);

	// the chosen item
	const int rx = X + 214;
	if (m_iEnchantSel < 0 || !IsGearItem(m_pItemList[m_iEnchantSel])) {
		R.TextOutSized(rx, Y + 128, "Choose a piece of gear on the left.", kGrey, 11);
	}
	else {
		const CItem * it = m_pItemList[m_iEnchantSel];
		const ItemData & d = it->m_valoria;
		const int tier = ItemTier(it->m_sLevelLimit);
		bool has[LEARN_COUNT];
		EnchantItemMods(it, has);
		int y = Y + 126;
		Bold(R, rx, y, ItemDisplayName(it->m_cName).c_str(), kWhite, 13);
		y += 18;
		wsprintf(cTxt, "Tier %d%s  -  %d/%d enchantment%s", tier, m_bIsItemEquipped[m_iEnchantSel] ? ", equipped" : "", d.Enchants(), EnchantSlots(lv), EnchantSlots(lv) == 1 ? "" : "s");
		R.TextOutSized(rx, y, cTxt, kGrey, 10);
		y += 16;
		std::string mods;
		for (int i = 0; i < MAX_ENCHANTS; i++) if (d.enKey[i] < LEARN_COUNT) {
			wsprintf(cTxt, "%s+%d %s", mods.empty() ? "" : ", ", (int)d.enVal[i], ModName(d.enKey[i]));
			mods += cTxt;
		}
		R.TextOutSized(rx, y, mods.empty() ? "No enchantments" : mods.c_str(), mods.empty() ? kDim : kPurple, 10);
		y += 14;
		std::string em;
		for (int k = 0; k < MOD_COUNT; k++) if (d.em[k] > 0) {
			wsprintf(cTxt, "%s%s %d/%d", em.empty() ? "" : ", ", ModName(k), (int)d.em[k], EMPOWER_MAX);
			em += cTxt;
		}
		R.TextOutSized(rx, y, em.empty() ? "Not empowered" : ("Empowered: " + em).c_str(), em.empty() ? kDim : RGB(160, 200, 255), 10);
		y += 18;
		R.FillPanel(rx, y, X + W - 12, y + 1, 0, RGB(255, 255, 255), 20);

		// the tab's rows: what each button does (the buttons are drawn above)
		y = Y + 236;
		switch (m_iEnchantTab) {
		case TAB_ENCHANT: {
			bool bAny = false;
			if (lv > 0 && tier > EnchantTierCap(lv)) {
				wsprintf(cTxt, "Tier %d gear needs Enchanting level %d.", tier, (tier - 3) * 8);
				R.TextOutSized(rx, Y + 222, cTxt, kBad, 10);
			}
			else if (lv > 0 && d.Enchants() >= EnchantSlots(lv)) R.TextOutSized(rx, Y + 222, "No free slot: re-enchant a modifier it has.", kGrey, 10);
			const std::vector<int> ks = EnchantChoices(*this, it);
			for (int r = 0; r < kModRows && m_iEnchantModScroll + r < (int)ks.size(); r++) {
				const int k = ks[m_iEnchantModScroll + r];
				bAny = true;
				const Cost cost = EnchantCost(tier, d.Enchants());
				R.TextOutSized(rx, y - 1, EnchantPreview(c, k, tier).c_str(), IsHbMod(k) ? RGB(160, 200, 255) : kWhite, 11);
				wsprintf(cTxt, "T%d L%d   %s", (int)c.known[k], ModLevel(c, k), CostText(cost).c_str());
				R.TextOutSized(rx, y + 10, cTxt, kDim, 9);
				y += 22;
			}
			if (!bAny) R.TextOutSized(rx, y, lv > 0 ? "Disenchant gear to learn modifiers that fit it." : "Take the lesson first.", kGrey, 11);
			break; }
		case TAB_DISENCHANT: {
			int iNew = 0;
			std::string teach;
			for (int k = 0; k < LEARN_COUNT; k++) {
				if (!has[k]) continue;
				const bool bNew = tier > c.known[k];
				if (bNew) iNew++;
				teach += std::string(teach.empty() ? "" : ", ") + ModName(k) + (bNew ? " (new)" : "");
			}
			const int sh = DisenchantShards(tier, d.EmpowerTotal(), d.Runes());
			wsprintf(cTxt, "Unravels into %d Mystic Shard%s", sh, sh == 1 ? "" : "s");
			R.TextOutSized(rx, y, cTxt, kWhite, 11); y += 18;
			wsprintf(cTxt, "+%u Enchanting XP", SkillXP(DisenchantXP(tier, iNew)));
			R.TextOutSized(rx, y, cTxt, kWhite, 11); y += 18;
			{	const std::string s = teach.empty() ? "Teaches nothing (no modifiers)." : "Teaches: " + teach;
				RECT rc; SetRect(&rc, rx, y, X + W - 12, y + 48);
				R.DrawTextEx(&rc, s.c_str(), teach.empty() ? kDim : kPurple, DT_LEFT, true);
				y += 50; }
			R.TextOutSized(rx, y, "The item is destroyed.", kBad, 10); y += 16;
			if (m_bIsItemEquipped[m_iEnchantSel]) R.TextOutSized(rx, y, "Unequip it first.", kBad, 10);
			break; }
		case TAB_EMPOWER:
			for (int k = 0; k < MOD_COUNT; k++) {
				if (!has[k]) continue;
				if (d.em[k] < EMPOWER_MAX) {
					wsprintf(cTxt, "%s %d/%d  +%d", ModName(k), (int)d.em[k], EMPOWER_MAX, IsCore(k) ? 1 : EmpowerStep(k, tier));
					R.TextOutSized(rx, y + 1, cTxt, kWhite, 11);
					R.TextOutSized(rx + 120, y + 2, CostText(EmpowerCost(tier, d.em[k] + 1)).c_str(), kDim, 9);
				}
				else {
					wsprintf(cTxt, "%s %d/%d", ModName(k), (int)d.em[k], EMPOWER_MAX);
					R.TextOutSized(rx, y + 1, cTxt, kOk, 11);
				}
				y += 22;
			}
			break;
		}
	}

	// what you carry
	wsprintf(cTxt, "Mystic Shards %lu    Prismatic %lu    Gold %lu", (unsigned long)GetItemCount(kShard), (unsigned long)GetItemCount(kPrism), (unsigned long)GetItemCount(kGoldName));
	R.TextOutSized(X + 214, Y + H - 14 - 12, cTxt, kGold, 10);
}

void CGame::DlgBoxClick_Enchant()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_ENCHANT];
	std::vector<Btn> btns = EnchantButtons(*this, dlg.m_X, dlg.m_Y);
	for (size_t i = 0; i < btns.size(); i++) {
		const Btn & b = btns[i];
		if (!InRect(b.rc, ms::X, ms::Y)) continue;
		if (!b.enabled) return;
		PlaySound('E', 14, 5);
		switch (b.action) {
		case -1: DisableDialogBox(DIALOG_ENCHANT); return;
		case -10: case -11: case -12: m_iEnchantTab = -10 - b.action; m_dwEnchantConfirmAt = 0; return;
		case -20: m_iEnchantSel = b.idx; m_iEnchantModScroll = 0; m_dwEnchantConfirmAt = 0; return;
		case -30: if (m_iEnchantScroll > 0) m_iEnchantScroll--; return;
		case -31: m_iEnchantScroll++; return;
		case -32: if (m_iEnchantModScroll > 0) m_iEnchantModScroll--; return;
		case -33: m_iEnchantModScroll++; return;
		case ENCH_DISENCHANT:
			if (m_dwEnchantConfirmAt == 0 || m_dwCurTime - m_dwEnchantConfirmAt >= 4000) { m_dwEnchantConfirmAt = m_dwCurTime; return; }
			m_dwEnchantConfirmAt = 0;
			bSendCommand(MSGID_REQUEST_ENCHANT, ENCH_DISENCHANT, NULL, b.idx, 0, 0, NULL);
			m_iEnchantSel = -1;
			return;
		default:
			bSendCommand(MSGID_REQUEST_ENCHANT, b.action, NULL, b.idx, b.key, 0, NULL);
			return;
		}
	}
}
