// ValoriaUse.cpp: VALORIA's usable items on the client (rules: Shared\ValoriaUse.h, server: HG Server\ValoriaUse.cpp).
//   - your elixir / scroll buffs (NOTIFY_VALORIABUFFS), shown under the clock with the time left, and
//     added to the stat window (ValoriaBuffTotal, used in ValoriaGear.cpp)
//   - tooltip lines for elixirs, scrolls, runes and the Blessed Upgrade Stone, and for runes on gear
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"

using namespace ValoriaUse;

void CGame::NotifyMsg_ValoriaBuffs(char * pData)
{
	char * cp = pData + INDEX2_MSGTYPE + 2;
	for (int b = 0; b < BUFF_COUNT; b++) m_dwValBuffEnd[b] = 0;
	uint8 n;
	Pop(cp, n);
	for (int i = 0; i < n; i++) {
		uint8 b; uint16 s;
		Pop(cp, b);
		Pop(cp, s);
		if (b < BUFF_COUNT) m_dwValBuffEnd[b] = m_dwCurTime + (DWORD)s * 1000 + 1;
	}
}

// What the running buffs add to one of the gear kinds (GEAR_STR ...).
int CGame::ValoriaBuffTotal(int iKind) const
{
	int v = 0;
	for (int b = 0; b < BUFF_COUNT; b++) {
		if (m_dwValBuffEnd[b] == 0) continue;
		const BuffDef & d = BUFFS[b];
		switch (iKind) {
		case GEAR_STR: v += d.str; break;
		case GEAR_DEX: v += d.dex; break;
		case GEAR_INT: v += d.intl; break;
		case GEAR_MAG: v += d.mag; break;
		case GEAR_DEF: v += d.def; break;
		case GEAR_VIT: v += d.vit; break;
		case GEAR_HP:  v += d.hp; break;
		case GEAR_MP:  v += d.mp; break;
		}
	}
	return v;
}

// One line per buff under the clock: "Elixir of Strength 2:41".
void CGame::DrawValoriaBuffs()
{
	RECT rc;
	ClockPingRect(&rc);
	if (rc.right <= rc.left) return;
	CRenderer & R = m_Renderer;
	int y = rc.bottom + 4;
	char cTxt[64];
	for (int b = 0; b < BUFF_COUNT; b++) {
		if (m_dwValBuffEnd[b] == 0) continue;
		const int left = (m_dwValBuffEnd[b] > m_dwCurTime) ? (int)((m_dwValBuffEnd[b] - m_dwCurTime) / 1000) : 0;
		wsprintf(cTxt, "%s  %d:%02d", BUFFS[b].name, left / 60, left % 60);
		const int w = R.TextWidthSized(cTxt, 10) + 14;
		R.FillPanel(rc.left, y, rc.left + w, y + 16, 5, RGB(0, 0, 0), 120, RGB(232, 188, 92), 90);
		R.TextOutSized(rc.left + 7, y + 1, cTxt, left <= 15 ? RGB(255, 150, 120) : RGB(236, 226, 200), 10);
		y += 19;
	}
}

// Tooltip lines: what an elixir / scroll / rune / stone does, and the runes on a piece of gear.
void CGame::ValoriaUseTipLines(ItemTip & t, const CItem * it)
{
	if (it == NULL) return;
	char cBuf[96];
	const COLORREF kUse = RGB(200, 230, 255), kHow = RGB(150, 156, 168);
	int v = 0;
	const int eff = UseKindOf(it->m_cName, &v);
	if (eff == ITEMEFFECTTYPE_VALORIA_BUFF) {
		char lines[4][48];
		const int b = v, n = BuffLines(b, lines);
		for (int i = 0; i < n; i++) t.bonus.push_back({ lines[i], "", kUse });
		if (b >= 0 && b < BUFF_COUNT) {
			wsprintf(cBuf, "For %d:%02d. Double-click to use.", BUFFS[b].seconds / 60, BUFFS[b].seconds % 60);
			t.bonus.push_back({ cBuf, "", kHow });
		}
		return;
	}
	if (eff == ITEMEFFECTTYPE_VALORIA_INFUSE) {
		if (v == INFUSE_BLESSED) t.bonus.push_back({ "+1 upgrade level, guaranteed (up to +15)", "", kUse });
		else if (v >= 1 && v <= Valoria::MOD_COUNT) {
			wsprintf(cBuf, "Infuses %s into gear: +%d to +%d by the gear's tier", Valoria::ModName(v - 1),
				Valoria::RuneAmount(v - 1, 1), Valoria::RuneAmount(v - 1, 6));
			t.bonus.push_back({ cBuf, "", kUse });
			t.bonus.push_back({ "2 runes per item at most", "", kHow });
		}
		t.bonus.push_back({ "Double-click, then click gear in your bag.", "", kHow });
		return;
	}
	const Valoria::ItemData & d = it->m_valoria;
	if (d.Runes() == 0) return;
	const int tier = Valoria::ItemTier(it->m_sLevelLimit);
	for (int i = 0; i < Valoria::MAX_RUNES; i++) {
		if (d.rune[i] >= Valoria::MOD_COUNT) continue;
		wsprintf(cBuf, "+%d %s", Valoria::RuneAmount(d.rune[i], tier), Valoria::ModName(d.rune[i]));
		t.bonus.push_back({ cBuf, "rune", RGB(255, 196, 120) });
	}
	wsprintf(cBuf, "Runes %d/%d", d.Runes(), Valoria::MAX_RUNES);
	t.bonus.push_back({ cBuf, "", kHow });
}
