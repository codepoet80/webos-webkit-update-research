
#include "config.h"
#include "webkitpalmclipboard.h"

#include <map>
#include <string>


struct PalmClipboard
{
	std::map<std::string,std::string>	data;
	
};

PalmClipboard* palm_clipboard_create()
{
	PalmClipboard * p = new PalmClipboard;	
	return p;
}


void palm_clipboard_destroy(PalmClipboard* c)
{
	delete c;
}

void palm_clipboard_set( PalmClipboard* cp, const char* type, const char* value )
{
	if( !cp )
		return;
	
	cp->data[ type ] = value;
}

int palm_clipboard_get( PalmClipboard* cp, const char* type, const char** outValue )
{
	*outValue = 0;
	std::map<std::string,std::string>::iterator it = cp->data.find(type);
	if( it == cp->data.end() )
		return 0;
	
	*outValue = it->second.c_str();
	return 1;
}

void palm_clipboard_clear( PalmClipboard* cp )
{
	cp->data.clear();
}

