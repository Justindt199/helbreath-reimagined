// windows.h must come before Shared/typedefs.h: its List/Vector macros otherwise break the Windows headers.
#include <windows.h>
#include "mail.h"

Mail::Mail(uint32 __id, string __title, string __sender, Date d) :
	id(__id), title(__title), sender(__sender), date(d), msg(NULL)
{
}

Mail::~Mail()
{
	if(msg)
		delete msg;

	for(Vector<CItem*>::iterator it = items.begin();
		it != items.end();
		++it)
	{
		delete *it;
	}
}