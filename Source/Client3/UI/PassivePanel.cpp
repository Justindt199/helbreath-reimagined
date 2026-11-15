// PassivePanel.cpp: VALORIA's passive skill tree on the client. The rules and the tree are in
// Shared\ValoriaPassives.h and Shared\ValoriaPassiveTree.h (the same files the game server uses) and
// HG Server\Passives.cpp.
//
//   - the Passive Skills window (DIALOG_PASSIVE), opened with the Passives button in the spellbook or
//     by double-clicking an Orb of Unmaking or a Tome of the Unbound Path: VALORIA's 301 nodes, the
//     links between them, your points, and the Orb and Tome buttons
//   - click a node next to one you have to pick it, click it again to learn it (points only come back
//     with an Orb of Unmaking, so a single stray click never spends one)
//   - mouse wheel zooms, the arrows and a click on empty space move the view
//   - the message from the server: your tree (NOTIFY_PASSIVES)
//   - the tree's totals for the stat window and the HP / MP bars (PassiveTotal, used in ValoriaGear.cpp)
//////////////////////////////////////////////////////////////////////

#include "..\Game.h"
#include "..\..\shared\buffer.h"
#include "..\Engine\UiGpu.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace ValoriaPassive;

namespace {

const COLORREF kGold	= RGB(232, 188, 92);
const COLORREF kWhite	= RGB(236, 238, 242);
const COLORREF kGrey	= RGB(150, 156, 168);
const COLORREF kDim		= RGB(110, 116, 126);
const COLORREF kOk		= RGB(110, 210, 120);
const COLORREF kBad		= RGB(255, 120, 110);
const COLORREF kBranch[4] = { RGB(232, 188, 92), RGB(226, 96, 80), RGB(96, 200, 120), RGB(110, 150, 255) };	// none, Might, Finesse, Arcana

enum { PASS_TAKE = 1, PASS_UNMAKE, PASS_TOME };
enum { A_CLOSE = -1, A_ZOOMOUT = -2, A_ZOOMIN = -3, A_LEFT = -4, A_UP = -5, A_DOWN = -6, A_RIGHT = -7,
	A_CENTRE = -8, A_TOTALS = -9, A_ORB = -10, A_TOME = -11 };
const float kZoom[] = { 0.16f, 0.22f, 0.30f, 0.42f, 0.58f, 0.80f };
const int kZooms = sizeof(kZoom) / sizeof(kZoom[0]);
const int kTop = 58, kBottom = 42;				// the tree view: below the header, above the buttons
char kOrbName[] = "OrbOfUnmaking", kTomeName[] = "TomeOfTheUnboundPath";

bool InRect(const RECT & rc, int x, int y) { return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom; }

struct Btn { RECT rc; int action; bool enabled; std::string label; };

struct View {
	RECT rc;		// the tree view, game pixels
	float z;		// game pixels per VALORIA tree unit
	float cx, cy;	// game pixel of the tree's (pan x, pan y)
	int px, py;
	int SX(int x) const { return (int)lroundf(cx + (x - px) * z); }
	int SY(int y) const { return (int)lroundf(cy + (y - py) * z); }
};

View MakeView(const CGame & g, int X, int Y)
{
	View v;
	SetRect(&v.rc, X + 12, Y + kTop, X + CGame::PASSIVEWIN_W - 12, Y + CGame::PASSIVEWIN_H - kBottom);
	v.z = kZoom[(std::max)(0, (std::min)(kZooms - 1, g.m_iPassiveZoom))];
	v.cx = (v.rc.left + v.rc.right) / 2.0f;
	v.cy = (v.rc.top + v.rc.bottom) / 2.0f;
	v.px = g.m_iPassivePanX;
	v.py = g.m_iPassivePanY;
	return v;
}

int Radius(int i, float z)
{
	static const float base[4] = { 9.f, 4.f, 6.f, 8.f };	// root, small, notable, keystone
	const float s = (std::max)(0.8f, (std::min)(1.9f, z / 0.30f));
	return (std::max)(3, (int)lroundf(base[NODES[i].kind] * s));
}

// The node under the mouse, or -1.
int NodeAt(const View & v, int mx, int my)
{
	if (!InRect(v.rc, mx, my)) return -1;
	int best = -1, bestD = 1 << 30;
	for (int i = 0; i < NODE_COUNT; i++) {
		const int dx = v.SX(NODES[i].x) - mx, dy = v.SY(NODES[i].y) - my;
		const int r = Radius(i, v.z) + 3, d = dx * dx + dy * dy;
		if (d <= r * r && d < bestD) { best = i; bestD = d; }
	}
	return best;
}

std::vector<Btn> PassiveButtons(CGame & g, int X, int Y)
{
	std::vector<Btn> out;
	const int W = CGame::PASSIVEWIN_W, H = CGame::PASSIVEWIN_H;
	const int by0 = Y + H - 32, by1 = Y + H - 10;
	Btn b;
	b.enabled = true;
	SetRect(&b.rc, X + W - 28, Y + 4, X + W - 6, Y + 26); b.action = A_CLOSE; b.label = "x"; out.push_back(b);

	int x = X + 12;
	auto Add = [&](int w, int a, const char * s, bool on) {
		SetRect(&b.rc, x, by0, x + w, by1); b.action = a; b.label = s; b.enabled = on; out.push_back(b); x += w + 4;
	};
	Add(24, A_ZOOMOUT, "-", g.m_iPassiveZoom > 0);
	Add(24, A_ZOOMIN, "+", g.m_iPassiveZoom < kZooms - 1);
	x += 6;
	Add(24, A_LEFT, "<", true);
	Add(24, A_UP, "^", true);
	Add(24, A_DOWN, "v", true);
	Add(24, A_RIGHT, ">", true);
	Add(56, A_CENTRE, "Centre", true);
	x += 6;
	Add(58, A_TOTALS, g.m_bPassiveTotals ? "Tree" : "Totals", true);

	char cTxt[64];
	const unsigned long orbs = g.GetItemCount(kOrbName), tomes = g.GetItemCount(kTomeName);
	x = X + W - 12 - 150 - 4 - 128;
	wsprintf(cTxt, "Orb of Unmaking (%lu)", orbs);
	Add(150, A_ORB, cTxt, orbs > 0 && g.m_passive.Taken() > 0);
	wsprintf(cTxt, "Read Tome (%lu)", tomes);
	Add(128, A_TOME, cTxt, tomes > 0 && g.m_passive.tomes < MAX_TOMES);
	return out;
}

void Mods(const Node & n, std::vector<std::pair<std::string, COLORREF>> & out)
{
	char c[64];
	for (int m = 0; m < n.mods; m++) {
		ModText(n.mod[m], n.val[m], c, sizeof(c));
		std::string s = c;
		if (!ModActive(n.mod[m])) s += "  (coming later)";
		out.push_back({ s, ModActive(n.mod[m]) ? (n.val[m] < 0 ? kBad : RGB(200, 230, 255)) : kDim });
	}
}

} // namespace

//////////////////////////////////////////////////////////////////////
// From the server

void CGame::NotifyMsg_Passives(char * pData)
{
	m_passive.Read((const unsigned char *)(pData + INDEX2_MSGTYPE + 2));
	if (m_iPassivePick >= 0 && !m_passive.CanTake(m_iPassivePick)) m_iPassivePick = -1;
}

// The tree's total for one of the gear kinds (GEAR_STR .. GEAR_DEF follow PM_STR .. PM_DEF), or a
// PMod for the rest; 0 while the tree gives nothing (more points spent than the level has).
int CGame::PassiveTotal(int iMod) const
{
	if (iMod < 0 || iMod >= PM_COUNT || !m_passive.Active(m_iLevel)) return 0;
	int t[PM_COUNT];
	m_passive.Totals(t);
	return t[iMod];
}

void CGame::OpenPassiveWindow()
{
	m_iPassivePick = -1;
	m_dwPassiveOrbAt = 0;
	EnableDialogBox(DIALOG_PASSIVE, NULL, NULL, NULL);
	PlaySound('E', 14, 5);
}

//////////////////////////////////////////////////////////////////////
// The window

void CGame::DrawDialogBox_Passives()
{
	CRenderer & R = m_Renderer;
	CDialogBox & dlg = m_dialogBoxes[DIALOG_PASSIVE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	const int W = PASSIVEWIN_W, H = PASSIVEWIN_H;
	const bool bTop = dlg.IsTop();
	const Data & d = m_passive;
	const int spare = d.Spare(m_iLevel), points = Points(m_iLevel, d.tomes);
	char cTxt[200];

	R.StonePanel(X, Y, X + W, Y + H);
	R.TitleOut(X + 14, Y + 8, "PASSIVE SKILLS", kGold, 15);
	R.FillPanel(X + 1, Y + 32, X + W - 1, Y + 33, 0, RGB(255, 255, 255), 26);

	if (spare >= 0) wsprintf(cTxt, "%d point%s to spend  -  %d of %d spent  (1 a level after 1, +%d from Tomes)", spare, spare == 1 ? "" : "s", d.Taken(), points, (int)d.tomes);
	else wsprintf(cTxt, "%d points spent but only %d to spend: the tree gives nothing until level %d.", d.Taken(), points, d.Taken() - d.tomes + 1);
	R.TextOutSized(X + 16, Y + 39, cTxt, spare > 0 ? kOk : (spare == 0 ? kGrey : kBad), 11);

	// the wheel zooms while the mouse is over the view
	View v = MakeView(*this, X, Y);
	if (bTop && ms::Z != 0 && ms::LB == 0 && InRect(v.rc, ms::X, ms::Y)) {
		const int z0 = m_iPassiveZoom;
		m_iPassiveZoom = (std::max)(0, (std::min)(kZooms - 1, m_iPassiveZoom + (ms::Z > 0 ? 1 : -1)));
		if (m_iPassiveZoom != z0) {				// keep the tree point under the mouse where it is
			const float fx = v.px + (ms::X - v.cx) / v.z, fy = v.py + (ms::Y - v.cy) / v.z;
			const float z1 = kZoom[m_iPassiveZoom];
			m_iPassivePanX = (int)lroundf(fx - (ms::X - v.cx) / z1);
			m_iPassivePanY = (int)lroundf(fy - (ms::Y - v.cy) / z1);
		}
		m_Mouse.m_sZ = 0;
		ms::Z = 0;
		v = MakeView(*this, X, Y);
	}

	R.FillPanel(v.rc.left, v.rc.top, v.rc.right, v.rc.bottom, 6, RGB(8, 9, 12), 235, RGB(255, 255, 255), 24);

	// the links: one picture, made again only when the tree or the view changes
	if (CUiGpu * pG = R.UiGpuRecorder()) {
		const int S = 2, w = (v.rc.right - v.rc.left) * S, h = (v.rc.bottom - v.rc.top) * S;
		std::uint32_t ver = 2166136261u;
		auto Mix = [&](std::uint32_t x) { ver = (ver ^ x) * 16777619u; };
		for (int i = 0; i < BIT_BYTES; i++) Mix(d.bits[i]);
		Mix((std::uint32_t)m_iPassiveZoom); Mix((std::uint32_t)m_iPassivePanX); Mix((std::uint32_t)m_iPassivePanY);
		const View vv = v;
		pG->AddImageCached(0x5041535349564500ull, ver | 1u, w, h,
			[&](std::uint32_t * px) {
				std::fill(px, px + (size_t)w * h, 0u);
				auto Plot = [&](int x, int y, std::uint32_t c) {
					for (int oy = 0; oy < 2; oy++) for (int ox = 0; ox < 2; ox++) {
						const int xx = x + ox, yy = y + oy;
						if (xx >= 0 && yy >= 0 && xx < w && yy < h) px[(size_t)yy * w + xx] = c;
					}
				};
				for (int i = 0; i < NODE_COUNT; i++)
					for (int l = 0; l < NODES[i].links; l++) {
						const int j = NODES[i].link[l];
						if (j < i) continue;
						const bool a = d.Has(i), b = d.Has(j);
						std::uint32_t c;
						if (a && b) c = 232u | (188u << 8) | (92u << 16) | (235u << 24);			// both learned
						else if (a || b) c = 170u | (176u << 8) | (190u << 16) | (150u << 24);	// one learned: the way on
						else c = 90u | (96u << 8) | (110u << 16) | (90u << 24);
						const float x0 = (vv.SX(NODES[i].x) - vv.rc.left) * (float)S, y0 = (vv.SY(NODES[i].y) - vv.rc.top) * (float)S;
						const float x1 = (vv.SX(NODES[j].x) - vv.rc.left) * (float)S, y1 = (vv.SY(NODES[j].y) - vv.rc.top) * (float)S;
						if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) || (x0 >= w && x1 >= w) || (y0 >= h && y1 >= h)) continue;
						const int n = (int)(std::max)(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
						for (int s = 0; s <= n; s++) {
							const float t = (float)s / n;
							Plot((int)(x0 + (x1 - x0) * t) - (a && b ? 1 : 0), (int)(y0 + (y1 - y0) * t), c);
							if (a && b) Plot((int)(x0 + (x1 - x0) * t), (int)(y0 + (y1 - y0) * t) - 1, c);
						}
					}
			}, true, (float)(v.rc.left * S), (float)(v.rc.top * S), (float)w, (float)h,
			(std::max)(0, (int)v.rc.left * S), (std::max)(0, (int)v.rc.top * S), (std::min)(GAME_W * S, (int)v.rc.right * S), (std::min)(GAME_H * S, (int)v.rc.bottom * S), UIBL_ALPHA);
	}

	// the nodes
	const int hover = bTop ? NodeAt(v, ms::X, ms::Y) : -1;
	for (int i = 0; i < NODE_COUNT; i++) {
		const int sx = v.SX(NODES[i].x), sy = v.SY(NODES[i].y), r = Radius(i, v.z);
		if (sx + r < v.rc.left || sx - r >= v.rc.right || sy + r < v.rc.top || sy - r >= v.rc.bottom) continue;
		const COLORREF bc = kBranch[NODES[i].branch];
		const bool bHas = d.Has(i), bCan = d.CanTake(i);
		if (bHas)
			R.FillPanel(sx - r, sy - r, sx + r + 1, sy + r + 1, r, bc, 240, kWhite, i == hover ? 255 : 150);
		else if (bCan)
			R.FillPanel(sx - r, sy - r, sx + r + 1, sy + r + 1, r, bc, i == hover ? 120 : 55, bc, 255);
		else
			R.FillPanel(sx - r, sy - r, sx + r + 1, sy + r + 1, r, RGB(40, 44, 52), 230, bc, i == hover ? 200 : 90);
		if (i == m_iPassivePick)
			R.FillPanel(sx - r - 3, sy - r - 3, sx + r + 4, sy + r + 4, r + 3, kWhite, 0, kWhite, (m_dwCurTime / 250) % 2 ? 255 : 120);
	}

	// the buttons
	std::vector<Btn> btns = PassiveButtons(*this, X, Y);
	for (size_t i = 0; i < btns.size(); i++) {
		const Btn & b = btns[i];
		const bool bOn = bTop && b.enabled && InRect(b.rc, ms::X, ms::Y);
		if (b.action == A_CLOSE) {
			if (bOn) R.FillPanel(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 5, RGB(255, 255, 255), 34);
			R.TextOutSized(b.rc.left + 7, b.rc.top, "x", bOn ? kWhite : kGrey, 17);
			continue;
		}
		const bool bGold = (b.action == A_ORB || b.action == A_TOME);
		std::string label = b.label;
		if (b.action == A_ORB && m_dwPassiveOrbAt != 0 && m_dwCurTime - m_dwPassiveOrbAt < 4000) label = "Click again: unmake all";
		R.StoneButton(b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, b.enabled && (bGold || bOn) ? 1 : 0);
		const int tw = R.TitleWidth(label.c_str(), 11);
		R.TitleOut((b.rc.left + b.rc.right - tw) / 2, b.rc.top + 4, label.c_str(),
			b.enabled ? (bOn ? RGB(255, 242, 199) : RGB(245, 219, 158)) : kDim, 11);
	}

	// the totals, over the tree
	if (m_bPassiveTotals) {
		int t[PM_COUNT];
		d.Totals(t);
		const int x0 = v.rc.right - 230, y0 = v.rc.top + 8;
		int n = 0;
		for (int k = 0; k < PM_COUNT; k++) if (t[k] != 0) n++;
		R.FillPanel(x0, y0, v.rc.right - 8, y0 + 30 + (std::max)(1, n) * 15, 6, RGB(16, 18, 24), 240, kGold, 120);
		R.TitleOut(x0 + 10, y0 + 6, d.Active(m_iLevel) ? "YOUR TREE GIVES" : "YOUR TREE (NOT WORKING)", d.Active(m_iLevel) ? kGold : kBad, 11);
		int y = y0 + 26;
		if (n == 0) R.TextOutSized(x0 + 10, y, "Nothing learned yet.", kDim, 10);
		for (int k = 0; k < PM_COUNT; k++) {
			if (t[k] == 0) continue;
			ModText(k, t[k], cTxt, sizeof(cTxt));
			if (!ModActive(k)) strcat(cTxt, "  (coming later)");
			R.TextOutSized(x0 + 10, y, cTxt, ModActive(k) ? (t[k] < 0 ? kBad : RGB(200, 230, 255)) : kDim, 10);
			y += 15;
		}
	}

	// the node under the mouse
	if (hover >= 0) {
		const Node & n = NODES[hover];
		static const char * const kKind[4] = { "Start", "Passive", "Notable", "Keystone" };
		static const char * const kBr[4] = { "", "Might", "Finesse", "Arcana" };
		std::vector<std::pair<std::string, COLORREF>> lines;
		Mods(n, lines);
		std::string status;
		COLORREF sc = kGrey;
		if (d.Has(hover)) { status = hover == 0 ? "Where every path starts" : "Learned"; sc = kOk; }
		else if (!d.CanTake(hover)) status = "Learn a node linked to it first";
		else if (spare <= 0) { status = "No passive points to spend"; sc = kBad; }
		else if (hover == m_iPassivePick) { status = "Click again to learn (1 point)"; sc = kGold; }
		else { status = "Click to pick, then again to learn"; sc = kGold; }
		int tw = R.TitleWidth(n.name, 13) + 20;
		wsprintf(cTxt, "%s%s%s", kKind[n.kind], n.branch ? "  -  " : "", kBr[n.branch]);
		tw = (std::max)(tw, R.TextWidthSized(cTxt, 10) + 20);
		tw = (std::max)(tw, R.TextWidthSized(status.c_str(), 10) + 20);
		for (size_t l = 0; l < lines.size(); l++) tw = (std::max)(tw, R.TextWidthSized(lines[l].first.c_str(), 11) + 20);
		const int th = 44 + (int)lines.size() * 15 + 18;
		int tx = ms::X + 16, ty = ms::Y + 12;
		if (tx + tw > X + W) tx = ms::X - 16 - tw;
		if (ty + th > Y + H) ty = Y + H - th;
		R.FillPanel(tx, ty, tx + tw, ty + th, 6, RGB(16, 18, 24), 245, kBranch[n.branch], 170);
		R.TitleOut(tx + 10, ty + 6, n.name, kBranch[n.branch], 13);
		R.TextOutSized(tx + 10, ty + 24, cTxt, kGrey, 10);
		int y = ty + 40;
		for (size_t l = 0; l < lines.size(); l++) { R.TextOutSized(tx + 10, y, lines[l].first.c_str(), lines[l].second, 11); y += 15; }
		R.TextOutSized(tx + 10, y + 2, status.c_str(), sc, 10);
	}
}

void CGame::DlgBoxClick_Passives()
{
	CDialogBox & dlg = m_dialogBoxes[DIALOG_PASSIVE];
	const short X = dlg.m_X, Y = dlg.m_Y;
	std::vector<Btn> btns = PassiveButtons(*this, X, Y);
	const View v = MakeView(*this, X, Y);
	const int step = (int)lroundf(120.0f / v.z);
	for (size_t i = 0; i < btns.size(); i++) {
		const Btn & b = btns[i];
		if (!InRect(b.rc, ms::X, ms::Y)) continue;
		if (!b.enabled) return;
		PlaySound('E', 14, 5);
		switch (b.action) {
		case A_CLOSE: DisableDialogBox(DIALOG_PASSIVE); return;
		case A_ZOOMOUT: m_iPassiveZoom--; return;
		case A_ZOOMIN: m_iPassiveZoom++; return;
		case A_LEFT: m_iPassivePanX -= step; return;
		case A_RIGHT: m_iPassivePanX += step; return;
		case A_UP: m_iPassivePanY -= step; return;
		case A_DOWN: m_iPassivePanY += step; return;
		case A_CENTRE: m_iPassivePanX = m_iPassivePanY = 0; return;
		case A_TOTALS: m_bPassiveTotals = !m_bPassiveTotals; return;
		case A_ORB:
			if (m_dwPassiveOrbAt == 0 || m_dwCurTime - m_dwPassiveOrbAt >= 4000) { m_dwPassiveOrbAt = m_dwCurTime; return; }
			if (m_dwCurTime - m_dwPassiveOrbAt < 300) return;	// the confirming click must be a separate one
			m_dwPassiveOrbAt = 0;
			m_iPassivePick = -1;
			bSendCommand(MSGID_REQUEST_PASSIVE, PASS_UNMAKE, NULL, 0, 0, 0, NULL);
			return;
		case A_TOME:
			bSendCommand(MSGID_REQUEST_PASSIVE, PASS_TOME, NULL, 0, 0, 0, NULL);
			return;
		}
		return;
	}

	if (!InRect(v.rc, ms::X, ms::Y)) return;
	const int n = NodeAt(v, ms::X, ms::Y);
	if (n < 0) {						// empty space: look there
		m_iPassivePanX = (int)lroundf(v.px + (ms::X - v.cx) / v.z);
		m_iPassivePanY = (int)lroundf(v.py + (ms::Y - v.cy) / v.z);
		return;
	}
	if (!m_passive.CanTake(n)) { m_iPassivePick = -1; return; }
	if (m_passive.Spare(m_iLevel) <= 0) { AddEventList("You have no passive points to spend.", 10); return; }
	if (m_iPassivePick != n) { m_iPassivePick = n; m_dwPassivePickAt = m_dwCurTime; PlaySound('E', 14, 5); return; }
	if (m_dwCurTime - m_dwPassivePickAt < 300) return;	// one click that arrived twice is not a confirmation
	m_iPassivePick = -1;
	PlaySound('E', 14, 5);
	bSendCommand(MSGID_REQUEST_PASSIVE, PASS_TAKE, NULL, n, 0, 0, NULL);
}
