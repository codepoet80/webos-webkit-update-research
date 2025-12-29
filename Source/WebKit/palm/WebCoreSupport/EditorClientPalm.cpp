
// Copyright 2008,2010 Palm Inc.

#include "config.h"
#include "EditorClientPalm.h"

#include "EditCommand.h"
#include "Editor.h"
#include "FocusController.h"
#include "Frame.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "HTMLElement.h"
#include "CSSStyleDeclaration.h"
#include "PlatformKeyboardEvent.h"
#include "SmartTextEngine.h"
#include "CString.h"
#include "RenderThemePalm.h"
#include "FrameView.h"
#include "RenderView.h"
#include "SpellCheck.h"
#include "DocumentMarker.h"
#include "TextBoundaries.h"

#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include <glib.h>
#include <fstream>

#include "palmwebpage.h"
#include "palmwebpageprivate.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "palmwebglobal.h"

#include "webkitpalmsettings.h"

#include "webosDeviceKeymap.h"
#include "webosDeviceKeydefs.h"

#include "TextLinker.h"

#include "htmlediting.h"
#include "visible_units.h"

#include "SpellingWidgetController.h"
#include "ClipboardWidgetController.h"
#include "visible_units.h"

#include "HTMLInputElement.h"

#include <palmimedefines.h>

using namespace WebCore;
#undef COPY_DEBUG

#ifdef COPY_DEBUG 
#define COPY_TRACE(...) \
do { \
    fprintf(stdout, "EditorClientPalm: %s: ", __FUNCTION__ ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)
#else
#define COPY_TRACE(...) (void)0
#endif


#if 0

#define EC_TRACE(...) \
do { \
    fprintf(stdout, "EditorClient: " ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)

#else

#define EC_TRACE(...) (void)0

#endif

#define BITSET(var,flags)   var |= (flags)
#define BITCLEAR(var,flags) var &= ~(flags)

// This first section is for use by the Framework only and will likely be deprecated.
static const AtomicString STE_MODE_PRIMARY("x-palm-ste-mode");
static const AtomicString STE_MODE_ATTR_EMOTICON_ON("emoticons-on");
static const AtomicString STE_MODE_ATTR_EMOTICON_OFF("emoticons-off");
static const AtomicString STE_MODE_ATTR_AC_TITLE("cap-title");
static const AtomicString STE_MODE_ATTR_AC_SENTENCE("cap-sentence");
static const AtomicString STE_MODE_ATTR_AC_LOWER("cap-lowercase");
static const AtomicString STE_MODE_ATTR_AR_ON("replace-on");
static const AtomicString STE_MODE_ATTR_AR_OFF("replace-off");
static const AtomicString STE_MODE_ATTR_LINKER_ON("textlinker-on");
static const AtomicString STE_MODE_ATTR_LINKER_OFF("textlinker-off");

// Public element attributes to control text assist features
static const AtomicString STE_TAG_DISABLE_ALL("x-palm-disable-ste-all");
static const AtomicString STE_TAG_DISABLE_AUTOCAP("x-palm-disable-auto-cap");
static const AtomicString STE_TAG_DISABLE_AUTOSUB("x-palm-disable-auto-sub");
static const AtomicString STE_TAG_DISABLE_AUTOREPLACE("x-palm-disable-auto-replace");
static const AtomicString STE_TAG_MODE_AUTOCAP_TITLE("x-palm-title-cap");
static const AtomicString STE_TAG_ENABLE_EMOTICONS("x-palm-enable-emoticons");
static const AtomicString STE_TAG_ENABLE_LINKER("x-palm-enable-linker");
static const AtomicString STE_ATTR_TEXT_ENTRY("x-palm-text-entry");
static const AtomicString STE_ATTR_SPELL_CHECK("x-palm-spell-check");
static const AtomicString STE_ATTR_DISABLE_IME("x-palm-disable-ime");
static const AtomicString VK_ATTR_ENTER_KEY_LABEL("x-palm-enter-key-label");
static const AtomicString VK_ATTR_DISABLE_WORDCOMPLETIONS("x-palm-word-completions");
static const AtomicString VK_ATTR_INPUT_TYPE("x-palm-input-type");

static const AtomicString ATOM_FALSE("false");
static const AtomicString ATOM_TRUE("true");

static const AtomicString ATOM_DISABLED("disabled");
static const AtomicString ATOM_UNDERLINE("underline");
static const AtomicString ATOM_AUTO_CORRECT("autoCorrect");
static bool dumpEditingCallbacks = false;
static bool acceptsEditing = true;
void palm_dump_editing_callbacks(bool b)
{
    dumpEditingCallbacks = b;
}

void palm_dump_resource_load_callbacks(bool b)
{
    acceptsEditing = b;
}

static String dumpPath(WebCore::Node *node)
{
    String str = node->nodeName();

    WebCore::Node *parent = node->parentNode();
    while (parent) {
        str.append(" > ");
        str.append(parent->nodeName());
        parent = parent->parentNode();
    }
    return str;
}

static String dumpRange(WebCore::Range *range)
{
    if (!range)
        return String("(null)");
    String str;
    WebCore::ExceptionCode code;
    str = String::format("range from %d of %s to %d of %s",
                range->startOffset(code), dumpPath(range->startContainer(code)).utf8().data(),
                range->endOffset(code), dumpPath(range->endContainer(code)).utf8().data());
    return str;
}


namespace WebKit {
	

EditorClient::EditorClient(Palm::WebPage* page)
    : m_page(page) 
	, m_smartTextEngine( new Nova::SmartTextEngine() )
	, m_editing(false)
	, m_runningTextLinker(false)
	, m_disableEditorEventCount(0)
	, m_trackballEnabled(true)
    , m_smartInsertDelete(true)
    , m_substitutionsPanelVisible(false)
    , m_spellingUiVisible(false)
    , m_automaticQuoteSubstitutionEnabled(true)
    , m_automaticLinkDetectionEnabled(true)
    , m_automaticDashSubstitutionEnabled(true)
	, m_spellChecker(Palm::SpellCheck::getSpellCheck())
	, m_stickyState(PalmBrowserSettings()->smartTextEnabled())
	, m_spellingWidget(0)
	, m_allowWordCompletions(false)
	, m_hasCompleteWord(false)
    , m_expectsInputMethodStateCallback(false)
    , m_clipboardController(0)
    , m_wordCompController(page)
{
	m_stickyState.setEditorClient(this);

	m_spellingWidget = new SpellingWidgetController(page, PalmBrowserSettings()->spellWidgetYMargin);
    if (PalmBrowserSettings()->clipboardWidgetsEnabled)
        m_clipboardController = new Palm::ClipboardController(page);
}

void EditorClient::handleInputMethodKeydown(KeyboardEvent* event )
{
}

bool EditorClient::shouldDeleteRange(Range* range)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n", dumpRange(range).utf8().data());

    return true;
}

bool EditorClient::shouldShowDeleteInterface(HTMLElement*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    return false;
}

bool EditorClient::isContinuousSpellCheckingEnabled()
{
	return PalmBrowserSettings()->checkSpelling != WebKitPalmSettings::DISABLED;
}

bool EditorClient::isGrammarCheckingEnabled()
{
	return false;
}

int EditorClient::spellCheckerDocumentTag()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    return 0;
}

bool EditorClient::shouldBeginEditing(WebCore::Range* range)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
            printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n", dumpRange(range).utf8().data());
	m_editing = true;
    return true;
}

bool EditorClient::shouldEndEditing(WebCore::Range* range)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
            printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n", dumpRange(range).utf8().data());
	m_editing = false;
    return true;
}

bool EditorClient::shouldInsertText(const String& string, Range* range, EditorInsertAction action)
{
    if (dumpEditingCallbacks) {
        static const char *insertactionstring[] = {
            "WebViewInsertActionTyped",
            "WebViewInsertActionPasted",
            "WebViewInsertActionDropped",
        };

        printf("EDITING DELEGATE: shouldInsertText:%s replacingDOMRange:%s givenAction:%s\n",
               (string).utf8().data(), dumpRange(range).utf8().data(), insertactionstring[action]);
    }
    return acceptsEditing;
}

bool EditorClient::shouldChangeSelectedRange(Range* fromRange, Range* toRange, 
                                             EAffinity selectionAffinity,
                                             bool stillSelecting)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks) {
        static const char *affinitystring[] = {
            "NSSelectionAffinityUpstream",
            "NSSelectionAffinityDownstream"
        };
        static const char *boolstring[] = {
            "FALSE",
            "TRUE"
        };

        printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s toDOMRange:%s affinity:%s stillSelecting:%s\n",
               dumpRange(fromRange).utf8().data(),
               dumpRange(toRange).utf8().data(),
               affinitystring[selectionAffinity], boolstring[stillSelecting]);
    }

    // If a selection change occurred because of user action (not because
    // we internally called editor commands), record direction of selection change
    // and save current selection as a pivot point
    if (editorEventsAllowed() 
            && fromRange 
            && toRange 
            && m_selectionForCopy.selecting) 
    {
        // we need from and to range to track selection changes for copy

        int comparePos = comparePositions(fromRange->startPosition(), toRange->startPosition());
        COPY_TRACE("%s: comparing start positions: %d\n", __FUNCTION__, comparePos);

        m_selectionForCopy.directionFromPivot = (SelectionForCopy::Direction)comparePos;
        
        ExceptionCode ec;
        
        // it's possible we're placing a cursor at the same spot where it was after the document was no longer editable
        if (fromRange->startOffset(ec) == toRange->startOffset(ec)
                && fromRange->endOffset(ec) == toRange->endOffset(ec)
                && m_selectionForCopy.choosingPivot) 
        {
            selectForCopyPivotWord();
            m_selectionForCopy.choosingPivot = false;
            COPY_TRACE("%s: should have chosen pivot when selection didn't change\n", __FUNCTION__);
        }
    }
    
    ExceptionCode ec;

    if (fromRange != NULL) {
        EC_TRACE("%s: fromRange (%d, %d)\n", __FUNCTION__, 
                fromRange->startOffset(ec),
                fromRange->endOffset(ec));
    }
    
    if (toRange != NULL) {
        EC_TRACE("%s: toRange (%d, %d)\n", __FUNCTION__, 
                toRange->startOffset(ec),
                toRange->endOffset(ec));
        
        // we have moved the cursor before undo of autocap
        // this function is not fired during backwards delete, so we don't destroy
        // undo information when we clear replace and autocap variables
        m_smartTextEngine->lastCapOffset = -1;

    }

    return true; // allows selection change 
}

bool EditorClient::shouldApplyStyle(WebCore::CSSStyleDeclaration* style,
                                      WebCore::Range* range)
{
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n",
               style->cssText().utf8().data(), dumpRange(range).utf8().data());
    return acceptsEditing;
    return true;
}

bool EditorClient::canUndoAutoReplace()
{
    return false;
}

bool EditorClient::shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    return true;
}

void EditorClient::didBeginEditing()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
	m_editing = true;
	
	if (m_spellingWidget->isVisible()) {
		m_spellingWidget->hide();
	}
}

void EditorClient::didEndEditing()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: webViewDidEndEditing:WebViewDidEndEditingNotification\n");

	m_editing = false;

    // is focus being removed?
    if (!m_expectsInputMethodStateCallback) {

        editorFocusChanged(0);
    }

	if (m_spellingWidget->isVisible()) {
		m_spellingWidget->hide();
	}
}

void EditorClient::respondToChangedContents()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");

    // link pattern may have been created around cursor
    runTextLinkerAtCursor();

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame)
		return;

    if (m_allowWordCompletions && !frame->editor()->hasComposition()) {
    	checkWordCompletion();
    }
}

void EditorClient::respondToChangedSelection()
{
    if (m_clipboardController)
        m_clipboardController->respondToChangedSelection();

    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    if (dumpEditingCallbacks)
        printf("EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }

    VisibleSelection sel = frame->selection()->selection();
    if (sel.isNone()) {
        return;
    }

	// clean up composition state if the selection is changing not due to composition update
    if (!frame->editor()->ignoreCompositionSelectionChange()) {
        frame->editor()->confirmCompositionWithoutDisturbingSelection();
    }

    // Expand/contract selection or create a selection pivot word
    //
    // changed selection may have been a mouse down for copy selection
    // pick "pivot word" -- word entirely selected, around which we will create
    // copy selection

	if (!editorEventsAllowed()) {
		return;
	}

	clearWordCompletion();

    setStickyStateByCurrentSelection(false);
    KeyStateNode* keyStateNode = m_stickyState.getCurrentKeyState();
    bool autoCap = keyStateNode && keyStateNode->getStateId() == KeyStateNode::Shift_Single_Hidden;
    Palm::WebView* view = m_page ? m_page->view() : NULL;
    if (view && view->client())
        view->client()->autoCapEnabled(autoCap);

    if (m_selectionForCopy.selecting)
    {

        // if selection size is == 0 
        if (m_selectionForCopy.choosingPivot) 
        {
            selectForCopyPivotWord();
            m_selectionForCopy.choosingPivot = false;
            COPY_TRACE("%s: should have chosen pivot\n", __FUNCTION__);
        } else {
            
            // extending selection range from pivot word
            
            COPY_TRACE("%s: we tapped\n", __FUNCTION__);
            Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
            if (frame && frame->editor()) 
            {
                VisibleSelection currSel = frame->selection()->selection();
                VisibleSelection mark = frame->mark();
                RefPtr<Range> currRange = currSel.firstRange();
                RefPtr<Range> markRange = mark.firstRange();

#ifdef COPY_DEBUG
                if (markRange && currRange) {
                    ExceptionCode ec;
                    int markStart = currRange->startOffset(ec);
                    int markEnd = markRange->startOffset(ec);
                    int comparePos = comparePositions(currRange->startPosition(), markRange->startPosition());
                    COPY_TRACE("%s: COMPARE POSITIONS: %d, currSel start: %d, mark start: %d, currSel end: %d, mark end: %d\n", __FUNCTION__, comparePos, 
                            currRange->startPosition().offset(), markRange->startPosition().offset(),
                            currRange->endPosition().offset(), markRange->endPosition().offset());
                }
#endif

                bool selectingMark = false;

                
                if (currRange && markRange && mark.start().node() == currSel.start().node()) {
                    ExceptionCode ec;
                    int currEnd = currRange->endOffset(ec);
                    int markStart = markRange->startOffset(ec);
                    int markEnd = markRange->startOffset(ec);
                    if (currEnd <= markEnd && currEnd >= markStart)                            
                    {
                        // tap is within original word mark, so just highlight mark
                        selectingMark = true;
                        disallowEditorEvents();
                        frame->selection()->setSelection(mark);
                        allowEditorEvents();
                    } 
                } 
                 
                
                if (!selectingMark) {
                    // create selection that goes to mark
                    disallowEditorEvents();

                    changeSelectionToWordBoundary();

                    COPY_TRACE("%s: setting mark after changing selection to word boundary\n", __FUNCTION__);
                    
                    frame->setMark(frame->selection()->selection());
                    
                    allowEditorEvents();
                }
            }
        }
    }

    if (view && view->client()) {
        view->client()->selectionChanged();
    }
}

bool EditorClient::setStickyStateByCurrentSelection(bool forceStateChange)
{
    if (!PalmBrowserSettings()->smartTextEnabled()) {
        return false;
    }

    if (!m_editing) {
        return false;
    }

	const unsigned int acFlags = Palm::EDITOR_STATE_AUTOCAP_SENTENCE | Palm::EDITOR_STATE_AUTOCAP_TITLE;
    if (!m_stickyState.isSmartTextEnabled() || !(getStateForFocusedNode() & acFlags)) {
        return false;
    }

    // never override a locked state
	int currentState = m_stickyState.getCurrentKeyState()->getStateId(); 
    if (currentState == KeyStateNode::Shift_Lock || currentState == KeyStateNode::Num_Lock) {
        return false;
    }
        
    // if we are not forcing, we are allowed to overwrite only Normal
    if (!forceStateChange && 
            (currentState == KeyStateNode::Shift_Single
                    || currentState == KeyStateNode::Num_Single)) {
        return false;
    }
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return false;
    }

    const VisibleSelection sel(frame->selection()->selection() );

    if (sel.isNone()) {
        return false;
    }
   
	RefPtr<Range> toRange = sel.firstRange();

    int cursorPos = -1;

    // Get cursor position from range, which contains selection boundaries

    if (toRange != NULL) 
    {
        ExceptionCode ec;
        
        EC_TRACE("%s: toRange (%d, %d)\n", __FUNCTION__, 
                toRange->startOffset(ec),
                toRange->endOffset(ec));
        
        // range isn't updated correctly when cursor is at beginning of line
        if (sel.start().atFirstEditingPositionForNode()) {
            cursorPos = 0;
        } else {
            cursorPos = toRange->startOffset(ec);
        }
    }
    
    if (cursorPos == -1) {
        return false;
    }

    // Get content at cursor position and determine what state we should be in

    String content = sel.start().node()->textContent();
 
    if ( (cursorPos == 0 && shouldAutocapByPreviousSibling())
            || shouldTitleCaseByPosition(cursorPos, content)
            || shouldAutocapByPosition(cursorPos, content)) 
    {
        EC_TRACE("%s: SELECTION turns ON ShiftSingleHidden!\n", __FUNCTION__);
        m_stickyState.setKeyState(KeyStateNode::Shift_Single_Hidden);
    } else {
        EC_TRACE("%s: SELECTION turns ON Normal\n", __FUNCTION__);
        m_stickyState.setKeyState(KeyStateNode::Normal);
    }
        
    return true;
}

/**
 * Assumes direction of text and looks at left sibling. If sibling is BR, cap.
 * Or, send last char to shouldTitleCaseByPosition and shouldAutocapByPosition
 * 
 */
bool EditorClient::shouldAutocapByPreviousSibling()
{
    if (PalmBrowserSettings()->runningInBrowserServer) {
        // too expensive when in copy & paste mode
        return false;
    }
    
	if (!PalmBrowserSettings()->checkGrammar) {
		return false;
	}
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return false;
    }

    VisibleSelection sel = frame->selection()->selection();

    if (sel.isNone()) {
        return false;
    }
    
    Node* node = sel.start().node();
    if (!node) {
        return false;
    }
    
    Node* prevSibling = node->previousSibling();
    if (!prevSibling) {
        return true;
    }
    
    if (prevSibling->isLink()) {
        // don't cap if we're next to a link
        return false;
    }
    
    RenderObject* r = prevSibling->renderer();
    if (!r || !r->isText() || r->isBR() || r->isRunIn()) {
        return true;
    }
    
    return true;
}

/**
 * Determines if Shift_Single_Hidden should be on. Looks to left of cursor to 
 * determine if character following cursorPos should be capitalized, according 
 * to definition of autocap: empty text field, beginning of text field, terminal 
 * character followed by break character.
 * 
 */
bool EditorClient::shouldAutocapByPosition(int cursorPos, const WTF::String& content)
{
    EC_TRACE("%s: Cursor pos: %d, Element with content: '%s'\n",
            __FUNCTION__, cursorPos, content.utf8().data());

    int originalCursorPos = cursorPos;
    
    // move cursor to preceding char                                                                             
    cursorPos--;
    UChar charAtCursor = content.characterStartingAt(cursorPos);

    // move cursor left past blank chars
    while (cursorPos >= 0 && (charAtCursor == ' ' || charAtCursor == 0xA0))
    {
        cursorPos--;
        charAtCursor = content.characterStartingAt(cursorPos);
    }

    // capitalize if original position is more than one char away from a punctuation                          
    bool validTerminalChar = ((originalCursorPos-cursorPos) > 1
            && Nova::Utils::isTerminalPunctuation(charAtCursor));

    bool capByPrevSibling = false;
    if (cursorPos < 0) {
        capByPrevSibling = shouldAutocapByPreviousSibling();
    }
    
    // cap if we were at the beginning of the text area or a terminal character
    if (capByPrevSibling || validTerminalChar)
    {
        EC_TRACE("%s: SELECTION should autocap\n", __FUNCTION__);
        return true;
    } 

    EC_TRACE("%s: SELECTION shouldn't autocap\n", __FUNCTION__);
    return false;
}

/**
 * Determines if Single_Shift_Hidden should be on. Checks if space is 
 * immediately to left of cursor position, if the field is blank, or cursor is 
 * at beginning of field.
 * 
 */
bool EditorClient::shouldTitleCaseByPosition(int cursorPos, const WTF::String& content)
{
    
    if (!m_stickyState.isTitleCaseEnabled()) {
        EC_TRACE("%s: SELECTION shouldn't title case - mode not enabled\n", 
                __FUNCTION__);        
        return false;
    }
    
    EC_TRACE("%s: Cursor pos: %d, Element with content: '%s'\n",
            __FUNCTION__, cursorPos, content.utf8().data());

    // move cursor to preceding char                                                                             
    cursorPos--;
    UChar charAtCursor = content.characterStartingAt(cursorPos);

    // cap if we were at the beginning of the text area or a terminal character
    if (cursorPos < 0 || (charAtCursor == ' ' || charAtCursor == 0xA0))
    {
        EC_TRACE("%s: SELECTION should title case\n", __FUNCTION__);
        return true;
    } 

    EC_TRACE("%s: SELECTION shouldn't title case\n", __FUNCTION__);
    return false;
}

void EditorClient::didWriteSelectionToPasteboard()
{
}

void EditorClient::didSetSelectionTypesForPasteboard()
{
}

// Returns whether the user can edit the HTML content of the document
bool EditorClient::isEditable()
{
	return false;
}

void EditorClient::registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	m_undoStack.push(command);
}

void EditorClient::registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand> command)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	m_redoStack.push(command);
}

void EditorClient::clearUndoRedoOperations()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	m_undoStack.clear();
	m_redoStack.clear();
}

bool EditorClient::canUndo() const
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	return !m_undoStack.isEmpty();
}

bool EditorClient::canRedo() const
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	return !m_redoStack.isEmpty();
}

void EditorClient::undo()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	if (!m_undoStack.isEmpty()) {
		WTF::RefPtr<WebCore::EditCommand> command = m_undoStack.pop();
		command->unapply();
	}
}

void EditorClient::redo()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
	if (!m_redoStack.isEmpty()) {
		WTF::RefPtr<WebCore::EditCommand> command = m_redoStack.pop();
		command->reapply();
	}
}

bool EditorClient::shouldInsertNode(Node* node, Range* range, EditorInsertAction action)
{
    if (dumpEditingCallbacks) {
        static const char *insertactionstring[] = {
            "WebViewInsertActionTyped",
            "WebViewInsertActionPasted",
            "WebViewInsertActionDropped",
        };

        printf("EDITING DELEGATE: shouldInsertNode:%s replacingDOMRange:%s givenAction:%s\n", dumpPath(node).utf8().data(),
               dumpRange(range).utf8().data(), insertactionstring[action]);
    }
    return true;
}

void EditorClient::pageDestroyed()
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    delete this;
}

bool EditorClient::smartInsertDeleteEnabled()
{
	// this is really only for clipboard operations, not input methods
    return m_smartInsertDelete;
}

void EditorClient::toggleContinuousSpellChecking()
{
}

void EditorClient::toggleGrammarChecking()
{
}

bool EditorClient::isSelectTrailingWhitespaceEnabled()
{
	return false;
}

/*
 * COMMENT: 1/20/2009 hvrsalovic:
 * 
 * STE-autocap is now implemented in a totally different way, using sticky state to assert caps at the right time.
 * As a result, a lot of the code in handleKeyboardEvent() below that deals with auto cap is meaningless, and will go away 
 * when the new method is completely tested.
 */
void EditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode())
        return;
	
    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent /*|| kevent->type() == PlatformKeyboardEvent::KeyUp*/)
        return;

	EC_TRACE("EditorClient keycode=0x%02x type=%d  RawKeyDown=%d \n", event->keyCode(), kevent->type(), PlatformKeyboardEvent::RawKeyDown  );

	// as soon as we get a key event, trackball or key, hide the spelling widget
	if (m_spellingWidget->isVisible()) {
		m_spellingWidget->hide();
	}

    Node* start = frame->selection()->start().node();
    if (!start)
        return;

    // check if we should undo last spelling correction
	SelectionController* selController = frame->selection();
	VisibleSelection curVisibleSel = selController->selection();
	
	if (event->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown &&
		event->keyCode() == VK_SPACE &&
		curVisibleSel.isCaret()) {

		if (commitWordCompletion()) {
			event->setDefaultHandled();
			return;
		}
	}
	else {
		clearWordCompletion();
	}

	m_allowWordCompletions = (event->keyCode() != VK_BACK && event->keyCode() != VK_DELETE);

	if (event->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown &&
		event->keyCode() == VK_BACK &&
		curVisibleSel.isCaret()) {

		WebCore::EditCommand* lastComm = frame->editor()->lastEditCommand();
		if (lastComm && lastComm->editingAction() == EditActionAutoReplace) {
			// Extract information from the last auto-replace command about the bounds of
			// the replaced word. Then select the current word that will be "undone"
			// and replace it with existing replacement's marker description which contains the
			// original word on which auto-replace was done
			unsigned movedBy = 0;
			VisiblePosition position = curVisibleSel.visibleStart();
			UChar32 chrAt = position.characterAfter();
            bool performUndo = (lastComm->endingSelection().visibleEnd() == position);
            do {
                if (lastComm->endingSelection().visibleEnd() == position) {
                    performUndo = true;
                    break;
                }
                ++movedBy;
                position = position.previous();
                chrAt = position.characterAfter();
            } while (position.isNotNull() && !u_isalnum(chrAt));

			if (performUndo) {
				const VisibleSelection wordToReplace(lastComm->startingSelection().visibleStart(), lastComm->endingSelection().visibleEnd());
				selController->setSelection(wordToReplace);
				// check if there is a replacement or ShortcutReplacement marker set
				DocumentMarker* existingMarker = frame->document()->markers()->markerContainedInRange(wordToReplace.firstRange().get(), DocumentMarker::Replacement);
				if (!existingMarker) {
					existingMarker = frame->document()->markers()->markerContainedInRange(wordToReplace.firstRange().get(), DocumentMarker::ShortcutReplacement);
				}

				if (existingMarker && !existingMarker->description.isEmpty()) {
					String description(existingMarker->description);

					frame->editor()->changeBackToReplacedString(description, existingMarker->type);
					frame->selection()->moveTo(frame->selection()->end());
					// restore the caret position
					while (movedBy-- > 0)
						frame->selection()->modify(SelectionController::AlterationMove, SelectionController::DirectionForward, CharacterGranularity);
					return;
				}
				else {
					// restore selection to what it was
					selController->setSelection(curVisibleSel);
				}
			}
		}
	}

    // FIXME: Use GtkBindingSet instead of this hard-coded switch
    // http://bugs.webkit.org/show_bug.cgi?id=15911
	
    if ((event->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown)
			&& (event->keyCode() == VK_BACK)
			&& (event->shiftKey()))
    {
		// Delete key with Shift: delete the previous word, including any trailing white space between it and the current cursor position
		{ 
			Editor* editor = frame->editor();
			if( !editor )
				return;

			const VisibleSelection sel( frame->selection()->selection() );
			String wholeText = sel.start().node()->textContent();
			
			if( sel.isCaret() )
			{
				String wholeText = sel.start().node()->textContent();

				//The offset() number is always the index of the character that the "cursor" would be in front of
				// e.g   |here  => offset() = 0,   he|re => offset() = 2
				EC_TRACE( "ShiftDel: caret=%d base=%d extent=%d [%s]\n", sel.isCaret(), sel.base().offsetInContainerNode(), sel.extent().offsetInContainerNode(), wholeText.utf8().data() );
				size_t mark=sel.base().computeOffsetInContainerNode();
				
				//if I'm at the beginning, there's nothing to delete
				if (mark == 0)
					return;
				
				disallowEditorEvents();

				if (mark != wholeText.length()) {	//NOT at the end of the whole text
					//if I'm in the middle of a word, then delete forward first
					if (!Nova::Utils::isBreakChar(wholeText.characterStartingAt(mark-1)) &&
							(!Nova::Utils::isBreakChar(wholeText.characterStartingAt(mark))) ) {
						editor->command("DeleteWordForward").execute();
					}
				}
				
				//delete word backward
				editor->command("DeleteWordBackward").execute();
				
				const VisibleSelection selectionAfterDelete( frame->selection()->selection() );
				
				EC_TRACE("ShiftDel:\t\t-- AFTER -- The current selection parameters are start = %d , base = %d , extent = %d , end = %d --\n",
						selectionAfterDelete.start().offsetInContainerNode(),
						selectionAfterDelete.base().offsetInContainerNode(),
						selectionAfterDelete.extent().offsetInContainerNode(),
						selectionAfterDelete.end().offsetInContainerNode());
				
				wholeText = selectionAfterDelete.start().node()->textContent();
				int cursorPos = selectionAfterDelete.end().computeOffsetInContainerNode();
				
				// move cursor to next character to the left
				cursorPos--;
				
				// skip break characters to the left of cursor
				while (cursorPos >= 0 && Nova::Utils::isBreakChar(wholeText.characterStartingAt(cursorPos))) {
					cursorPos--;
				}
				allowEditorEvents();
				
			}
		}

		m_smartTextEngine->reset();

		return;
		
    }


	if( PalmBrowserSettings()->smartTextEnabled() 
			&& event->keyEvent()->type() == PlatformKeyboardEvent::RawKeyDown
			&& canAutoCorrectSpellingErrors() )
	{
		
		Editor* editor = frame->editor();
		if( !editor )
			return;
		
		int currCursorPos = frame->selection()->selection().base().computeOffsetInContainerNode(); // ANTHONY
		
		if (event->keyCode() == VK_BACK)
		{

            EC_TRACE("%s: backspace at cursor pos: %d, last cap: %d\n",
                    __FUNCTION__, currCursorPos,
                    m_smartTextEngine->lastCapOffset);
			
			bool bspActionTaken=false;
			// check if last state was autocap and cursor wasn't moved
			if (m_stickyState.didAutocapPreviousKeypress()
					&& m_smartTextEngine->lastCapOffset != -1)
			{
				editor->command("DeleteBackward").execute();
				UChar rchar = m_stickyState.getPreviousRawKeyCode();
				editor->command("InsertText").execute(String(&rchar,1));
				m_stickyState.undidPreviousAutocap();
				bspActionTaken = true;
			}


			if (bspActionTaken)
				return;
		}
	}
	// end : auto-text replace.

    bool setDefaultHandled = true;

    if (start->isContentEditable()) {
        switch (kevent->windowsVirtualKeyCode()) {
            case VK_BACK:
                if (kevent->ctrlKey())
                    frame->editor()->command("DeleteWordBackward").execute();
                else
                    frame->editor()->command("DeleteBackward").execute();
                setDefaultHandled = false;
                break;
            case VK_DELETE:
                 if (kevent->ctrlKey())
                    frame->editor()->command("DeleteWordForward").execute();
                else
                    frame->editor()->command("DeleteForward").execute();
                setDefaultHandled = false;
                break;
            case VK_LEFT:
                disallowEditorEvents();
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveWordLeftAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveWordLeft").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveLeftAndModifySelection").execute();
                else
                    frame->editor()->command("MoveLeft").execute();
                allowEditorEvents();
                break;
            case VK_RIGHT:
                disallowEditorEvents();
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveWordRightAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveWordRight").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveRightAndModifySelection").execute();
                else
                    frame->editor()->command("MoveRight").execute();
                allowEditorEvents();
                break;
            case VK_UP:
                disallowEditorEvents();
                if (kevent->shiftKey())
                    frame->editor()->command("MoveUpAndModifySelection").execute();
                else
                    frame->editor()->command("MoveUp").execute();
                allowEditorEvents();
                break;
            case VK_DOWN:
                disallowEditorEvents();
                if (kevent->shiftKey())
                    frame->editor()->command("MoveDownAndModifySelection").execute();
                else
                    frame->editor()->command("MoveDown").execute();
                allowEditorEvents();
                break;
            case VK_PRIOR:  // PageUp
                frame->editor()->command("MovePageUp").execute();
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->command("MovePageDown").execute();
                break;
            case VK_HOME:
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveToBeginningOfDocumentAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveToBeginningOfDocument").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveToBeginningOfLineAndModifySelection").execute();
                else
                    frame->editor()->command("MoveToBeginningOfLine").execute();
                break;
            case VK_END:
                if (kevent->ctrlKey() && kevent->shiftKey())
                    frame->editor()->command("MoveToEndOfDocumentAndModifySelection").execute();
                else if (kevent->ctrlKey())
                    frame->editor()->command("MoveToEndOfDocument").execute();
                else if (kevent->shiftKey())
                    frame->editor()->command("MoveToEndOfLineAndModifySelection").execute();
                else
                    frame->editor()->command("MoveToEndOfLine").execute();
                break;
            case VK_RETURN:
                frame->editor()->command("InsertLineBreak").execute();
                setDefaultHandled = false;
                break;
            case VK_TAB:
                return;
            default:
                if (!kevent->ctrlKey() &&
                    !kevent->altKey() &&
                    !kevent->text().isEmpty()) {
                    if (event->charCode() < ' ')
                        return;
                    frame->editor()->insertText(kevent->text(), event);
                } else if (kevent->metaKey()) {
                    switch (kevent->windowsVirtualKeyCode()) {
                        case VK_B:
                            frame->editor()->command("ToggleBold").execute();
                            break;
                        case VK_I:
                            frame->editor()->command("ToggleItalic").execute();
                            break;
                        case VK_Y:
                            frame->editor()->command("Redo").execute();
                            break;
                        case VK_Z:
                            frame->editor()->command("Undo").execute();
                            break;
                        default:
                            return;
                    }
                } else return;
        }
    } else {											// isContentEditable() == false
        switch (kevent->windowsVirtualKeyCode()) {
            case VK_UP:
                frame->editor()->command("MoveUp").execute();
                break;
            case VK_DOWN:
                frame->editor()->command("MoveDown").execute();
                break;
            case VK_PRIOR:  // PageUp
                frame->editor()->command("MovePageUp").execute();
                break;
            case VK_NEXT:  // PageDown
                frame->editor()->command("MovePageDown").execute();
                break;
            case VK_HOME:
                if (kevent->ctrlKey())
                    frame->editor()->command("MoveToBeginningOfDocument").execute();
                break;
            case VK_END:
                if (kevent->ctrlKey())
                    frame->editor()->command("MoveToEndOfDocument").execute();
                break;
            default:
                return;
        }
    }
	
    if (setDefaultHandled)
        event->setDefaultHandled();

	if( PalmBrowserSettings()->smartTextEnabled()
		&& event->keyEvent()->type() == PlatformKeyboardEvent::Char
		&& canAutoCorrectSpellingErrors()
		&& m_editing)
	{
		// Here we handle the type-as-you-go modifier that Nova defines.

		VisibleSelection sel( frame->selection()->selection() );

		if (sel.isCaret() &&
				(Nova::Utils::isBreakChar((UChar)event->keyCode())
						|| Nova::Utils::isTerminalPunctuation((UChar)event->keyCode())))
		{
			Editor* editor = frame->editor();
			if (!editor) {
				return;
			}

			// didn't autocap, so resetting undo
			m_smartTextEngine->lastCapOffset = -1;

		} else {
			// not a break char, so couldn't have autocapped or replaced
			m_smartTextEngine->resetUndo();
		}
	}
}

EditorClient::~EditorClient()
{
	delete m_smartTextEngine;
	delete m_spellingWidget;
    delete m_clipboardController;
}

void EditorClient::textFieldDidBeginEditing(Element* e)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);

	m_editing = true;
}

void EditorClient::textFieldDidEndEditing(Element*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);

	m_editing = false;

    // is focus being removed?
    if (!m_expectsInputMethodStateCallback) {

        editorFocusChanged(0);
    }
}

void EditorClient::textDidChangeInTextField(Element*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
}

bool EditorClient::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    return false;
}

void EditorClient::textWillBeDeletedInTextField(Element*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
}

void EditorClient::textDidChangeInTextArea(Element*)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
}

void EditorClient::ignoreWordInSpellDocument(const String& word)
{
    EC_TRACE("<-------- %s ---- '%s' ---->\n", __FUNCTION__, word.utf8().data());
}

void EditorClient::learnWord(const String& word)
{
    EC_TRACE("<-------- %s ---- '%s' ---->\n", __FUNCTION__, word.utf8().data());
	m_spellChecker->learnWord(word);
}

void EditorClient::checkSpellingOfString(const UChar* s, int length, int* misspellingLocation, int* misspellingLength)
{
}

void EditorClient::checkGrammarOfString(const UChar* s, int length, Vector<GrammarDetail>&, int*, int*)
{
    String str(s, length);

    printf("checkGrammarOfString of \"%s\"\n", str.utf8().data());
}

void EditorClient::updateSpellingUIWithGrammarString(const String& str, const GrammarDetail&)
{
    g_debug("updateSpellingUIWithGrammarString: '%s'", str.utf8().data());
}

void EditorClient::updateSpellingUIWithMisspelledWord(const String& word)
{
    g_debug("updateSpellingUIWithMisspelledWord: '%s'", word.utf8().data());
}

void EditorClient::showSpellingUI(bool show)
{
    g_debug("showSpellingUI: %c", show ? 'Y' : 'N');
    m_spellingUiVisible = show;
}

bool EditorClient::spellingUIIsShowing()
{
    return m_spellingUiVisible;
}

void EditorClient::getGuessesForWord(const String& word, Vector<String>& guesses)
{
	if (word.isEmpty()) {
		return;
	}

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame)
		return;

	Palm::SpellCheck::SpellCheckWordInfo info;
	// TODO what to do with spelling widget if word is not in the cache?
	if (Palm::SpellCheck::SpellCheckReady == m_spellChecker->getGuessesForWord(word, info, frame)) {
		WTF::Vector<Palm::SpellCheck::WordGuess>::const_iterator i;
		for (i = info.guesses.begin(); i != info.guesses.end(); ++i) {
			guesses.append(i->word);
		}
	}
}

bool EditorClient::getElementAttrBoolVal(const Element* element, const AtomicString& valName)
{
	const AtomicString& value = element->getAttribute(valName);

	if (value == ATOM_TRUE)
		return true;
	else if (value == ATOM_FALSE)
		return false;
	else
		return false;
}

// map HTMLInputElement::InputType to our own internal values
static PalmIME::FieldType mapInputTypeToFieldType(HTMLInputElement::InputType type)
{
	switch (type) {
	case HTMLInputElement::TEXT: 		return PalmIME::FieldType_Text;
	case HTMLInputElement::PASSWORD: 	return PalmIME::FieldType_Password;
	case HTMLInputElement::SEARCH: 		return PalmIME::FieldType_Search;
	case HTMLInputElement::RANGE: 		return PalmIME::FieldType_Range;
	case HTMLInputElement::EMAIL: 		return PalmIME::FieldType_Email;
	case HTMLInputElement::NUMBER: 		return PalmIME::FieldType_Number;
	case HTMLInputElement::TELEPHONE: 	return PalmIME::FieldType_Phone;
	case HTMLInputElement::URL: 		return PalmIME::FieldType_URL;
	case HTMLInputElement::COLOR: 		return PalmIME::FieldType_Color;
	default: 							return PalmIME::FieldType_Text;
	}
	return PalmIME::FieldType_Text;
}

// map x-palm attribute and map them to HTMLInputElement::InputType values
static HTMLInputElement::InputType getInputTypeForElement(const Element* element)
{
    if (!element)
        return HTMLInputElement::TEXT;

    const AtomicString& attrVal = element->getAttribute(VK_ATTR_INPUT_TYPE);
    if (attrVal.isEmpty() || equalIgnoringCase(attrVal, "text"))
        return HTMLInputElement::TEXT;
    if (equalIgnoringCase(attrVal, "password"))
        return HTMLInputElement::PASSWORD;
    if (equalIgnoringCase(attrVal, "search"))
        return HTMLInputElement::SEARCH;
    if (equalIgnoringCase(attrVal, "range"))
        return HTMLInputElement::RANGE;
    if (equalIgnoringCase(attrVal, "email"))
        return HTMLInputElement::EMAIL;
    if (equalIgnoringCase(attrVal, "number"))
        return HTMLInputElement::NUMBER;
    if (equalIgnoringCase(attrVal, "tel"))
        return HTMLInputElement::TELEPHONE;
    if (equalIgnoringCase(attrVal, "url"))
        return HTMLInputElement::URL;
    if (equalIgnoringCase(attrVal, "color"))
        return HTMLInputElement::COLOR;

    return HTMLInputElement::TEXT;
}

void EditorClient::editorFocusChanged(Node* focusedNode)
{
    // notifies the client view of input focus changes
    Palm::WebView* view = m_page->view();
    if (!view || !view->client())
        return;

    HTMLInputElement::InputType fieldType = HTMLInputElement::TEXT;
    if (focusedNode && focusedNode->isElementNode()) {
        Element* focusedElement = static_cast<Element*>(focusedNode);
        const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(toInputElement(focusedElement));
        if (inputElement && inputElement->isTextField()) {
            fieldType = inputElement->inputType();
        }
        else {
            fieldType = getInputTypeForElement(focusedElement);
        }
    }

	bool focused = (focusedNode != 0);
	PalmIME::EditorState editorState(mapInputTypeToFieldType(fieldType));
	unsigned int state = 0;

    clearWordCompletion();

	const Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (frame)
	{
		Document* document = frame->document();
		if (document)
		{
			Node* node = document->focusedNode();

			// FIXME: disable object element behavior for now, this will break browser and email
			if (node && node->isElementNode() && !isObjectElement(node))
			{
				document->updateLayoutIgnorePendingStylesheets();

				// can we advance focus to next/previous fields?
				if (document->nextFocusableNode(node, 0) || document->previousFocusableNode(node, 0))
					editorState.actions = static_cast<PalmIME::FieldAction>(editorState.actions | PalmIME::FieldAction_Next | PalmIME::FieldAction_Previous);

				Element* element = static_cast<Element*>(node);

				state = getStateForElement(element);
				if (state & Palm::EDITOR_STATE_EMOTICONS)
					editorState.flags = static_cast<PalmIME::FieldFlags>(editorState.flags | PalmIME::FieldFlags_Emoticons);

				const AtomicString& enterkeylabel = element->getAttribute(VK_ATTR_ENTER_KEY_LABEL);
				CString	string = enterkeylabel.string().utf8();
//				string = String().fromUTF8("123456789012345678901234567890").utf8();
//				string = String().fromUTF8("12345‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›‹›").utf8();
				unsigned int length = string.length();
				if (length > 0)
				{
					const char * str = string.data();
					if (length >= sizeof(editorState.enterKeyLabel))
						length = sizeof(editorState.enterKeyLabel) - 1;
					unsigned int k = 0;
					do {
						editorState.enterKeyLabel[k] = str[k];
					} while (++k < length);
					while (k > 0 && (str[k] & 0x80) != 0 && (str[k] & 0xC0) != 0xC0)	// top bit 0 or highest two bits 11? first byte of a utf8 char
						--k;
					editorState.enterKeyLabel[k] = 0;
				}
			}
		}
	}

	view->client()->editorFocused(focused, editorState);

	// CFISH-1020: make legacy focus call to satisfy browserserver on pre-Dartfish builds
	view->client()->editorFocused(focused, state);
}

void EditorClient::setInputMethodState(bool useInputMethod)
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);
    
	m_stickyState.reset();

    m_expectsInputMethodStateCallback = false;
	
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame)
        return;

	Node* node = frame->document()->focusedNode();
	bool focused = (node != NULL);
    m_editing = focused;

    // assume we're editing when an input method is needed when
    // document()->focusedNode() == NULL. this happens if this call fires before
    // document pointer is changed to a new document instance which has
    // the focused node. this is our chance to tell browser-adapter that
    // an editable field is in focus, so do it.
    //
    // also send this message to BA when useInputMethod==false and document
    // has focused node. this can happen when focus changes to a node
    // that's not the root editable element (see Document::Document::setFocusedNode
    // line 2638). we must notify browser-adapter because we're still editing.
    Node* focusedNode = ((useInputMethod || focused) && isFocusableElement(node)) ? node : 0;
    editorFocusChanged(focusedNode);

	// The _ime_state parameter only tells us to use an input method
	// or not. We need to see we're entering focus on an edit field
	// or leaving it (we know one of the two is happening)

	if (focused) {

		m_smartTextEngine->lastCapOffset = -1;
		
		// Is there a tag present to set to number mode?
		if( !node->isElementNode() ) return;
		Element* el = (Element*)node;
		
		const AtomicString& idAttr = el->getAttribute(HTMLNames::idAttr);
		//EC_TRACE("\t\t\t\t\tNow editing field: [%s]\n",((String)s).utf8().data());
		
		const AtomicString& typeAttr = el->getAttribute( HTMLNames::typeAttr );
		ExceptionCode ec;
		
		if (el->hasAttribute(STE_MODE_PRIMARY)) {
			//this element has not been parsed for STE 
			EditorClient::setSTEAttributesFromModestring(el);
			//remove the attribute so it won't be reparsed
			el->removeAttribute(STE_MODE_PRIMARY,ec);
		}

    	unsigned int steState = getStateForFocusedNode();
		bool adjustingCase = steState & (Palm::EDITOR_STATE_AUTOCAP_SENTENCE | Palm::EDITOR_STATE_AUTOCAP_TITLE);
		
		if (adjustingCase) {

			if (steState & Palm::EDITOR_STATE_AUTOCAP_TITLE) {
				EC_TRACE("%s: Title Case turning ON\n", __FUNCTION__);
				m_stickyState.enableTitleCase();
			} else if (m_stickyState.isTitleCaseEnabled()) {
				EC_TRACE("%s: Title Case turning OFF\n", __FUNCTION__);
				m_stickyState.disableTitleCase();
			}
			
			// Turn on STE with single shift if field is empty
			
			//TODO: improvement would be to try and figure out which sticky state the field left off in
			//EC_TRACE("\t\t\t\t\tenabling STE, but in which mode? field is: [%s]\n",el->innerText().utf8().data());

			// We need to update layout to position cursor correctly after line boxes are laid out
			frame->document()->updateLayoutIgnorePendingStylesheets();
			
			m_stickyState.enableSmartTextFeatures(KeyStateNode::Normal);
        } else {
			m_stickyState.disableSmartTextFeatures();
        }

		if ((steState & Palm::EDITOR_STATE_TEXT_ENTRY_OPT_LOCK) == Palm::EDITOR_STATE_TEXT_ENTRY_OPT_LOCK)
			m_stickyState.setStateNumLock();
		else if ((steState & Palm::EDITOR_STATE_TEXT_ENTRY_SHIFT_LOCK) == Palm::EDITOR_STATE_TEXT_ENTRY_SHIFT_LOCK)
			m_stickyState.setStateShiftLock();
		else if (steState & Palm::EDITOR_STATE_TEXT_ENTRY_OPT)
			m_stickyState.setStateNumSingle();
		else if (steState & Palm::EDITOR_STATE_TEXT_ENTRY_SHIFT)
			m_stickyState.setStateShiftSingle();
	}
}

int EditorClient::textCaretDecoration()
{
	if( m_editing )
	{
		switch( m_stickyState.textCaret() ) {
		case KeyStateNode::Caret_circle:
			return TextCaretDecoration::CircleHollow;
		case KeyStateNode::Caret_disc:
			return TextCaretDecoration::CircleFilled;
		case KeyStateNode::Caret_arrow:
			return TextCaretDecoration::ArrowHollow;
		case KeyStateNode::Caret_arrowFilled:
			return TextCaretDecoration::ArrowFilled;
		}
	}
	return TextCaretDecoration::None; 
}

void EditorClient::translateKeyWithStickyState( unsigned short& key, unsigned short& modifier, bool keyDown )
{
    EC_TRACE("<-------- %s -------->\n", __FUNCTION__);

	// Note: disable sticky-state translation when meta-key is down. This is so the framework
	// can process shortcuts independent of sticky state, and remain unware of all the physical 
	// keymappings.
    if( !(modifier & 0x0020) && m_editing
			&& PalmBrowserSettings()->enableStickyState && canDoStickyState() )
	{
		if( keyDown) 
		{
			EC_TRACE("translateKeyWithStickyState in( key=%d mod=0x%02x )\n",key,modifier);
		}
		unsigned short unmodifiedKey = key;
		m_stickyState.feedKeyEvent( keyDown, key, modifier );
		if( keyDown) 
		{
			EC_TRACE(" >> translateKeyWithStickyState out( key=%d mod=0x%02x )\n",key,modifier);
		}

        if (m_smartTextEngine->lastCapOffset == -1 && key != VK_BACK)
        {
            if (m_stickyState.didAutocapPreviousKeypress()) 
            {
                Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
                if (!frame || !frame->document()->focusedNode())
                    return;

                int currCursorPos = frame->selection()->selection().base().computeOffsetInContainerNode();
                m_smartTextEngine->lastCapOffset = currCursorPos;

                printf("%s: raw key that wasn't modified: %c\n", __FUNCTION__, (UChar)unmodifiedKey);
                EC_TRACE("%s: Autocapped at cursor pos: %d\n", __FUNCTION__, currCursorPos);
            }
        }

	}
}

void EditorClient::enableSingleAndLockStickyStates(bool enable)
{
    m_stickyState.enableSingleAndLockStates(enable);
}

String EditorClient::getPreviousWord()
{
	String word = "";

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();

	VisibleSelection sel = frame->selection()->selection();

	if (!sel.isCaret()) {
		return word;
	}

	VisiblePosition endWordPos = sel.visibleStart();

	VisiblePosition startWordPos = startOfWord(endWordPos, LeftWordIfOnBoundary);
	UChar leftChar =  startWordPos.characterBefore();

	UChar lastChar = endWordPos.characterBefore();

	// allow one move to the left at the end of word if we're not at last char

	if (Nova::Utils::isBreakChar(lastChar) || Nova::Utils::isTerminalPunctuation(lastChar)) {
		endWordPos = endWordPos.previous(true);
	}


	// back up to ATOM_TRUE word boundary, i.e. a whitespace or beginning of paragraph
	while (leftChar != 0 && leftChar != '\x0a' && leftChar != ' ' && leftChar != '\x0d'
			&& leftChar != '\x09' && leftChar != 160)
	{
		startWordPos = startWordPos.previous(true);
		leftChar = startWordPos.characterBefore();
		// printf("%s: walking left, at char '%c'\n", __FUNCTION__, leftChar);
	}

	ExceptionCode ec;

	word = Range::create(frame->document(), startWordPos.deepEquivalent(),
			endWordPos.deepEquivalent())->toString(ec);

	//printf("%s: alternate candidate: '%s'\n", __FUNCTION__, word.utf8().data());

	return word;
}

bool EditorClient::isObjectElement(const Node* node)
{
	if (node == NULL)
		return false;

	return node->hasTagName(HTMLNames::objectTag) || node->hasTagName(HTMLNames::embedTag);
}

bool EditorClient::isFocusableElement(const Node* node)
{
    if (node == NULL)
        return false;

    // input elements are always focusable
    if (node->isElementNode() && static_cast<const Element*>(node)->isTextFormControl())
        return true;

    // anything which flags itself as contenteditable (like div's) are allowed to be focusable
    return node->isContentEditable();
}

bool EditorClient::canAutoCorrectSpellingErrors() {

	return getStateForFocusedNode() & Palm::EDITOR_STATE_SPELL_CHECK_AUTO_CORRECT;
}

bool EditorClient::canDoStickyState()
{
	// TODO 
	return true;
}

void EditorClient::setEditable(bool editable)
{
	// FIXME?
}

void EditorClient::selectingForCopy(bool on) 
{
    COPY_TRACE("%s: turning selecting for copy: %s\n", __FUNCTION__,
            (on) ? "ON" : "OFF");
    
    if (on && !m_selectionForCopy.selecting ) {
        Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
        if (frame && !frame->selection()->selection().isRange()) {
            // we need to pick pivot word
            // this will be done on next selection change event
            m_selectionForCopy.choosingPivot = true;
        }
    } else if (!on) {
        m_selectionForCopy.choosingPivot = false;
    }

    m_selectionForCopy.selecting = on;

}

void EditorClient::selectForCopyPivotWordBoundary()
{
    if (!editorEventsAllowed() 
            || !m_selectionForCopy.selecting 
            || !m_selectionForCopy.choosingPivot) {
        return;
    }
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode()) {
        return;
    }
    
    COPY_TRACE("%s: should jump to left of word\n", __FUNCTION__);
    
    disallowEditorEvents();
    frame->selection()->modify(SelectionController::AlterationMove,
            SelectionController::DirectionLeft, m_selectionForCopy.horizontalGranularity, true);
    allowEditorEvents();
    
}

void EditorClient::selectForCopyPivotWord()
{
    if (!editorEventsAllowed() 
            || !m_selectionForCopy.selecting 
            || !m_selectionForCopy.choosingPivot) {
        return;
    }
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode()) {
        return;
    }
    
    VisibleSelection sel = frame->selection()->selection();

    if (sel.isNone()) {
        return;
    }

    ExceptionCode ec;

    RefPtr<Range> toRange = sel.firstRange();

    if (!toRange) {
        return;
    }
    int cursorPos = toRange->startOffset(ec);

    
    COPY_TRACE("%s: toRange before word selection: from: %d, to: %d, cursorPos: %d\n",
            __FUNCTION__, toRange->startOffset(ec), toRange->endOffset(ec), cursorPos);

    // assume we're in the middle of the word. move cursor left by word.
    // move cursor while extending selection right by word
    disallowEditorEvents();
    
    frame->selection()->modify(SelectionController::AlterationMove,
            SelectionController::DirectionLeft, m_selectionForCopy.horizontalGranularity, true);
    frame->selection()->modify(SelectionController::AlterationExtend,
            SelectionController::DirectionRight, m_selectionForCopy.horizontalGranularity, true);
    
    allowEditorEvents();

    // test if we weren't in the middle of a word, but at the first char
    // avoid traversing nodes to left by moving around selection
    VisibleSelection selLeftward = frame->selection()->selection();
    
    if (!selLeftward.isNone()) 
    {
        
        toRange = selLeftward.firstRange().get();
        COPY_TRACE("%s: toRange AFTER DirectionLeftWARD word selection: from: %d, to: %d\n",
                __FUNCTION__, toRange->startOffset(ec), toRange->endOffset(ec));

      
        // save current torange as word range

        expandSelectionToPunctuation(SelectionController::AlterationExtend);

        frame->setMark(frame->selection()->selection());

        m_selectionForCopy.choosingPivot = false;
        
        toRange = frame->mark().firstRange().get();
        COPY_TRACE("%s: SET MARK!!! ITS toRange: startOffset: %d, endOffset: %d\n",
                __FUNCTION__, toRange->startOffset(ec), toRange->endOffset(ec));
    } 
    
    m_stickyState.reset();
}

/**
 * Get default editor state based on global settings (not looking at focused
 * element being edited).
 */
unsigned int EditorClient::getDefaultEditorState() const
{
	unsigned int state = Palm::EDITOR_STATE_IME;

	switch (PalmBrowserSettings()->checkSpelling) {
		case WebKitPalmSettings::DISABLED:
			break;
		case WebKitPalmSettings::UNDERLINE:
			BITSET(state, Palm::EDITOR_STATE_SPELL_CHECK_UNDERLINE);
			break;
		case WebKitPalmSettings::AUTO_CORRECT:
			BITSET(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
			break;
	}
	
	if (PalmBrowserSettings()->checkGrammar)
		BITSET(state, Palm::EDITOR_STATE_AUTOCAP_SENTENCE);
	
	if (PalmBrowserSettings()->shortcutChecking)
		BITSET(state, Palm::EDITOR_STATE_AUTO_SUBSTITUTE);

	return state;
}

/**
 * Should this element (regardless of global settings or element attributes)
 * always have STE (AKA Text Assist) disabled?
 */
bool EditorClient::shouldElementAlwaysHaveSteDisabled(const Element* element)
{
	// if element in focus is an object, like the browser adapter, turn off ste
	if (isObjectElement(element))
		return true;

	// Always disable for password fields.
	if( equalIgnoringCase( element->getAttribute(HTMLNames::typeAttr), "password" ) ||
        equalIgnoringCase( element->getAttribute(VK_ATTR_INPUT_TYPE), "password") )
		return true;

	// If Element attribute is set to disable all STE features.
	if (element->getAttribute(STE_TAG_DISABLE_ALL) == ATOM_TRUE)
		return true;

	return false;
}

bool EditorClient::shouldDisableWordCompletions(const Element* element)
{
    if (!element)
        return true;

    // always disable word completions inside browser pages?
    if (PalmBrowserSettings()->runningInBrowserServer)
        return true;

    // disable word completions for password fields
    if (equalIgnoringCase(element->getAttribute(VK_ATTR_INPUT_TYPE), "password"))
        return true;

    return true;//element->getAttribute(VK_ATTR_DISABLE_WORDCOMPLETIONS) == ATOM_DISABLED;
}

/**
 * Does the attribute value match a "true" value?
 */
bool EditorClient::isValueTrue(const AtomicString& attrValue)
{
	return equalIgnoringCase(attrValue, "true") || equalIgnoringCase(attrValue, "on");
}

/**
 * Does the attribute value match a "false" value?
 */
bool EditorClient::isValueFalse(const AtomicString& attrValue)
{
	return equalIgnoringCase(attrValue, "false") || equalIgnoringCase(attrValue, "off");
}

bool EditorClient::attrValTrue(const Element* element, const AtomicString& attrName)
{
	return element != NULL && isValueTrue(element->getAttribute(attrName));
}

bool EditorClient::attrValFalse(const Element* element, const AtomicString& attrName)
{
	return element != NULL && isValueFalse(element->getAttribute(attrName));
}

/**
 * Return the effective editor state for the element being edited.
 */
unsigned int EditorClient::getStateForElement(Element* element)
{
	if (element == NULL)
		return 0x0;

	// Check for the private framework attribute, If present remove and parse it,
	// replacing with the public attributes.
	if (element->hasAttribute(STE_MODE_PRIMARY)) {
		//this element has not been parsed for STE 
		EditorClient::setSTEAttributesFromModestring(element);
		//remove the attribute so it won't be reparsed
		ExceptionCode ec;
		element->removeAttribute(STE_MODE_PRIMARY,ec);
	}

    unsigned int state = 0x0;
    bool steAlwaysDisabled = shouldElementAlwaysHaveSteDisabled(element);
    if (!steAlwaysDisabled)
        state = getDefaultEditorState();

    if (m_stickyState.singleAndLockStatesEnabled()) {
        const AtomicString& teAttr = element->getAttribute(STE_ATTR_TEXT_ENTRY);
        if( teAttr == "number" || teAttr == "num-lock" ) {
            BITSET(state, Palm::EDITOR_STATE_TEXT_ENTRY_OPT_LOCK);
            BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
        }
        else if( teAttr == "shift" || teAttr == "shift-lock" )
            BITSET(state, Palm::EDITOR_STATE_TEXT_ENTRY_SHIFT_LOCK);
        else if( teAttr == "num-single" )
            BITSET(state, Palm::EDITOR_STATE_TEXT_ENTRY_OPT);
        else if( teAttr == "shift-single" )
            BITSET(state, Palm::EDITOR_STATE_TEXT_ENTRY_SHIFT);
    }

    if (steAlwaysDisabled)
        return state;

	const WTF::String strAutoCorrectName("autocorrect");
	const AtomicString& strAutoCorrectVal = element->getAttribute(strAutoCorrectName);
	const bool autoCorrectExplicitlyEnabled = isValueTrue(strAutoCorrectVal);
	const bool autoCorrectExplicitlyDisabled = isValueFalse(strAutoCorrectVal);

	const WTF::AtomicString& strSpellCheckVal = element->getAttribute(HTMLNames::spellcheckAttr);
	const bool spellCheckExplicitlyEnabled = isValueTrue(strSpellCheckVal);
    const bool spellCheckExplicitlyDisabled = isValueFalse(strSpellCheckVal);

	// Spell checking
	{
		if (autoCorrectExplicitlyEnabled) {
			BITSET(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
		}
		else if (spellCheckExplicitlyDisabled) {
			BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
		}
		else {
			// Check the Palm attributes
			const AtomicString& ste_spellCheck = element->getAttribute(STE_ATTR_SPELL_CHECK);
			if (ste_spellCheck == ATOM_UNDERLINE) {
				BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
				BITSET(state, Palm::EDITOR_STATE_SPELL_CHECK_UNDERLINE);
			}
			else if (ste_spellCheck == ATOM_AUTO_CORRECT)
				BITSET(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
			else if (ste_spellCheck == ATOM_DISABLED)
				BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
		}

		if (autoCorrectExplicitlyDisabled) {
			BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_AUTO_CORRECT);
		}
	}

	// Sentence auto-capitalization
	const WTF::String strAutoCapName("autocapitalize");
	const AtomicString& strAutoCapVal = element->getAttribute(strAutoCapName);
	const bool autoCapExplicitlyEnabled = isValueTrue(strAutoCapVal);
	const bool autoCapExplicitlyDisabled = isValueFalse(strAutoCapVal);
	{
		if (autoCapExplicitlyEnabled) {
			BITSET(state, Palm::EDITOR_STATE_AUTOCAP_SENTENCE);
		}
		else if (autoCapExplicitlyDisabled) {
			BITCLEAR(state, Palm::EDITOR_STATE_AUTOCAP_SENTENCE);
		}
		else if (attrValTrue(element, STE_TAG_DISABLE_AUTOCAP)) {	// The Palm value
			BITCLEAR(state, Palm::EDITOR_STATE_AUTOCAP_SENTENCE);
		}
	}

	// Title auto-capitalization
	{
		if (autoCapExplicitlyDisabled) {
			BITCLEAR(state, Palm::EDITOR_STATE_AUTOCAP_TITLE);
		}
		else {
			// The Palm values
			if (attrValTrue(element, STE_TAG_MODE_AUTOCAP_TITLE)) {
				BITSET(state, Palm::EDITOR_STATE_AUTOCAP_TITLE);
			}
			else if (attrValFalse(element, STE_TAG_MODE_AUTOCAP_TITLE)) {
				BITCLEAR(state, Palm::EDITOR_STATE_AUTOCAP_TITLE);
			}
		}
	}

	// x-palm-disable-auto-replace is the old term that disables both spelling
	// auto-correction as well as user auto-substitution.
	if (attrValTrue(element, STE_TAG_DISABLE_AUTOREPLACE)) {
		BITCLEAR(state, Palm::EDITOR_STATE_SPELL_CHECK_AUTO_CORRECT);
		BITCLEAR(state, Palm::EDITOR_STATE_AUTO_SUBSTITUTE);
	}

	if (attrValTrue(element, STE_TAG_ENABLE_LINKER)) {
		BITSET(state, Palm::EDITOR_STATE_LINKER);
	}
	else if (attrValFalse(element, STE_TAG_ENABLE_LINKER)) {
		BITCLEAR(state, Palm::EDITOR_STATE_LINKER);
	}
	
	if (attrValTrue(element, STE_TAG_ENABLE_EMOTICONS)) {
		BITSET(state, Palm::EDITOR_STATE_EMOTICONS);
	}
	else if (attrValFalse(element, STE_TAG_ENABLE_EMOTICONS)) {
		BITCLEAR(state, Palm::EDITOR_STATE_EMOTICONS);
	}

	if (attrValTrue(element, STE_TAG_DISABLE_AUTOSUB)) {
		BITCLEAR(state, Palm::EDITOR_STATE_AUTO_SUBSTITUTE);
	}

	if (attrValTrue(element, STE_ATTR_DISABLE_IME)) {
		BITCLEAR(state, Palm::EDITOR_STATE_IME);
	}

	// And finally because checking the ID's and names for exclusions
	// (which requires regular expression matches) is so slow we do this last and 
	// only if necessary.
	
	const unsigned int flagsToClear = Palm::EDITOR_STATE_LINKER |
		Palm::EDITOR_STATE_EMOTICONS | 
		Palm::EDITOR_STATE_AUTO_SUBSTITUTE | 
		Palm::EDITOR_STATE_AUTOCAP_SENTENCE | 
		Palm::EDITOR_STATE_AUTOCAP_TITLE | 
		Palm::EDITOR_STATE_SPELL_CHECK_FLAGS;
	const bool hasAttribToDisableByCommonTag = state & flagsToClear;
	if (hasAttribToDisableByCommonTag &&
		PalmBrowserSettings()->enableNoSteCommonTagsCheck &&
		noSteTagList.matchCommonTags(element)) {

		// This means the element has either an ID or name that matches a value
		// in our config file that when matched means don't change the text
		// just leave it as is.
		BITCLEAR(state, flagsToClear);
	}

	return state;
}

/**
 * Get the editor text assist state for the currently focused node (if there is one).
 */
unsigned int EditorClient::getStateForFocusedNode()
{
	const Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
		return 0x0;
	
	Node* node = frame->document()->focusedNode();
	if (node == NULL || !node->isElementNode()) {
        return 0x0;
    }

	return getStateForElement(static_cast<Element*>(node));
}

/**
 * Parse the Framework's mode string and convert it to the public element attributes.
 */
void EditorClient::setSTEAttributesFromModestring(Element * element)
{
	if (element == NULL)
		return;
	
	const AtomicString& s = element->getAttribute( STE_MODE_PRIMARY );
	
	if (s.length() == 0)
		return;				//this will avoid disturbing any of the specific tags in case no x-palm-ste-mode string was specified
							//(this is for legacy compatibility, so that specifying the tags manually/separately doesn't get overwritten)
	
	String base = s.string().stripWhiteSpace();
	//Vector<String> split(const String& separator, bool allowEmptyEntries = false) const;
	WTF::Vector<String> mtags;
	base.split(String(" "), mtags);
	
	ExceptionCode ec;
	
	element->setAttribute(STE_TAG_DISABLE_ALL,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_DISABLE_AUTOCAP,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_DISABLE_AUTOREPLACE,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_DISABLE_AUTOSUB,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_MODE_AUTOCAP_TITLE,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_ENABLE_EMOTICONS,ATOM_FALSE,ec);
	element->setAttribute(STE_TAG_ENABLE_LINKER, ATOM_FALSE, ec);
	
	for (WTF::Vector<String>::iterator it = mtags.begin();it != mtags.end();it++) 
	{
		if (*it == STE_MODE_ATTR_EMOTICON_ON) {
			element->setAttribute(STE_TAG_ENABLE_EMOTICONS,ATOM_TRUE,ec);
		}
		else if (*it == STE_MODE_ATTR_EMOTICON_OFF) {
            element->setAttribute(STE_TAG_ENABLE_EMOTICONS,ATOM_FALSE,ec);
		}
		else if (*it == STE_MODE_ATTR_AC_LOWER) {
			element->setAttribute(STE_TAG_DISABLE_AUTOCAP,ATOM_TRUE,ec);
		}
		else if (*it == STE_MODE_ATTR_AC_TITLE) {
			element->setAttribute(STE_TAG_DISABLE_AUTOCAP,ATOM_FALSE,ec);
			element->setAttribute(STE_TAG_MODE_AUTOCAP_TITLE,ATOM_TRUE,ec);
		}
		else if (*it == STE_MODE_ATTR_AC_SENTENCE) {
			element->setAttribute(STE_TAG_DISABLE_AUTOCAP,ATOM_FALSE,ec);
			element->setAttribute(STE_TAG_MODE_AUTOCAP_TITLE,ATOM_FALSE,ec);
		}
		else if (*it == STE_MODE_ATTR_AR_ON) {
			element->setAttribute(STE_ATTR_SPELL_CHECK,ATOM_AUTO_CORRECT,ec);
		}
		else if (*it == STE_MODE_ATTR_AR_OFF) {
			element->setAttribute(STE_TAG_DISABLE_AUTOREPLACE,ATOM_TRUE,ec);
			// Before we had underlining and a spell check UI disabling autoreplace meant
			// disabling spell checking. Now that we can underline we should really map the
			// old autoReplace=false to just disabling auto-correct, but this probably doesn't
			// give the app developer what they want so we're now going to map it to disabling
			// all spell checking.
			element->setAttribute(STE_ATTR_SPELL_CHECK,ATOM_DISABLED,ec);

			// x-palm-disable-auto-sub is new to Blowfish and most users that disable
			// auto-replace (for spelling) probably want auto-sub turned off too.
			element->setAttribute(STE_TAG_DISABLE_AUTOSUB,ATOM_TRUE,ec);
		} 
		else if (*it == STE_MODE_ATTR_LINKER_ON) {
			element->setAttribute(STE_TAG_ENABLE_LINKER,ATOM_TRUE,ec);
		}
		else if (*it == STE_MODE_ATTR_LINKER_OFF) {
			element->setAttribute(STE_TAG_ENABLE_LINKER,ATOM_FALSE,ec);			
		}
	}
			
}

/**
 * Expands selection from pivot word to tap position. Places cursor at tap
 * location by doing back/forward cursor movement if there is space on the 
 * to the left or right of selection.
 * 
 */
void EditorClient::changeSelectionToWordBoundary()
{
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode()) {
        return;
    }
    
    VisibleSelection sel = frame->selection()->selection();
    if (sel.isNone()) {
        return;
    }
 
    // we are to the right of pivot if a move to the left contracts the selection
    
    disallowEditorEvents();

    SelectionForCopy::Direction direction = comparePositionToMark(sel);
    
    bool tapInCenter = false;
    
    if (direction == SelectionForCopy::CENTER) {
        tapInCenter = true;
        direction = directionOfSelectionExpansion(frame->mark(), sel);
    }
    
    if (direction == SelectionForCopy::RIGHT)
    {
        COPY_TRACE("AlterationMoveD TO DirectionRight!\n");
        
        UChar nextChar = sel.visibleEnd().characterAfter();
        
        bool atEndOfLine = isEndOfLine(sel.visibleEnd());
        
        if (atEndOfLine) {
            COPY_TRACE("%s: at end of line >>>>>>>>>>>>>>\n", __FUNCTION__);
        }
        if (m_selectionForCopy.horizontalGranularity == ParagraphBoundary) {
            frame->selection()->modify(SelectionController::AlterationMove,
                    SelectionController::DirectionRight, ParagraphBoundary, true);

        } else if (!atEndOfLine && (!Nova::Utils::isTerminalPunctuation(nextChar) || nextChar != ' ')) {
            frame->selection()->modify(SelectionController::AlterationMove,
                    SelectionController::DirectionRight, m_selectionForCopy.horizontalGranularity, true);
        } 

        expandSelectionToPunctuation(SelectionController::AlterationMove);
      
        selectFromCursorToMark(direction, frame->selection()->selection());
        
        // doing a quickie right and left to position cursor on right boundary of selection
        frame->selection()->modify(SelectionController::AlterationExtend,
                SelectionController::DirectionRight, CharacterGranularity, true);
        
        // we have to move back one char, unless cursor is at the last char and
        // there is no editable node immediately to our right
        sel = frame->selection()->selection();
        if (!sel.isNone()) 
        {
            int cursorPos = sel.end().computeOffsetInContainerNode();
            // move back once only if we were not at the end of the text leaf
            // if at end of text leaf, move back if we moved into the next leaf
            if (cursorPos != (int)sel.end().node()->textContent().length() 
                    || (sel.start().node()->nextSibling() 
                            && sel.start().node()->nextSibling()->isContentEditable())) {
                frame->selection()->modify(SelectionController::AlterationExtend,
                        SelectionController::DirectionLeft, CharacterGranularity, true);
            }
        }
        
       
    } else if (direction == SelectionForCopy::LEFT
            || direction == SelectionForCopy::CENTER) 
    {
        COPY_TRACE("AlterationMoveD TO DirectionLeft or CENTER!\n");

        int cursorPos = sel.start().computeOffsetInContainerNode();

        if (cursorPos > 0) 
        {
            UChar prevChar = frame->selection()->selection().visibleStart().characterBefore();
            COPY_TRACE("character before highlight: '%c'\n", prevChar);

            bool atStartOfLine = isStartOfLine(sel.visibleStart());
            if (atStartOfLine) {
                COPY_TRACE("%s: at start of line <<<<<<<<<<\n", __FUNCTION__);
            }
            
            if (m_selectionForCopy.horizontalGranularity == ParagraphBoundary) {
                // we always move back one paragraph when granularity is ParagraphBoundary
                frame->selection()->modify(SelectionController::AlterationMove,
                        SelectionController::DirectionLeft, ParagraphBoundary, true);
                
            } else if (!atStartOfLine && prevChar != ' ') {
                // move beyond original selection to word boundary on left
                frame->selection()->modify(SelectionController::AlterationMove,
                        SelectionController::DirectionLeft, m_selectionForCopy.horizontalGranularity, true);
            }
        }
        
        
        // make selection to mark
        selectFromCursorToMark(direction, frame->selection()->selection());

        // doing a quickie left and right to position cursor on left boundary of selection
        // if we're at the very beginning of a text field
        frame->selection()->modify(SelectionController::AlterationExtend,
                    SelectionController::DirectionLeft, CharacterGranularity, true);
        
        // only move right if something selectable was to our left
        
        sel = frame->selection()->selection();
        if (!sel.isNone()) 
        {
            cursorPos = sel.start().computeOffsetInContainerNode();
            if (cursorPos > 0 
                    || (sel.start().node()->previousSibling() 
                            && sel.start().node()->previousSibling()->isContentEditable())) {
                frame->selection()->modify(SelectionController::AlterationExtend,
                        SelectionController::DirectionRight, CharacterGranularity, true);
            }
        }
        
    }
    
    allowEditorEvents();    
        
    m_stickyState.reset();
}

/**
 * Runs TextLinker at cursor position. Looks at text to left and right,
 * submits candidate to TextIndexer. Creates and destroys links in rich
 * text field structure.
 * 
 */
void EditorClient::runTextLinkerAtCursor()
{
    if (m_runningTextLinker) {
        COPY_TRACE("%s: not calling linker because it is making this change!\n", __FUNCTION__);
        return;
    }
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->editor()) {
        return;
    }
    
    // only run linker on richly-editable text 
    if (!frame->selection()->isContentRichlyEditable()) {
        return;
    }

    VisibleSelection sel = frame->selection()->selection();

    if (sel.isNone()) {
        return;
    }

    Node* node = frame->document()->focusedNode();

    if (node && (node->nodeType() == Node::TEXT_NODE || node->nodeType() == Node::ELEMENT_NODE))
    {
        Element* element = static_cast<Element*>(node);
        
        // verify that we've checked for linker on/off attribute - x-palm-enable-linker
        if (element->hasAttribute(STE_MODE_PRIMARY)) {
            //this element has not been parsed for STE 
            EditorClient::setSTEAttributesFromModestring(element);
            //remove the attribute so it won't be reparsed
            ExceptionCode ec;
            element->removeAttribute(STE_MODE_PRIMARY,ec);
        }
        if (element->getAttribute(STE_TAG_ENABLE_LINKER) == ATOM_TRUE) {
            m_runningTextLinker = true;
            disallowEditorEvents();
            TextLinker::findLinksInText(sel, frame->editor());
            allowEditorEvents();
            m_runningTextLinker = false;
        }
    }
}

void EditorClient::checkWordCompletion()
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !editorEventsAllowed()) {
        return;
    }

    VisibleSelection sel = frame->selection()->selection();
    if (!sel.isContentEditable() || !sel.isCaret() || frame->selection()->isInPasswordField()) {
        return;
    }

    Node* node = frame->document()->focusedNode();
    if (node && node->isElementNode()) {

        // completely suppress word completions for the currently focused element
        if (shouldDisableWordCompletions(static_cast<const Element*>(node)))
            return;

   		// ignore whitespace
   		if (isWhitespaceToLeft())
 			return;

   		VisiblePosition end = sel.visibleStart();
   		// ignore word completions if the user is editing in the middle of a word
   		if (!Nova::Utils::isWordBreakChar(end.characterAfter()))
   			return;

   		VisiblePosition start = startOfWord(end, LeftWordIfOnBoundary);
   		ExceptionCode ec;
  		WTF::String prefix = Range::create(frame->document(), start.deepEquivalent(), end.deepEquivalent())->toString(ec);

   		if (prefix.length() >= PalmBrowserSettings()->wordCompletionStartLength) {
   			m_spellChecker->getCompletionForWord(prefix, frame, node, end);
   		}
    }
}

void EditorClient::clearWordCompletion()
{
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }

    PassRefPtr<Range> range = frame->selection()->selection().firstRange();
    Node* searchNode = range.get() ? range.get()->startContainer() : 0;
    PassRefPtr<Range> existingRange = frame->document()->markers()->findRangeForWordCompletionMarker(searchNode);

	// remove completion + markers
	if (existingRange.get()) {
		disallowEditorEvents();
		VisibleSelection oldSel = frame->selection()->selection();
		frame->selection()->setSelection(VisibleSelection(existingRange.get()));
		frame->editor()->command("Delete").execute();
		frame->selection()->setSelection(oldSel);
		allowEditorEvents();
		frame->document()->markers()->removeMarkers(DocumentMarker::WordCompletion);
	}

	m_hasCompleteWord = false;
}

void EditorClient::replaceWordCompletion(const WTF::String& completion)
{
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }

    disallowEditorEvents();

    PassRefPtr<Range> range = frame->selection()->selection().firstRange();
    Node* searchNode = range.get() ? range.get()->startContainer() : 0;
    PassRefPtr<Range> existingRange = frame->document()->markers()->findRangeForWordCompletionMarker(searchNode);

    // remove completion + markers
    if (existingRange.get()) {
    	frame->selection()->setSelection(VisibleSelection(existingRange.get()));
    	frame->editor()->command("Delete").execute();
    }

    m_hasCompleteWord = completion.isEmpty();

    if (completion.isEmpty()) {
    	frame->document()->markers()->removeMarkers(DocumentMarker::WordCompletion);
    }
    else {
		// insert completion and restore selection position just after the prefix
    	VisibleSelection start = frame->selection()->selection();
    	frame->editor()->command("InsertText").execute(completion);
    	VisibleSelection end = frame->selection()->selection();
    	frame->selection()->setSelection(start);

    	PassRefPtr<Range> compRange = Range::create(frame->document(), start.visibleStart().deepEquivalent(), end.visibleStart().deepEquivalent());
    	frame->document()->markers()->addMarker(compRange.get(), DocumentMarker::WordCompletion);
    }

	allowEditorEvents();
}

bool EditorClient::pointInWordCompletion(const IntPoint& viewPt)
{
    bool ptInCompletion = false;
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (frame) {

        VisibleSelection sel = frame->selection()->selection();
        PassRefPtr<Range> range = sel.firstRange();
        Node* searchNode = range.get() ? range.get()->startContainer() : 0;
        PassRefPtr<Range> existingRange = frame->document()->markers()->findRangeForWordCompletionMarker(searchNode);

        if (existingRange.get()) {
            VisiblePosition prefixEnd = sel.visibleStart();
            VisiblePosition prefixStart = startOfWord(prefixEnd, LeftWordIfOnBoundary);
            PassRefPtr<Range> completeRange = Range::create(frame->document(), prefixStart.deepEquivalent(), existingRange.get()->endPosition());
            Vector<IntRect> rects;
            completeRange->textRects(rects, true);
            const IntPoint contentPt(frame->view()->windowToContents(viewPt));
            for (size_t i=0; i<rects.size(); i++) {
                if (rects[i].contains(contentPt)) {
                    ptInCompletion = true;
                    break;
                }
            }
        }
    }
    return ptInCompletion;
}

bool EditorClient::commitWordCompletion()
{
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame) {
		return false;
	}

	disallowEditorEvents();

    SelectionController* selController = frame->selection();
    VisibleSelection sel = selController->selection();
    PassRefPtr<Range> range = sel.firstRange();
    Node* searchNode = range.get() ? range.get()->startContainer() : 0;
    PassRefPtr<Range> existingRange = frame->document()->markers()->findRangeForWordCompletionMarker(searchNode);

    if (existingRange.get()) {

		if (m_hasCompleteWord) {

    		selController->moveTo(existingRange.get()->endPosition(), DOWNSTREAM);

			// add acceptance space
			frame->editor()->command("InsertText").execute(" ");
		}
		else {
            // perform an auto replace of the prefix with the prefix+completion to support
            // undo on backspace
            ExceptionCode ec;

            // save prefix and complete word
            VisiblePosition prefixEnd = sel.visibleStart();
            VisiblePosition prefixStart = startOfWord(prefixEnd, LeftWordIfOnBoundary);
            VisiblePosition completionEnd = existingRange.get()->endPosition();

            PassRefPtr<Range> prefixRange = Range::create(frame->document(), prefixStart.deepEquivalent(), prefixEnd.deepEquivalent());
            PassRefPtr<Range> completeRange = Range::create(frame->document(), prefixStart.deepEquivalent(), existingRange.get()->endPosition());
            WTF::String prefix = prefixRange.get()->toString(ec);
            WTF::String completeWord = completeRange.get()->toString(ec);

            // select and delete completion
            selController->setSelectedRange(existingRange.get(), DOWNSTREAM, true);
            frame->editor()->command("Delete").execute();

            // select and auto replace prefix with complete word
            selController->setSelectedRange(prefixRange.get(), DOWNSTREAM, true);
            frame->editor()->autoReplaceSelectionWithText(completeWord + " ", false, false);

            // add a marker around the complete word to restore the prefix
            PassRefPtr<Range> replaceRange = Range::create(frame->document(), prefixStart.deepEquivalent(), completionEnd.deepEquivalent());
            frame->document()->markers()->addMarker(replaceRange.get(), DocumentMarker::Replacement, prefix);
        }

        frame->document()->markers()->removeMarkers(DocumentMarker::WordCompletion);
    }
/*
    if (existingRange.get() || m_hasCompleteWord) {
    	// learn the accepted completed word
    	WTF::String commitedWord = getPreviousWord();
    	g_debug("%s: learn accepted word '%s'", __PRETTY_FUNCTION__, commitedWord.utf8().data());
    	m_spellChecker->learnWord(commitedWord);
    }
*/  
    m_hasCompleteWord = false;

	allowEditorEvents();

	return existingRange.get() != 0;
}

void EditorClient::wordCompletionReady(const WTF::String& prefix, const WTF::String& completion, 
		const WebCore::VisiblePosition& pivot)
{
	EC_TRACE("<-------- %s ---- '%s' -- '%s' ---->\n", __FUNCTION__, prefix.utf8().data(), completion.utf8().data());

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame || !editorEventsAllowed() || frame->editor()->hasComposition()) {
		return;
	}

	VisibleSelection sel = frame->selection()->selection();
	if (!sel.isContentEditable() || !sel.isCaret() || frame->selection()->isInPasswordField()) {
		return;
	}

	// cursor position has changed since this request
	if (sel.visibleStart() != pivot) {
		return;
	}

	Node* node = frame->document()->focusedNode();
	if (node && node->isElementNode()) {

        if (shouldDisableWordCompletions(static_cast<Element*>(node)))
            return;

		// does the prefix still match?
		VisiblePosition end = sel.visibleStart();
		VisiblePosition start = startOfWord(end, LeftWordIfOnBoundary);
		ExceptionCode ec;
		WTF::String curPrefix = Range::create(frame->document(), start.deepEquivalent(), end.deepEquivalent())->toString(ec);
		if (curPrefix != prefix)
			return;

		replaceWordCompletion(completion);
	}
}

/**
 * A chance to modify a piece of text before it gets inserted at the current cursor position.
*/
void EditorClient::filterTextForInsertion(String& insertionText)
{
    if (insertionText.isEmpty())
        return;

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !isEditing())
        return;

    VisibleSelection sel = frame->selection()->selection();
    if (sel.isNone())
        return;

    // DFISH-3143: trim off the '.' from '.com'
    if (insertionText.length() > 1 && insertionText[0] == sel.visibleStart().characterBefore()) {
        insertionText.remove(0);
        return;
    }
}

/**
 * Creates a new selection from current selection to the start position of mark
 * 
 * 
 * 
 */
void EditorClient::selectFromCursorToMark(const SelectionForCopy::Direction direction, 
        const VisibleSelection& selection)
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }
    VisibleSelection mark = frame->mark();
    
    IntRect selStart = selection.visibleStart().absoluteCaretBounds();
    IntRect markStart = mark.visibleStart().absoluteCaretBounds();
    IntRect markEnd = mark.visibleEnd().absoluteCaretBounds();
    
    
    Position currSel = selection.start();
    Position existingSelBoundary;
    
    switch (direction) 
    {
    
    case SelectionForCopy::CENTER : 
    {


        // distance to start
        float xDistanceToStart = abs(markStart.x() - selStart.x());
        float yDistanceToStart = abs(markStart.y() - selStart.y()); 

        // distance to end
        float xDistanceToEnd = abs(markEnd.x() - selStart.x());
        float yDistanceToEnd = abs(markEnd.y() - selStart.y());


        if (yDistanceToStart == yDistanceToEnd) {
            if (xDistanceToStart > xDistanceToEnd) {
                existingSelBoundary = mark.start();
                COPY_TRACE("%s: going to selection START\n", __FUNCTION__);
            } else {
                existingSelBoundary = mark.end();
                COPY_TRACE("%s: going to selection END\n", __FUNCTION__);
            }
        } else if (yDistanceToStart > yDistanceToEnd) {
            existingSelBoundary = mark.start();
            COPY_TRACE("%s: going to selection START\n", __FUNCTION__);
        } else {
            existingSelBoundary = mark.end();
            COPY_TRACE("%s: going to selection END\n", __FUNCTION__);
        }

        break;
    }
    
    case SelectionForCopy::LEFT :
    {
        if (!m_trackballEnabled && selection.visibleStart() == mark.visibleStart()) {
            COPY_TRACE("%s: we're in a DirectionLeft BOUNDARY UNIT!\n", __FUNCTION__);
            // modify start by one paragraph to the left
            VisiblePosition paragraphPos = endOfParagraph(selection.visibleStart());
            if (paragraphPos.isNotNull()) {
                currSel = paragraphPos.deepEquivalent();
            }
        }
        
        existingSelBoundary = mark.end();
        break;
    }
    
    case SelectionForCopy::RIGHT :
    {
        if (!m_trackballEnabled && selection.visibleStart() == mark.visibleEnd()) {
            COPY_TRACE("%s: we're in a DirectionRight BOUNDARY UNIT!\n", __FUNCTION__);
            
            // modify start by one paragraph to the right
            VisiblePosition paragraphPos = startOfParagraph(selection.visibleStart());
            if (paragraphPos.isNotNull()) {
                currSel = paragraphPos.deepEquivalent();
            }
        }
        
        existingSelBoundary = mark.start();
        
        break;
    }
    
    }

    // create a selection from the existing one, to the start of the new one
    VisibleSelection newSel(existingSelBoundary, currSel);
    
    if (newSel.isRange()) {
        frame->selection()->setSelection(newSel);
    } else {
        COPY_TRACE("%s: CLEARING SELECTION because it's a cursor\n", __FUNCTION__);
        frame->selection()->clear();
        m_selectionForCopy.choosingPivot = true;
    }
}

void EditorClient::disallowEditorEvents()
{
    m_disableEditorEventCount++;
}

void EditorClient::allowEditorEvents()
{
    m_disableEditorEventCount--;
}

bool EditorClient::editorEventsAllowed() 
{
    if (m_disableEditorEventCount > 0) {
        COPY_TRACE("rejecting editor event\n");
    }
    return (m_disableEditorEventCount == 0);
}

/**
 * Keep current cursor position in viewport in browser by getting coordinates
 * of cursor and sending them to browser adapter. BA knows about the viewport
 * size and scroll position, so it determines if the webview should scroll.
 * 
 */
bool EditorClient::keepSelectionInViewport(const VisibleSelection& selection)
{
    if (!PalmBrowserSettings()->runningInBrowserServer) {
        return false;
    }
    
    if (!m_selectionForCopy.selecting 
            || m_selectionForCopy.directionFromPivot == SelectionForCopy::UNDEFINED
            || selection.isNone()) {
        return false;
    }

    switch (comparePositionToPivot(selection))
    {
        case SelectionForCopy::RIGHT :
        {
            // want visible start of range, since we're expanding on its right side
            COPY_TRACE("%s: DirectionRight SIDE OF SELECTION\n", __FUNCTION__);
            
            Palm::WebView* view = m_page->view();
            if (view && view->client()) {
                IntRect cursorRect = selection.visibleEnd().absoluteCaretBounds();
                view->client()->makePointVisible(cursorRect.x(), cursorRect.y());
            }
            
            break;
        }

        case SelectionForCopy::LEFT :
        {
            COPY_TRACE("%s: DirectionLeft SIDE OF SELECTION:\n", __FUNCTION__);
            
            Palm::WebView* view = m_page->view();
            if (view && view->client()) {
                IntRect cursorRect = selection.visibleStart().absoluteCaretBounds();
                view->client()->makePointVisible(cursorRect.x(), cursorRect.y());
            }

            break;        
        }
        
        case SelectionForCopy::CENTER :
        {
            COPY_TRACE("%s: AT PIVOT, so do nothing\n", __FUNCTION__);
            
            break;
        }
        
        case SelectionForCopy::UNDEFINED :
        {
            COPY_TRACE("%s: can't get direction of selection wrt to the pivot\n",
                    __FUNCTION__);
            break;
        }

    }

	return true;
}

/**
 * Determines if given selection is to the left or right of a selection "pivot".
 * A pivot is the first word that was highlighted when selection began. It is 
 * saved as a frame mark, and to this mark the given selection is
 * compared.
 * 
 */
SelectionForCopy::Direction EditorClient::comparePositionToPivot(const VisibleSelection& selection) 
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return SelectionForCopy::UNDEFINED;
    }

    VisibleSelection mark = frame->mark();
    
    if (mark.isNone()) {
        return SelectionForCopy::UNDEFINED;
    }

    SelectionForCopy::Direction direction = SelectionForCopy::UNDEFINED;
    
    IntRect selStart = selection.visibleStart().absoluteCaretBounds();
    IntRect markStart = mark.visibleStart().absoluteCaretBounds();
    
    if (selStart.x() == markStart.x() && selStart.y() == markStart.y()) {
        direction = SelectionForCopy::RIGHT;
    } else {
        direction = SelectionForCopy::LEFT;
    }

    return direction;
}

SelectionForCopy::Direction EditorClient::comparePositionToMark(const VisibleSelection& selection)
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return SelectionForCopy::UNDEFINED;
    }

    VisibleSelection mark = frame->mark();
    if (mark.isNone()) {
        return SelectionForCopy::UNDEFINED;
    }
    
    
    IntRect selStart = selection.visibleStart().absoluteCaretBounds();
    IntRect markStart = mark.visibleStart().absoluteCaretBounds();
    IntRect markEnd = mark.visibleEnd().absoluteCaretBounds();
    
    
    SelectionForCopy::Direction direction;
    
    // check if we're on the left
    if (selStart.y() < markStart.y()
            || (selStart.y() == markStart.y() && selStart.x() <= markStart.x())) {
        
        direction = SelectionForCopy::LEFT;
        COPY_TRACE("%s: moving to DirectionLeft SIDE OF SELECTION:\n", __FUNCTION__);
        
    } else if (selStart.y() > markEnd.y() // check if we're on the right
            || (selStart.y() == markEnd.y() && selStart.x() >= markEnd.x())) {
        
        direction = SelectionForCopy::RIGHT;
        COPY_TRACE("%s: moving to DirectionRight SIDE OF SELECTION:\n", __FUNCTION__);
        
    } else {
        COPY_TRACE("%s: moving to CENTER OF SELECTION:\n", __FUNCTION__);
        direction = SelectionForCopy::CENTER;
    }
    
    return direction;
}

/**
 * Compares newSelection position to mark and decides which way we should expand
 * by our granularity.
 * 
 * 
 */
SelectionForCopy::Direction EditorClient::directionOfSelectionExpansion(const VisibleSelection& mark,
        const VisibleSelection& selection)
{
    if (mark.isNone() || selection.isNone()) {
        return SelectionForCopy::UNDEFINED;
    }
    
    IntRect selStart = selection.visibleStart().absoluteCaretBounds();
    IntRect markStart = mark.visibleStart().absoluteCaretBounds();
    IntRect markEnd = mark.visibleEnd().absoluteCaretBounds();

    // determine in which direction we're going to reconstruct selection before
    // it is expanded in the opposite direction
    
    // distance to start
    float xDistanceToStart = abs(markStart.x() - selStart.x());
    float yDistanceToStart = abs(markStart.y() - selStart.y()); 
    
    // distance to end
    float xDistanceToEnd = abs(markEnd.x() - selStart.x());
    float yDistanceToEnd = abs(markEnd.y() - selStart.y());
    
    SelectionForCopy::Direction direction =  SelectionForCopy::UNDEFINED;

    if (yDistanceToStart == yDistanceToEnd) {
        if (xDistanceToStart > xDistanceToEnd) {
            COPY_TRACE("%s: should expand to DirectionLeft\n", __FUNCTION__);
            direction = SelectionForCopy::RIGHT;        
        } else {
            COPY_TRACE("%s: should expand to DirectionRight\n", __FUNCTION__);
            direction = SelectionForCopy::LEFT;
        }
    } else if (yDistanceToStart > yDistanceToEnd) {
        COPY_TRACE("%s: should expand to DirectionRight\n", __FUNCTION__);
        direction = SelectionForCopy::RIGHT;                
        
    } else {
        COPY_TRACE("%s: should expand to DirectionLeft\n", __FUNCTION__);
        direction = SelectionForCopy::LEFT;
    }
    
    return direction;
}

void EditorClient::expandSelectionToPunctuation(SelectionController::EAlteration alteration)
{
    disallowEditorEvents();
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }
    
    UChar nextChar = frame->selection()->selection().visibleEnd().characterAfter();
    
    COPY_TRACE("%s: see next char: '%c'\n", __FUNCTION__, nextChar);
    
    if (Nova::Utils::isTerminalPunctuation(nextChar)) {
        COPY_TRACE("%s: moving forward one character cluster\n", __FUNCTION__);
        frame->selection()->modify(alteration, 
                SelectionController::DirectionForward, 
                CharacterGranularity, true); 
    }
    
    allowEditorEvents();

}

void EditorClient::expandSelectionByParagraph(SelectionController::EDirection direction) 
{
    disallowEditorEvents();
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return;
    }
    // move ahead into the next paragraph
    frame->selection()->modify(SelectionController::AlterationExtend, 
            direction, 
            CharacterGranularity, true); 

    frame->selection()->modify(SelectionController::AlterationExtend, 
            direction, 
            ParagraphBoundary, true); 
    
    allowEditorEvents();
}

void EditorClient::setTrackballEnabled(bool enabled) {
    m_trackballEnabled = enabled;
}

void EditorClient::setSelectionGranularityForDesignMode(bool inDesignMode)
{
    if (inDesignMode) {
        m_selectionForCopy.verticalGranularity = ParagraphBoundary;
        m_selectionForCopy.horizontalGranularity = ParagraphBoundary;
    } else {
        m_selectionForCopy.verticalGranularity = LineGranularity;
        m_selectionForCopy.horizontalGranularity = WordGranularity;
    }
}

bool EditorClient::allowTrackballEvent(int period) {
    static unsigned int numEvents = 0;
    
    numEvents++;
    
    return (numEvents % period == 0); 
}

String EditorClient::getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord)
{
	ASSERT(false);	// Not called for webOS.
	return String();
}

UChar EditorClient::charToLeft()
{
	UChar leftChar = 0;

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame) {
		return leftChar;
	}

	VisibleSelection sel = frame->selection()->selection();

	return (UChar)sel.visibleStart().characterBefore();
}

bool EditorClient::isWhitespaceToLeft()
{
	UChar leftChar = charToLeft();

    bool nextToSpace = (leftChar == 0
			|| leftChar == '\x0a' || leftChar == ' ' || leftChar == '\x0d'
			|| leftChar == '\x09' || leftChar == 160);

    return nextToSpace;
}


EditorClient::NoSteTagList::NoSteTagList()
	: m_lastElement(0)
	, m_lastElementMatched(false)
	, m_initialized(false)
{}

void EditorClient::NoSteTagList::init() {
	if (m_initialized)
		return;

	std::ifstream inTags(PalmBrowserSettings()->noSteCommonTagsFilePath);

	if (inTags) {
		std::string line;
		while (getline(inTags, line, '\n')) {
			if (line.empty())
				continue;

			m_tagsRegex.push_back(WebCore::RegularExpression(line.c_str(), TextCaseInsensitive));
		}
		inTags.close();
	}

	m_initialized = true;
}

bool EditorClient::NoSteTagList::search(const WTF::String& name, const WTF::String& id) {
	if (name.isEmpty() && id.isEmpty())
		return false;

	// lazy initialization
	if (!m_initialized)
		init();

	int matchLength = 0;
	for (std::vector<WebCore::RegularExpression>::iterator it = m_tagsRegex.begin(); it != m_tagsRegex.end(); ++it) {
		if (-1 != (*it).match(name, 0, &matchLength) ||
			-1 != (*it).match(id, 0, &matchLength)) {
			return true;
		}
	}

	return false;
}

bool EditorClient::NoSteTagList::matchCommonTags(const WebCore::Element* el) {

	if (m_lastElement == el)
		return m_lastElementMatched;

	m_lastElement = el;

	// we always want to spell check in text areas
	if (el->hasTagName(HTMLNames::textareaTag)) {
		m_lastElementMatched = false;
		return m_lastElementMatched;
	}

	// make sure there are no attributes that explicitly require STE
	const WTF::AtomicString& scAttr = el->getAttribute(HTMLNames::spellcheckAttr);
	if (equalIgnoringCase(scAttr, "true")) {
		m_lastElementMatched = false;
		return m_lastElementMatched;
	}

	WebCore::Element* parent = el->parentElement();
	while (parent && parent->tagName() != "FORM") {
		parent = parent->parentElement();
	}

	if (!parent) {
		m_lastElementMatched = false;
		return m_lastElementMatched;
	}

	const WTF::String& name(el->getAttribute(HTMLNames::nameAttr).string());
    const WTF::String id(el->attributeMap() ? (el->attributeMap()->idForStyleResolution().string()) : WTF::String());
	m_lastElementMatched = search(name, id);
	return m_lastElementMatched;
}


// TODO maybe we should extract label text and check that as well
// need to figure out how to extract text from inside the label
/*
bool EditorClient::NoSteTagList::hasMatchingLabel(WebCore::Element* root, WebCore::Element* el)
{
	if (root->isHTMLElement()) {
		WebCore::HTMLElement* elHtml = static_cast<HTMLElement*>(root);
		RefPtr<WebCore::HTMLCollection> children = elHtml->children();
		for (WebCore::Node* child = children->firstItem(); child; child = children->nextItem()) {

			if (child->isHTMLElement()) {
				WebCore::HTMLElement* htmlEl = static_cast<WebCore::HTMLElement*>(child);
				if (htmlEl->hasTagName("LABEL") && htmlEl->getAttribute("for") == el->getIDAttribute()) {
					match label->innerText() with our regex stuff
					return true;
				}
			}
		}
	}
}
*/

/**
 * Make the selected word uppercase.
 */
void EditorClient::uppercaseWord()
{
}

/**
 * Make the selected word lowercase.
 */
void EditorClient::lowercaseWord()
{
}

/**
 * Capitalize the selected word.
 */
void EditorClient::capitalizeWord()
{
}

void EditorClient::showSubstitutionsPanel(bool show)
{
	g_debug("showSubstitutionsPanel: %c", show ? 'Y' : 'N');
    m_substitutionsPanelVisible = show;
}

bool EditorClient::substitutionsPanelIsShowing()
{
    return m_substitutionsPanelVisible;
}

void EditorClient::toggleSmartInsertDelete()
{
    m_smartInsertDelete = !m_smartInsertDelete;;
}

bool EditorClient::isAutomaticQuoteSubstitutionEnabled()
{
    return m_automaticQuoteSubstitutionEnabled;
}

void EditorClient::toggleAutomaticQuoteSubstitution()
{
    m_automaticQuoteSubstitutionEnabled = !m_automaticQuoteSubstitutionEnabled;
}

bool EditorClient::isAutomaticLinkDetectionEnabled()
{
    return m_automaticLinkDetectionEnabled;
}

void EditorClient::toggleAutomaticLinkDetection()
{
    m_automaticLinkDetectionEnabled = !m_automaticLinkDetectionEnabled;
}

bool EditorClient::isAutomaticDashSubstitutionEnabled()
{
    return m_automaticDashSubstitutionEnabled;
}

void EditorClient::toggleAutomaticDashSubstitution()
{
    m_automaticDashSubstitutionEnabled = !m_automaticDashSubstitutionEnabled;
}

bool EditorClient::isAutomaticTextReplacementEnabled()
{
	return PalmBrowserSettings()->shortcutChecking;
}

void EditorClient::toggleAutomaticTextReplacement()
{
}

bool EditorClient::isAutomaticSpellingCorrectionEnabled()
{
    return PalmBrowserSettings()->checkSpelling == WebKitPalmSettings::AUTO_CORRECT;
}

void EditorClient::toggleAutomaticSpellingCorrection()
{
}

bool EditorClient::isAsynchronousSpellCheckingEnabled()
{
    return true;
}

#if !defined(NDEBUG)
static const char* TextCheckingTypeName(TextCheckingType type)
{
	switch (type) {
		case TextCheckingTypeSpelling: return "TextCheckingTypeSpelling";
		case TextCheckingTypeSpellPending: return "TextCheckingTypeSpellPending";
		case TextCheckingTypeSpellingCorrect: return "TextCheckingTypeSpellingCorrect";
		case TextCheckingTypeGrammar: return "TextCheckingTypeGrammar";
		case TextCheckingTypeLink: return "TextCheckingTypeLink";
		case TextCheckingTypeQuote: return "TextCheckingTypeQuote";
		case TextCheckingTypeDash: return "TextCheckingTypeDash";
		case TextCheckingTypeReplacement: return "TextCheckingTypeReplacement";
		case TextCheckingTypeCorrection: return "TextCheckingTypeCorrection";
		case 0x0: return "Nothing";
		default: return "!!! Unknown !!!";
	}
}
#endif

static String uppercaseFirstCharacter(const String& word)
{
	// FIXME: There's surely a more efficient way to do this.
	UChar firstChar = u_toupper(word[0]);
	return String(&firstChar, 1) + word.substring(1);
}

// TODO: Take this out after feature development complete.
#define SUPER_VERBOSE

/**
 * @param autoCorrectAllowed Any form of (spelling, grammar, etc.) allowed.
 */
void EditorClient::checkWord(const WTF::String& word, unsigned int steState,
		TextCheckingResult& result, bool autoCorrectAllowed, bool firstWordOfSentence, bool checkCacheOnly)
{
    result.type = static_cast<TextCheckingType> (0x0);

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

	bool shouldSpellCheckWord = steState & (Palm::EDITOR_STATE_SPELL_CHECK_FLAGS|Palm::EDITOR_STATE_AUTO_SUBSTITUTE);
    if (!result.type && shouldSpellCheckWord) {

        Palm::SpellCheck::SpellCheckWordInfo info;
        Palm::SpellCheck::SpellCheckResult response;
        if (m_spellChecker->processingResponse() || checkCacheOnly)
            response = m_spellChecker->getGuessesFromCacheOnly(word, info);
        else
            response = m_spellChecker->getGuessesForWord(word, info, frame);

        if (response == Palm::SpellCheck::SpellCheckReady) {

            if (!info.isMisspelled() && info.isEmpty()) {
                // word is correctly spelled
                result.type = TextCheckingTypeSpellingCorrect;
            } else if (!info.isEmpty()) {
                const Palm::SpellCheck::WordGuess& topGuess = info.guesses[0];
                const Palm::SpellCheck::WordGuess* autoReplaceGuess = info.getAutoReplace();
                const Palm::SpellCheck::WordGuess* autoAcceptGuess = info.getAutoAccept();

                if (autoCorrectAllowed && (steState & Palm::EDITOR_STATE_AUTO_SUBSTITUTE) && autoReplaceGuess) {
                    result.replacement = autoReplaceGuess->word;
                    result.type = TextCheckingTypeReplacement;
                } else if (autoCorrectAllowed && (steState & Palm::EDITOR_STATE_SPELL_CHECK_AUTO_CORRECT) && autoAcceptGuess) {
                    result.replacement = autoAcceptGuess->word;
                    result.type = TextCheckingTypeCorrection;
                } else if ((steState & Palm::EDITOR_STATE_SPELL_CHECK_FLAGS) && info.isMisspelled()) {
                    // Nothing automatic here, just flag as a spelling error if appropriate
                    result.replacement = topGuess.word;
                    result.type = TextCheckingTypeSpelling;
                }
            }
        } else if (response == Palm::SpellCheck::SpellCheckPending) {
            result.type = TextCheckingTypeSpellPending;
        }
    }

#if defined(SUPER_VERBOSE) && !defined(NDEBUG)
	EC_TRACE(" Word '%s' = '%s', autoCorrectAllowed=%c, canAutoCorrectSpellingErrors=%c\n",
			word.utf8().data(), TextCheckingTypeName(result.type),
			autoCorrectAllowed ? 'Y' : 'N',
			canAutoCorrectSpellingErrors() ? 'Y' : 'N');
#endif
}

/**
 * Called by the Editor to check the text of a paragraph. 
 *
 * @param text	The text to check.
 * @param length The number of characters in "text".
 * @param checkingTypes bitmask of TextCheckingType values.
 * @param results Initially empty. It's the job of this function to fill in this vector with the results of words that
 *                are not correct (for whatever reason).
 * @param caretIdx The character position of the caret. A value of zero means selection is unknown.
 */
void EditorClient::checkTextOfParagraph(const UChar* text, int length, uint64_t checkingTypes, Vector<TextCheckingResult>& results, int caretIdx)
{
	WTF::Vector<Palm::WordRange> words;
	m_spellChecker->parseParagraph(text, length, words);

	TextCheckingResult result;

	unsigned int steState = getStateForFocusedNode();

	if (!(checkingTypes & TextCheckingTypeSpelling))
		BITCLEAR(steState, Palm::EDITOR_STATE_SPELL_CHECK_FLAGS);
	
	if (!(checkingTypes & TextCheckingTypeCorrection))
		BITCLEAR(steState, Palm::EDITOR_STATE_SPELL_CHECK_AUTO_CORRECT);
	
	if (!(checkingTypes & TextCheckingTypeReplacement))
		BITCLEAR(steState, Palm::EDITOR_STATE_AUTO_SUBSTITUTE);

#if 0
	{
	String in(text, length);
	g_debug("%s: Parsing '%s' (length=%d), caretIdx=%d, service response=%d\n",
            __FUNCTION__, in.utf8().data(), length, caretIdx, m_spellChecker->processingResponse());
	}
#endif

	WTF::Vector<Palm::WordRange>::const_iterator word;
	for (word = words.begin(); word != words.end(); ++word) {
		// If text is "the" and caret is at "the|" then word will be start..end = 0..3 and caretIdx will be 4
		const bool caretInWord = caretIdx > 0 && caretIdx >= word->start && caretIdx <= (word->end);
		const bool canSpellCheckWord = !caretInWord || m_spellChecker->processingResponse();
		if (canSpellCheckWord) {

			bool autoCorrectAllowed = false;
			Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
			if (frame) {
				UChar32 previousChr = frame->selection()->selection().visibleStart().characterBefore();
				autoCorrectAllowed = caretIdx > 0 && Nova::Utils::isWordBreakChar(previousChr) &&
									(word->end+1 == caretIdx || word->end+2 == caretIdx);
                autoCorrectAllowed |= m_spellChecker->processingResponse();
			}

		    String strWord(text + word->start, word->length());
			checkWord(strWord, steState, result, autoCorrectAllowed, word->firstWordInSentence, caretIdx == 0);

			if (result.type) {
				result.location = word->start;
				result.length = word->length();
				results.append(result);
			}
		}
	}
}

void EditorClient::EditHistoryStack::push(WTF::PassRefPtr<WebCore::EditCommand> command)
{
	m_stack.append(command);
}

WTF::PassRefPtr<WebCore::EditCommand> EditorClient::EditHistoryStack::pop()
{
	if (m_stack.isEmpty()) {
		return NULL;
	}
	else {
		WTF::RefPtr<WebCore::EditCommand> command = m_stack.last();
		m_stack.removeLast();
		return command;
	}
}

void EditorClient::EditHistoryStack::clear()
{
	m_stack.clear();
}

bool EditorClient::EditHistoryStack::isEmpty() const
{
	return m_stack.isEmpty();
}

SpellingWidgetController* EditorClient::getSpellWidgetController()
{
	return m_spellingWidget;
}

void EditorClient::willSetInputMethodState()
{
	EC_TRACE("<-------- %s -------->\n", __FUNCTION__);

    m_expectsInputMethodStateCallback = true;

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame || !frame->editor()) {
		return;
	}

	// clean up composition state before focus shifts
	frame->editor()->confirmComposition();

    // clean up word completion
    clearWordCompletion();
}

}

