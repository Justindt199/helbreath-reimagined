// Msg.cpp: implementation of the CMsg class.
//
//////////////////////////////////////////////////////////////////////

#include "Msg.h"
ChatType CMsg::predremove;
uint32 CMsg::nmsgs = 0;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMsg::CMsg(char cType, char * pMsg, DWORD dwTime)
{
	m_cType = cType;

	m_pMsg = NULL;
	m_pMsg = new char [strlen(pMsg) + 1];
	ZeroMemory(m_pMsg, strlen(pMsg) + 1);
	strcpy(m_pMsg, pMsg);
	m_dwTime = dwTime;
	m_iObjectID = -1;
	m_cChannel = -1;
	memset(m_cName, 0, sizeof(m_cName));
	m_pPrev = NULL;

	m_msgid = nmsgs++;
}

CMsg::~CMsg()
{
	if (m_pMsg) delete[] m_pMsg;
	if (m_pPrev) delete[] m_pPrev;
}

void CMsg::SetPrev(const char * pPrev)
{
	if (m_pPrev) delete[] m_pPrev;
	m_pPrev = NULL;
	if (pPrev == NULL || pPrev[0] == 0) return;
	m_pPrev = new char[strlen(pPrev) + 1];
	strcpy(m_pPrev, pPrev);
}

bool CMsg::RemovePred(const CMsg * msg)
{
	return msg->m_dwTime == predremove;
}