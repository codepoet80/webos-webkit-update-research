
#ifndef __EditorStickyState_h__
#define __EditorStickyState_h__

#include "webosDeviceKeymap.h"

namespace WebKit
{
class EditorClient;
}

class EditorStickyState;

class KeyStateNode {

public:
	
	enum StickyState {
		Normal = 0,
		Shift_Single_Hidden,
		Shift_Single,
		Shift_Lock,
		Num_Single,
		Num_Lock,
		TOTAL_STATES
	};

	/**
	 * Actually a bitmask.
	 */
	enum {
		Transition_Normal = 0x0,
		Transition_BlockKey = 0x1,
		Transition_RetainPreviousState = 0x2
	};
	enum TextCaretType {
		Caret_normal,
		Caret_circle,
		Caret_disc,
		Caret_arrow,
		Caret_arrowFilled
	};
	
	/**
	 * @return Transition_Normal for default action (pass key onward with modification),
	 *         or bit-OR with Transition_ enums for desired behavior.
	 */
	virtual int transition(EditorStickyState& editor,KeyStateNode *& nextState,bool keyDown,unsigned short& key, unsigned short& modifier) = 0;
	virtual void affectState(EditorStickyState& editorState) = 0;
	virtual const char * stringState();
	bool operator==(const KeyStateNode& cmp) const {return (this->state == cmp.state);}
	bool operator!=(const KeyStateNode& cmp) const {return (this->state != cmp.state);}
	
	//a bit of a cheat, to allow spaceAssert "sub"state var to be changed by the other states.
	// the alternative is to pass this through transition() but that's uglier, I think
	void setPunctuationAssert() { punctuationAssert = true; }
	void clearPunctuationAssert() { punctuationAssert = false; }
	void setSpaceAssert() { spaceAssert = true; }
	void clearSpaceAssert() { spaceAssert = false; }
	
	void setPrevState(StickyState state) { prevState = state; }
	StickyState getStateId() { return state;}
	virtual TextCaretType textCaretType();
	virtual unsigned short modifyKey(unsigned short key,unsigned short modifier) { return key;}
	static unsigned short isOptKeyAlpha(const unsigned short key);
	static unsigned short modWithShift(const unsigned short key);
	static unsigned short modWithOpt(const unsigned short key);
	
	virtual void clearStateFlags() {
		punctuationAssert = false;
		spaceAssert = false;
	}
	
protected:
	
	KeyStateNode(StickyState _state) : state(_state), prevState(Normal), punctuationAssert(false) , spaceAssert(false) {}
	StickyState state;
	StickyState prevState;	
	
	bool punctuationAssert;
	bool spaceAssert;
	
private:
	KeyStateNode();
	char strspace[256];
};

class EditorStickyState
{
public:
	EditorStickyState(bool featuresEnabled);
	~EditorStickyState();
	
	KeyStateNode::TextCaretType feedKeyEvent( bool isKeyDown, unsigned short &keyCode, unsigned short& modifier );
	void reset(KeyStateNode::StickyState resetState=KeyStateNode::Normal);
	void setKeyState(KeyStateNode::StickyState state);
	void dumpState();
	KeyStateNode::TextCaretType textCaret( );
	
	void disableSmartTextFeatures();
	void enableSmartTextFeatures(KeyStateNode::StickyState resetState=KeyStateNode::Normal);
	bool isSmartTextEnabled() { return m_steFeatures;}

    void enableSingleAndLockStates(bool enable);
    bool singleAndLockStatesEnabled() const { return m_singleAndLockStatesEnabled; }
	
	void disableTitleCase();
	void enableTitleCase();
	bool isTitleCaseEnabled() { return m_titleCaseEnabled; }

	bool didAutocapPreviousKeypress();
	void undidPreviousAutocap();
	unsigned short getPreviousRawKeyCode();
	KeyStateNode* getCurrentKeyState();
	void resetPreviousRawKeyCode() { m_prevRawKeyCode = 0;}
	
	bool setStateAutoAsserted(bool v=true);
	bool isTerminalPunctuationKey(unsigned short key,unsigned short modifier);
	void setStateNumLock();
	void setStateShiftLock();
	void setStateNumSingle();
	void setStateShiftSingle();
	
	enum
	{
		Normal,
		SingleShot,
		Locked
	};
	
	KeyStateNode * getState(KeyStateNode::StickyState stateNum, KeyStateNode::StickyState prevStateNum);
	bool 			getStateDirtyBit() {return stateflags_dirtyBit;}
	void			setStateDirtyBit(bool v=true) {stateflags_dirtyBit = v;}
	bool 			getStateAutocapRevertBit() {return stateflags_autocapReverted;}
	void			setStateAutocapRevertBit(bool v=true) {stateflags_autocapReverted = v;}

	void setEditorClient(WebKit::EditorClient* editorClient);
	WebKit::EditorClient* getEditorClient() { return m_editorClient; }
	
	static const KeyMapType * deviceKeymap();
	
private:

	static void initKeyboardDeviceMap();
	
	void modifyKeyEvent( unsigned short& keyCode, unsigned short &modififer );

	unsigned short	m_key;	
	bool			m_keyIsDown;
	
	bool            m_titleCaseEnabled;  // capitalize after space
	bool			m_steFeatures;
    bool            m_singleAndLockStatesEnabled; // support shift/num single/lock state changes
	unsigned short 	m_prevRawKeyCode;	
	
	KeyStateNode *  previousKeyState;
	KeyStateNode *	currentKeyState;
	KeyStateNode **  states;
	bool			stateflags_dirtyBit;
	bool			stateflags_autocapReverted;
	
	WebKit::EditorClient* m_editorClient;
	
	static  	const KeyMapType * s_deviceKeyMap;
	
	void clearAllStateFlags();
	
};

#endif
