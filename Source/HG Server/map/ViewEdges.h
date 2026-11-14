// ViewEdges.h: which tiles of a player's view window are new after moving one step.
//
// The view window is W x H tiles (W = 2*halfX + 1, H = 2*halfY + 2). After a step in
// direction cDir (1 = north, clockwise to 8 = north-west) the tiles along the leading
// edge(s) are new, and the server sends what is on them. For the legacy 21 x 16 window
// this produces exactly the same tiles, in the same order, as the old fixed tables in
// char\movement.h.
//////////////////////////////////////////////////////////////////////

#ifndef HB_VIEWEDGES_H
#define HB_VIEWEDGES_H

#include <vector>

inline void GetViewEdgeTiles(int cDir, int W, int H, std::vector<int> & xs, std::vector<int> & ys)
{
	xs.clear();
	ys.clear();
	const int R = W - 1, B = H - 1;
	int i;
	switch (cDir) {
	case 1:	// north: top row
		for (i = 0; i <= R; i++) { xs.push_back(i); ys.push_back(0); }
		break;
	case 2:	// north-east: top row, then right column
		for (i = 0; i <= R; i++) { xs.push_back(i); ys.push_back(0); }
		for (i = 1; i <= B; i++) { xs.push_back(R); ys.push_back(i); }
		break;
	case 3:	// east: right column
		for (i = 0; i <= B; i++) { xs.push_back(R); ys.push_back(i); }
		break;
	case 4:	// south-east: right column, then bottom row right to left
		for (i = 0; i <= B; i++) { xs.push_back(R); ys.push_back(i); }
		for (i = R - 1; i >= 0; i--) { xs.push_back(i); ys.push_back(B); }
		break;
	case 5:	// south: bottom row
		for (i = 0; i <= R; i++) { xs.push_back(i); ys.push_back(B); }
		break;
	case 6:	// south-west: left column, then bottom row
		for (i = 0; i <= B; i++) { xs.push_back(0); ys.push_back(i); }
		for (i = 1; i <= R; i++) { xs.push_back(i); ys.push_back(B); }
		break;
	case 7:	// west: left column
		for (i = 0; i <= B; i++) { xs.push_back(0); ys.push_back(i); }
		break;
	case 8:	// north-west: left column bottom to top, then top row
		for (i = B; i >= 0; i--) { xs.push_back(0); ys.push_back(i); }
		for (i = 1; i <= R; i++) { xs.push_back(i); ys.push_back(0); }
		break;
	}
}

#endif // HB_VIEWEDGES_H
