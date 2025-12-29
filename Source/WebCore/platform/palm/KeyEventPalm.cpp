
// Copyright 2008-2009 Palm Inc.

#include "config.h"
#include <stdio.h>
#include "PlatformKeyboardEvent.h"
#include "KeyboardCodes.h"
#include "CString.h"
#include "EditorStickyState.h"
#include "webosDeviceKeymap.h"
#include "webosDeviceKeydefs.h"


namespace WebCore {
	

// FIXME: This is incomplete. We could change this to mirror
// more like what Firefox does, and generate these switch statements
// at build time.
static String keyIdentifierForPalmCode(unsigned int key, unsigned int modifier)
{
	String ident;
	
	switch ( key ) 
	{
		case VK_MENU:
			ident = "Alt";
			break;
		case VK_CLEAR:
			ident = "Clear";
			break;
		case VK_RETURN:
			ident = "Enter";
			break;
		case 0x80: 
			ident = "Shift";
			break;
		case 0x81: 
			ident = "Alt";
			break;
		case 0x82: 
			ident = "Ctrl";
			break;
		case 0x83: 
			ident = "Opt";	
			break;
		case Key_PageDown:
			ident = "PageDown";
			break;
		case Key_PageUp:
			ident = "PageUp";
			break;
		case VK_PAUSE:
			ident = "Pause";
			break;
		case Key_Right:
			ident = "Right";
			break;
		case Key_Left:
			ident = "Left";
			break;
		case Key_Up:
			ident = "Up";
			break;
		case Key_Down:
			ident = "Down";
			break;
		case VK_SCROLL:
			ident = "Scroll";
			break;
		// Standard says that DEL becomes U+007F.
		case VK_DELETE:
			ident = "U+007F";
			break;
		case Key_CoreNavi_Meta:
			ident = "Meta";
			break;
		case Key_CoreNavi_Launcher:
			ident = "Launcher";
			break;
		case Key_CoreNavi_Back:
			ident = "Back";
			break;
		case Key_CoreNavi_Home:
			ident = "Home";
			break;
		case Key_CoreNavi_Next:
			ident = "Next";
			break;
		case Key_CoreNavi_Previous:
			ident = "Previous";
			break;
		case Key_CoreNavi_Menu:
			ident = "Menu";
			break;
//		case Key_Backspace:
//			ident = "Back";
//			break;
	}
	
	// if (modifier & 0x80 ) ... 
	if( !ident.length() ) 
		return String::format("U+%04X", key );
	
	return ident;
}


// Convert the raw key code (and modifier) into a virtual
// key code.
static int windowsKeyCodeForKeyEvent(unsigned int key, unsigned int modifier)
{	
	const KeyMapType* e = EditorStickyState::deviceKeymap();
	while( (int)e->devicekey != (int)LAST_KEY )
	{
		if( modifier & 0x80 ) {
			if( (int)key == e->shift ) {
				return e->virtualkeycode;
			}
		}
		else if( (int)key == e->normal )
			return e->virtualkeycode;
			
		e++;
	}
	
	return key;
}

static inline String singleCharacterString(UChar c) 
{
    return String((UChar*)&c, 1);
}


// Note : the 'key' passed in is taken from the table in luna-keymap-headers.
PlatformKeyboardEvent::PlatformKeyboardEvent(unsigned short key, unsigned short modifier, bool isKeyUp)
{	
	m_rawKeyCode = key;
	m_rawModifier = modifier;
	m_nativeVirtualKeyCode = 0x0;
	
	m_type=(isKeyUp?KeyUp:KeyDown);
    m_text=(singleCharacterString(key));
    m_unmodifiedText=(singleCharacterString(key));
    m_keyIdentifier=(keyIdentifierForPalmCode(key,modifier));
    m_autoRepeat=(false);
    m_windowsVirtualKeyCode=(windowsKeyCodeForKeyEvent(key,modifier));
    m_isKeypad=(false);
    m_shiftKey=(false);
    m_ctrlKey=(false);
    m_altKey=(false);
    m_metaKey=(false);
	
	if( modifier & 0x80 )
		m_shiftKey = true;

	if( modifier & 0x40 )
		m_ctrlKey = true;

	if( modifier & 0x20 )
		m_metaKey = true;
	
	// we can't enable this as the "alt" key (orange dot on Palm)
	// is not *really* a detectable modifier; it actually changes 
	// the keycode emitted.
	//if( modifier & 0x20 )
	//	m_altKey = true;
	
	switch( key ) {
	case Key_Shift: // Key_Shift
		m_shiftKey = true;
		m_text = "";
		break;
	case Key_Alt: // Key_Alt
		m_altKey = true;
		m_text = "";
		break;
	case Key_Ctrl: // Key_Ctrl (desktop)
		m_ctrlKey = true;
		m_text = "";
		break;
	case Key_Option: // Key_Option
		m_ctrlKey = true;
		 m_text = "";
		break;
	case Key_CoreNavi_Meta: // meta key
		m_metaKey = true;
		m_text = "";
		break;
	case Key_PageUp: // page up, down
	case Key_PageDown:
    case Key_Insert:
    case Key_Home:
    case Key_End:
    case Key_Delete:
    case Key_Up:
    case Key_Down:
    case Key_Left:
    case Key_Right:
		m_text = "";
		m_unmodifiedText = "";
	}

	if( getenv("WK_WATCHKEYS") || getenv("DEBUG_KEYS") )
	{	
		printf(" %s key=0x%02x modifier=0x%02x m_windowsVirtualKeyCode=%d (0x%02x) m_keyIdentifier=%s m_text='%s' m_unmodifiedText='0x%02x'  s=%d m=%d a=%d o=%d \n", 
			(isKeyUp ? "up" : "down"),key, modifier, m_windowsVirtualKeyCode, m_windowsVirtualKeyCode,
			m_keyIdentifier.utf8().data(), m_text.utf8().data(), *m_unmodifiedText.utf8().data(), 
			m_shiftKey, m_metaKey, m_altKey, m_ctrlKey );
	}
	//m_windowsVirtualKeyCode = xx; // this is what gets transmitted as the event.keyCode
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == KeyDown);
    m_type = type;
    
    if (backwardCompatibilityMode)
        return;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    return false;
}

/* static */ void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}


}

