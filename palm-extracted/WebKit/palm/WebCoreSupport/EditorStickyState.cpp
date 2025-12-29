//TODO: complete changeover to KeyStateNode state machine and remove the 

//#include "Event.h"
#include "config.h"

#include "EditorStickyState.h"
#include "EditorClientPalm.h"
#include "SmartTextEngine.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "webosDeviceKeydefs.h"

#include "unicode/uchar.h"

#if 0

#define EC_TRACE(...) \
do { \
    fprintf(stdout, "EditorStickyState: " ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)

#else

#define EC_TRACE(...) (void)0

#endif


const KeyMapType * EditorStickyState::s_deviceKeyMap = 0;


static bool isMetaKey( int key )
{
	return ( key == Key_Shift ) ||
			( key == Key_Alt ) ;
}

static bool isControlKey(int key) 
{
	return ((key == Key_Return) ||
			(key == Key_Backspace) ||
			(key == Key_Space) ||
			(key == Key_PageUp) ||
			(key == Key_PageDown));
}

static bool isStateShiftVariant(KeyStateNode::StickyState state)
{
	return state == KeyStateNode::Shift_Single_Hidden ||
		state == KeyStateNode::Shift_Single ||
		state == KeyStateNode::Shift_Lock;
}

static bool isStateNumVariant(KeyStateNode::StickyState state)
{
	return state == KeyStateNode::Num_Single || state == KeyStateNode::Num_Lock;
}

static const char* getStateName(KeyStateNode::StickyState stateId)
{
	switch (stateId) {
		case KeyStateNode::Normal: return "Normal";
		case KeyStateNode::Shift_Single_Hidden: return "Shift_Single_Hidden";
		case KeyStateNode::Shift_Single: return "Shift_Single";
		case KeyStateNode::Shift_Lock: return "Shift_Lock";
		case KeyStateNode::Num_Single: return "Num_Single";
		case KeyStateNode::Num_Lock: return "Num_Lock";
		default: return "<Unkown>";
	}
}

const char * KeyStateNode::stringState() {
	sprintf(strspace,"%s, spaceAssert? = %s , punctuationAssert? = %s",
			getStateName(state),
			(spaceAssert ? "True" : "False") ,
			(punctuationAssert ? "True" : "False"));
	return strspace;
}

unsigned short KeyStateNode::modWithShift(const unsigned short key) {
	const KeyMapType *e = EditorStickyState::deviceKeymap();
	int index=0;
	while( e[index].devicekey != (int)LAST_KEY ) {
		if( e[index].normal == key ) {
			return (e[index].shift);
		}
		index++;
	}
        // Fixes DFISH-1852.  Not all characters can be found in the device
        // key map - e.g. international characters.  Rather than returning the
        // unmodified key, shift it up by calling u_toupper().
        return u_toupper(key);
}

unsigned short KeyStateNode::modWithOpt(const unsigned short key) {
	const KeyMapType *e = EditorStickyState::deviceKeymap();
	int index=0;
	while( e[index].devicekey != (int)LAST_KEY ) {
		if( e[index].normal == key ) {
			return (e[index].opt);
		}
		index++;
	}
	return key;
}
	
KeyStateNode::TextCaretType KeyStateNode::textCaretType() 
{ 
	return KeyStateNode::Caret_normal;
}

class KeyStateNode_Normal : public KeyStateNode {

public:
	
	KeyStateNode_Normal() : KeyStateNode(KeyStateNode::Normal) {}
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

        if (editor.singleAndLockStatesEnabled()) {
            //check modifier keys first, as these cause a direct transition
            switch (key)
            {
            case Key_Shift:
                nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Normal);
                return KeyStateNode::Transition_Normal;
            case Key_Alt:
                nextState = editor.getState(KeyStateNode::Num_Single, KeyStateNode::Normal);
                return KeyStateNode::Transition_Normal;
            }
        }

		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Normal);
			return KeyStateNode::Transition_Normal;
		}
		
		//check for Return...this goes into Shift Single Hidden
		if (key == Key_Return) {
			nextState = editor.getState(KeyStateNode::Shift_Single_Hidden, KeyStateNode::Normal);
			return KeyStateNode::Transition_Normal;
		}
		
		//check punctuation
		if (editor.isTerminalPunctuationKey(key,modifier)) 
		{
			punctuationAssert = true;
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Normal);
			return KeyStateNode::Transition_Normal;
		}
		
		if ((editor.isTitleCaseEnabled() == true) && (key == Key_Space)) 
		{
		    punctuationAssert = false;
            nextState = editor.getState(KeyStateNode::Shift_Single_Hidden, KeyStateNode::Normal);
            nextState->setSpaceAssert();
            return KeyStateNode::Transition_Normal;
		}
		
		
		//if space character was pressed and punctuation was asserted previously, then transition to Shift Single Hidden
		if (punctuationAssert && (key == Key_Space))
		{
			punctuationAssert = false;
			nextState = editor.getState(KeyStateNode::Shift_Single_Hidden, KeyStateNode::Normal);
			nextState->setSpaceAssert();
			return KeyStateNode::Transition_Normal;
		}
		
		//if backspace key and previous state was Shift Single Hidden, then prevent the previous state from getting overwritten 
		/*
		 * COMMENT: this is necessary because of the following: Say you type in  "Hello. W"
		 * At this point, the current state = Normal, previous state = Shift Single Hidden
		 * If you type a Backspace, then feedKey in this source file will get called first, before EditorClientPalm
		 * has a chance to see the event and act on it. It is in ECP that "corrections" are made to auto-cap and auto-replace
		 * (to undo them), because ECP has more context and state about the current text-field/other editable field that 
		 * is being operated on. Because feedKey in here sees the event first, it will by default make current state = Normal,
		 * previous state = Normal. At that point, didAutocapPreviousKeypress() will return false and ECP will not attempt to undo
		 * the auto-cap
		 */

		if ((key == Key_Backspace) && (editor.didAutocapPreviousKeypress())) {
			spaceAssert = false;
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Normal);
			return KeyStateNode::Transition_RetainPreviousState;
		}

		//Normal key...de-assert punct. and stay in normal mode
		punctuationAssert = false;
		nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Normal);
		return KeyStateNode::Transition_Normal;
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_normal;
	}
	
	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		return key;
	}
	
private:
	
};

class KeyStateNode_ShiftSingleHidden : public KeyStateNode {

public:
	
	KeyStateNode_ShiftSingleHidden() : KeyStateNode(KeyStateNode::Shift_Single_Hidden) {}
	
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

        if (editor.singleAndLockStatesEnabled()) {
            //check modifier keys first, as these cause a direct transition
            switch (key)
            {
            case Key_Shift:
                nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Shift_Single_Hidden);
                return KeyStateNode::Transition_Normal;
            case Key_Alt:
                nextState = editor.getState(KeyStateNode::Num_Single, KeyStateNode::Shift_Single_Hidden);
                return KeyStateNode::Transition_Normal;
            }
        }
		
		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single_Hidden);
			return KeyStateNode::Transition_Normal;
		}
		
		//check for Return...this stays in this mode
		if (key == Key_Return) {
			nextState = editor.getState(KeyStateNode::Shift_Single_Hidden, KeyStateNode::Shift_Single_Hidden);
			return KeyStateNode::Transition_Normal;
		}

		//if space, then assert the space flag, stay in this mode
		if (key == Key_Space) {
			spaceAssert = true;
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Shift_Single_Hidden, KeyStateNode::Shift_Single_Hidden);
			return KeyStateNode::Transition_Normal;
		}
		
		//COMMENT: this can be made more "efficient" but is broken out for easier understanding
		
		//no matter what, at this point the previous key code needs to be reset to prevent erroneous substitutions
		editor.resetPreviousRawKeyCode();
		
		//if space was asserted, and backspace is pressed, then go back to normal mode
		if (spaceAssert && (key == Key_Backspace)) {
			spaceAssert = false;
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single_Hidden);
			return KeyStateNode::Transition_Normal;
		} else if (key == Key_Backspace) {
		    // stay in mode
		    return KeyStateNode::Transition_Normal;
		}
		
		//if normal key, then just go back to normal mode, pass the key on
		spaceAssert = false;
		nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single_Hidden);
		return KeyStateNode::Transition_Normal;
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_normal;
	}
	
	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		if ((isMetaKey(key)) || isControlKey(key))
			return key;
	
			return modWithShift(key);
	}
	
private:
	
};

class KeyStateNode_ShiftSingle : public KeyStateNode {

public:
	
	KeyStateNode_ShiftSingle() : KeyStateNode(KeyStateNode::Shift_Single) {}
	
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

		//check modifier keys first, as these cause a direct transition
		switch (key)
		{
		case Key_Shift:
			nextState = editor.getState(KeyStateNode::Shift_Lock, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		case Key_Alt:
			nextState = editor.getState(KeyStateNode::Num_Single, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		}
		
		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		}
		
		//if Return key pressed, stay in this mode, and assert space (so newline-cancel works)
		if (key == Key_Return)
		{
			editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Shift_Single)->setSpaceAssert();
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		}

		//check punctuation
		if (editor.isTerminalPunctuationKey(key,modifier)) 
		{
			editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single)->setPunctuationAssert();
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		}
				
		//if space, then assert the space flag, stay in this mode
		if (key == Key_Space) {
			spaceAssert = true;
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_Normal;
		}
		
		//COMMENT: this can be made more "efficient" but is broken out for easier understanding
		
		//if space was asserted, and backspace is pressed, then go back to normal mode
		//block the key from being propagated
		if (spaceAssert && (key == Key_Backspace)) {
			spaceAssert = false;
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single);
			return KeyStateNode::Transition_BlockKey;
		}
		
		//if normal key, then just go back to normal mode, pass the key on
		spaceAssert = false;
		nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Single);
		return KeyStateNode::Transition_Normal;
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_arrow;
	}
	
	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		unsigned short optkey = modWithOpt(key);
		if (isStateNumVariant(prevState) && u_isalpha(optkey)) {
			return u_toupper(optkey);
		}
		else {
			return modWithShift(key);
		}
	}
	
private:
	
};

class KeyStateNode_ShiftLock : public KeyStateNode {

public:
	
	KeyStateNode_ShiftLock() : KeyStateNode(KeyStateNode::Shift_Lock)  {}
	
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

		//check modifier keys first, as these cause a direct transition
		switch (key)
		{
		case Key_Shift:
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Lock);
			return KeyStateNode::Transition_Normal;
		case Key_Alt:
			nextState = editor.getState(KeyStateNode::Num_Single, KeyStateNode::Shift_Lock);
			return KeyStateNode::Transition_Normal;
		}

		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Shift_Lock, KeyStateNode::Shift_Lock);
			return KeyStateNode::Transition_Normal;
		}
		
		//if space, then assert the space flag, stay in this mode
		if (key == Key_Space) {
			spaceAssert = true;
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Shift_Lock, KeyStateNode::Shift_Lock);
			return KeyStateNode::Transition_Normal;
		}
		
		// on backspace, transition to normal state only after a space 
		// and if no autoreplace to undo
		// keep space assert on backspace when we have something to undo
		if (spaceAssert) 
		{
		    if (key == Key_Backspace) 
		    {
		        if (editor.getEditorClient() 
		                && ! editor.getEditorClient()->canUndoAutoReplace()) 
		        {
		            // we're not reinserting text that contains a space
		            spaceAssert = false;
		            nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Shift_Lock);
		            return KeyStateNode::Transition_BlockKey;
		        }
		    } else {
		        spaceAssert = false;
		    }
		}
		nextState = editor.getState(KeyStateNode::Shift_Lock, KeyStateNode::Shift_Lock);
		return KeyStateNode::Transition_Normal;				
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_arrowFilled;
	}
	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		return modWithShift(key);
	}
	
private:
	
};

class KeyStateNode_NumSingle : public KeyStateNode {

public:
	
	KeyStateNode_NumSingle() : KeyStateNode(KeyStateNode::Num_Single) {}
	
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

		//check modifier keys first, as these cause a direct transition
		switch (key)
		{
		case Key_Shift:
			nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		case Key_Alt:
			nextState = editor.getState(KeyStateNode::Num_Lock, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		}

		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		}
		
		//if Return key pressed, go to Shift Single, and assert space (so newline-cancel works)
		if (key == Key_Return)
		{
			editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Num_Single)->setSpaceAssert();
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		}
		
		//check punctuation
		if (editor.isTerminalPunctuationKey(key,modifier)) 
		{
			editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Single)->setPunctuationAssert();
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		}
	
		// for keyboards with letters (that we want shifted like AZERTY/QWERTZ) in the opt
		// (aka num) position we want to support a single opt and have it return back to shift lock.
		if (prevState == KeyStateNode::Shift_Lock && u_isalpha(modWithOpt(key))) {
			nextState = editor.getState(prevState, KeyStateNode::Num_Single);
			return KeyStateNode::Transition_Normal;
		}

		//else if normal key, just drop to Normal mode
		nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Single);
		return KeyStateNode::Transition_Normal;
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_circle;
	}

	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		key = modWithOpt(key);
		if (isStateShiftVariant(prevState) && u_isalpha(key)) {
			return u_toupper(key);
		}
		else {
			return key;
		}
	}
private:
	
};

class KeyStateNode_NumLock : public KeyStateNode {

public:
	
	KeyStateNode_NumLock() : KeyStateNode(KeyStateNode::Num_Lock) {}
	
	int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) {
		if (!keyDown)
			return KeyStateNode::Transition_Normal;

		//check modifier keys first, as these cause a direct transition
		switch (key)
		{
		case Key_Shift:
			nextState = editor.getState(KeyStateNode::Shift_Single, KeyStateNode::Num_Lock);
			return KeyStateNode::Transition_Normal;
		case Key_Alt:
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Lock);
			return KeyStateNode::Transition_Normal;
		}

		if (editor.isSmartTextEnabled() == false) {
			nextState = editor.getState(KeyStateNode::Num_Lock, KeyStateNode::Num_Lock);
			return KeyStateNode::Transition_Normal;
		}

		//if space, then assert the space flag, stay in this mode
		if (key == Key_Space) {
			spaceAssert = true;
			editor.setStateDirtyBit(true);
			nextState = editor.getState(KeyStateNode::Num_Lock, KeyStateNode::Num_Lock);
			return KeyStateNode::Transition_Normal;
		}

		//if space was asserted, and backspace is pressed, then go back to normal mode
		//block the key from being propagated
		if (spaceAssert && (key == Key_Backspace)) {
			spaceAssert = false;
			nextState = editor.getState(KeyStateNode::Normal, KeyStateNode::Num_Lock);
			return KeyStateNode::Transition_BlockKey;
		}

		//if normal key, then stay in this mode, pass the key on
		spaceAssert = false;
		nextState = editor.getState(KeyStateNode::Num_Lock, KeyStateNode::Num_Lock);
		return KeyStateNode::Transition_Normal;				
	}
	
	void affectState(EditorStickyState& editorState) {
	}
	
	TextCaretType textCaretType() { 
		return KeyStateNode::Caret_disc;
	}
	
	unsigned short modifyKey(unsigned short key,unsigned short modifier) { 
		key = modWithOpt(key);
		if (isStateShiftVariant(prevState) && u_isalpha(key)) {
			return u_toupper(key);
		}
		else {
			return key;
		}
	}
private:

};

bool EditorStickyState::isTerminalPunctuationKey(unsigned short key,unsigned short modifier)
{
		modifyKeyEvent(key,modifier);
		
		if( key == Key_Return ||
			key == Key_Period ||
			key == Key_Exclamation ||
			key == Key_Question )
		return true;
	else
		return false;
}

EditorStickyState::EditorStickyState(bool enabled) :
	 m_key(0x0)
	,m_keyIsDown(false)
	,m_titleCaseEnabled(false)
	,m_steFeatures(enabled)
    ,m_singleAndLockStatesEnabled(true)
	,m_prevRawKeyCode(0x0)
	,states(NULL)
	,stateflags_dirtyBit(true)
	,stateflags_autocapReverted(false)
	,previousKeyState(NULL)
	,currentKeyState(NULL)
	,m_editorClient(NULL)
{
	initKeyboardDeviceMap();
	states = new KeyStateNode*[KeyStateNode::TOTAL_STATES];
	states[KeyStateNode::Normal] = new KeyStateNode_Normal();
	states[KeyStateNode::Shift_Single_Hidden] = new KeyStateNode_ShiftSingleHidden();
	states[KeyStateNode::Shift_Single] = new KeyStateNode_ShiftSingle();
	states[KeyStateNode::Num_Single] = new KeyStateNode_NumSingle();
	states[KeyStateNode::Shift_Lock] = new KeyStateNode_ShiftLock();
	states[KeyStateNode::Num_Lock] = new KeyStateNode_NumLock();
	reset();
	
}

EditorStickyState::~EditorStickyState()
{
	for (int i = 0;i< KeyStateNode::TOTAL_STATES;i++)
		delete states[i];
	delete[] states;
}

void EditorStickyState::setStateNumLock() {

    if (m_singleAndLockStatesEnabled) {
        currentKeyState->clearPunctuationAssert();
        currentKeyState->clearSpaceAssert();
        currentKeyState = states[KeyStateNode::Num_Lock];
    }
}

void EditorStickyState::setStateShiftLock() {
	
    if (m_singleAndLockStatesEnabled) {
        currentKeyState->clearPunctuationAssert();
        currentKeyState->clearSpaceAssert();
        currentKeyState = states[KeyStateNode::Shift_Lock];
    }
}

void EditorStickyState::setStateNumSingle() {
	
    if (m_singleAndLockStatesEnabled) {
        currentKeyState->clearPunctuationAssert();
        currentKeyState->clearSpaceAssert();
        currentKeyState = states[KeyStateNode::Num_Single];
    }
}

void EditorStickyState::setStateShiftSingle() {
	
    if (m_singleAndLockStatesEnabled) {
        currentKeyState->clearPunctuationAssert();
        currentKeyState->clearSpaceAssert();
        currentKeyState = states[KeyStateNode::Shift_Single];
    }
}


KeyStateNode * EditorStickyState::getState(KeyStateNode::StickyState stateNum, KeyStateNode::StickyState prevStateNum) {
	
	if (stateNum >= KeyStateNode::TOTAL_STATES)
		return NULL;
	KeyStateNode* newState = states[stateNum];
	newState->setPrevState(prevStateNum);

	return newState;
}

void EditorStickyState::dumpState()
{
	printf("key=%02x  caret=%d\n", m_key, textCaret() );
}

/**
 * 
 * COMMENT: relies on the fact that there are is at most 1 object of each state type, and they're all in states[]
 * (i.e. state objects are not copied.) This makes == comparison just a little bit faster, but if the method of state transitioning
 * ever changes to violate this singleton rule, then the comparison should change to actual overriden object == operator use  (e.g. *currentKeyState)
 */
bool EditorStickyState::didAutocapPreviousKeypress() {
	
		return ((currentKeyState == states[KeyStateNode::Normal]) &&
				(previousKeyState == states[KeyStateNode::Shift_Single_Hidden]) &&
				(m_prevRawKeyCode != 0) &&
				(!isTerminalPunctuationKey(m_prevRawKeyCode, -1)));  // we didn't cap terminals
				
}

/**
 * 
 * Used by EditorClientPalm to signal that an autocap has been undone
 */
void EditorStickyState::undidPreviousAutocap() 
{
	previousKeyState = NULL;
	m_prevRawKeyCode = 0;
}

unsigned short EditorStickyState::getPreviousRawKeyCode() {
	return m_prevRawKeyCode;
}

KeyStateNode* EditorStickyState::getCurrentKeyState() {
    return currentKeyState;
}

KeyStateNode::TextCaretType EditorStickyState::feedKeyEvent( bool isKeyDown, unsigned short& key, unsigned short& modifier )
{
	//int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier)
	unsigned short keycopy = key;	
	unsigned short savedKey = m_prevRawKeyCode;
	KeyStateNode * savedState = previousKeyState;
	KeyStateNode * newState = currentKeyState;

	EC_TRACE("feedKeyEvent: key = %u , modifier = %u , current state [%s] , previous state [%s]\n",
			(unsigned int)key,(unsigned int)modifier,currentKeyState->stringState(),(previousKeyState == NULL ? "null" : (previousKeyState->stringState())));

	// Modify key based on state on keyDown. On keyUp, reuse state 
	// that modified keyDown. 
	if (isKeyDown) { 
	    key = currentKeyState->modifyKey(key, modifier);
	} else if (previousKeyState != NULL) {
        key = previousKeyState->modifyKey(key, modifier);
	}
	
	int rc = currentKeyState->transition(*this,newState,isKeyDown,key,modifier);
	
	//TODO: FIXME: actually should be keyed on what 'rc' says...so transition() should be expanded to dictate whether to transition or not. It adds flexibility...
	if (isKeyDown) {	//only transition on keydowns
		previousKeyState = currentKeyState;
		currentKeyState = newState;
		if (!isControlKey(keycopy) && !isMetaKey(keycopy))
			m_prevRawKeyCode = keycopy;
		
	}
	
	currentKeyState->affectState(*this);
	EC_TRACE("feedKeyEvent(): state transitioned to [%s]\n",currentKeyState->stringState());
	if (rc & KeyStateNode::Transition_BlockKey)
		key = 0;
	if (rc & KeyStateNode::Transition_RetainPreviousState) {
		previousKeyState = savedState;
		m_prevRawKeyCode = savedKey;
	}
	
	return currentKeyState->textCaretType();
}

// The key tables are in Host*.cpp files. We'll call a function
// to search this table and modify
void EditorStickyState::modifyKeyEvent( unsigned short& keyCode, unsigned short &modififer )
{
	if( !s_deviceKeyMap )
		return;
	
	switch( m_key )
	{
	case Key_Shift:
		{
			const KeyMapType *e = s_deviceKeyMap;
			int index=0;
			while( e[index].devicekey != (int)LAST_KEY ) {
				if( e[index].normal == keyCode ) {
					keyCode = e[index].shift;
				}
				index++;
			}
		}
		break;
		
	case Key_Alt:
		{
			const KeyMapType *e = s_deviceKeyMap;
			int index=0;
			while( e[index].devicekey != (int)LAST_KEY ) {
				if( e[index].normal == keyCode ) {
					keyCode = e[index].opt;
				}
				index++;
			}
		}
		break;
		
	default:
		EC_TRACE("EditorStickyState::modifyKeyEvent unknown meta key %02x\n", m_key );
	}
}

/**
 * 
 * used by disableSmartTextFeatures to kill off "sub"state info that may affect things if STE is turned on/off at runtime
 * (we don't support this right now, but it's safer this way in case it ever shows up)
 */
void EditorStickyState::clearAllStateFlags() {
	
	if (stateflags_dirtyBit == false)
		return;
	
	for (int i=0;i<KeyStateNode::TOTAL_STATES;i++) {
		states[i]->clearStateFlags();
	}
	stateflags_dirtyBit = false;
	
}

KeyStateNode::TextCaretType EditorStickyState::textCaret()
{
	return currentKeyState->textCaretType();
}

void EditorStickyState::setEditorClient(WebKit::EditorClient* editorClient)
{
    m_editorClient = editorClient;
}

void EditorStickyState::disableSmartTextFeatures()
{ 
    EC_TRACE("\t\t\t\tEditorStickyState::disableSmartTextFeatures() DISABLED\n");
	m_steFeatures = false; 
	m_titleCaseEnabled = false;
	
	reset(); 
}

void EditorStickyState::enableSmartTextFeatures(KeyStateNode::StickyState resetState)
{ 
	EC_TRACE("\t\t\t\tEditorStickyState::enableSmartTextFeatures() ENABLED\n");
	m_steFeatures = true; 
	reset(resetState); 
    EC_TRACE("\t\t\t\tEditorStickyState::enableSmartTextFeatures() resuming in [%s] state\n",currentKeyState->stringState());
}

void EditorStickyState::enableTitleCase()
{
    EC_TRACE("\t\t\t\tEditorStickyState::enableTitleCase() ENABLED\n");
    m_titleCaseEnabled = true;

    reset();
}

void EditorStickyState::disableTitleCase()
{
    EC_TRACE("\t\t\t\tEditorStickyState::disableTitleCase() DISABLED\n");
    m_titleCaseEnabled = false;

    reset();
}

void EditorStickyState::enableSingleAndLockStates(bool enable)
{
    EC_TRACE("\t\t\t\t%s %s\n", __PRETTY_FUNCTION__, enable ? "ENABLED" : "DISABLED");
    m_singleAndLockStatesEnabled = enable;

    reset();
}

/**
 * @brief Changes key state without resetting previous raw key values
 * 
 */
void EditorStickyState::setKeyState(KeyStateNode::StickyState state) 
{
    clearAllStateFlags();

    if (m_steFeatures) {
        currentKeyState = states[state];
    } else { 
        currentKeyState = states[KeyStateNode::Normal];
    }
    
}

void EditorStickyState::reset(KeyStateNode::StickyState resetState)
{

	EC_TRACE("\t\t\t\t---> RESET STICKY STATE <---\n");
	m_key = 0;	
	m_keyIsDown = false;
	
	setKeyState(resetState);
	
	previousKeyState = NULL;
	m_prevRawKeyCode = 0;
}

const KeyMapType * EditorStickyState::deviceKeymap()
{
	if( !s_deviceKeyMap ) 
		EditorStickyState::initKeyboardDeviceMap();
	
	return s_deviceKeyMap;
}

void EditorStickyState::initKeyboardDeviceMap()
{	
	if( s_deviceKeyMap )
		return;
	
	s_deviceKeyMap = webosGetDeviceKeymap();
}


