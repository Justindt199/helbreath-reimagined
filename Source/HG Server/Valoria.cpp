// Valoria.cpp: systems ported from VALORIA (Twin Realms). See Valoria.h.

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "Valoria.h"

namespace
{
	std::map<std::string, ValoriaGearBonus> g_gear;
}

int Valoria_LoadGear(const char * pFileName)
{
	FILE * pFile = fopen(pFileName, "rt");
	if (pFile == NULL) return -1;

	g_gear.clear();
	char cLine[256], cName[64];
	while (fgets(cLine, sizeof(cLine), pFile) != NULL)
	{
		if (cLine[0] == '/' || cLine[0] == '\r' || cLine[0] == '\n') continue;
		ValoriaGearBonus b;
		memset(&b, 0, sizeof(b));
		const int n = sscanf(cLine, "%63s %d %d %d %d %d %d %d %d %d", cName,
			&b.str, &b.dex, &b.vit, &b.iint, &b.mag, &b.hp, &b.mp, &b.def, &b.crit);
		if (n == 10) g_gear[cName] = b;
	}
	fclose(pFile);
	return (int)g_gear.size();
}

const ValoriaGearBonus * Valoria_FindGear(const char * pItemName)
{
	if (pItemName == NULL) return NULL;
	std::map<std::string, ValoriaGearBonus>::const_iterator it = g_gear.find(pItemName);
	return (it == g_gear.end()) ? NULL : &it->second;
}
