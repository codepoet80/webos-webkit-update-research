
#ifndef EditorClientPalm_h
#define EditorClientPalm_h

#include "EditorClient.h"
#include "EditorStickyState.h"
#include "SelectionController.h"
#include "TextGranularity.h"
#include "RegularExpression.h"

#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#include <wctype.h>
#include <stdio.h>
#include <unicode/uchar.h>
#include <string>
#include <vector>

#include "ClipboardController.h"
#include "WordCompletionController.h"

namespace WebCore {
    class Page;
    class Editor;
    class SpellingWidgetController;
    class ClipboardWidgetController;
    enum TextCheckingType;
}

namespace Nova {
    class SmartTextEngine;
}

namespace Palm {
    class WebPage;
    class SpellCheck;
}

namespace std
{
    typedef std::basic_string<UChar> ustring;
}


namespace WebKit {

struct SelectionForCopy {

    enum Direction {LEFT = 1, CENTER = 0, RIGHT = -1, UNDEFINED = -2};  // range boundary bias constants

    bool selecting;
    bool choosingPivot;
    Direction  directionFromPivot;
    WebCore::TextGranularity horizontalGranularity;
    WebCore::TextGranularity verticalGranularity;
    
    SelectionForCopy() {
        selecting = false;
        choosingPivot = false;
        directionFromPivot = UNDEFINED;
        verticalGranularity = WebCore::LineGranularity;
        horizontalGranularity = WebCore::WordGranularity; 
    }

};

    class EditorClient : public WebCore::EditorClient {
		
		public:
			EditorClient(Palm::WebPage* page);
			 ~EditorClient();
	
			static const unsigned int kTrackballSubsamplePeriod = 3;
	
			// from EditorClient
			virtual void pageDestroyed();
		
			virtual bool shouldDeleteRange(WebCore::Range*);
			virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*);
			virtual bool smartInsertDeleteEnabled(); 
			virtual bool isContinuousSpellCheckingEnabled();
			virtual void toggleContinuousSpellChecking();
			virtual bool isGrammarCheckingEnabled();
			virtual void toggleGrammarChecking();
			virtual int spellCheckerDocumentTag();
		
			virtual bool isEditable();
	
			virtual bool shouldBeginEditing(WebCore::Range*);
			virtual bool shouldEndEditing(WebCore::Range*);
			virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction);
			virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction);
			virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting);
	
			virtual bool shouldApplyStyle(WebCore::CSSStyleDeclaration*, WebCore::Range*);
	
			virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range*);
	
			virtual void didBeginEditing();
			virtual void respondToChangedContents();
			virtual void respondToChangedSelection();
			virtual void didEndEditing();
			virtual void didWriteSelectionToPasteboard();
			virtual void didSetSelectionTypesForPasteboard();
		
			virtual void registerCommandForUndo(WTF::PassRefPtr<WebCore::EditCommand>);
			virtual void registerCommandForRedo(WTF::PassRefPtr<WebCore::EditCommand>);
			virtual void clearUndoRedoOperations();
	
			virtual bool canUndo() const;
			virtual bool canRedo() const;
		
			virtual void undo();
			virtual void redo();
			
			virtual void handleKeyboardEvent(WebCore::KeyboardEvent*);
			virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*);
	
			virtual void textFieldDidBeginEditing(WebCore::Element*);
			virtual void textFieldDidEndEditing(WebCore::Element*);
			virtual void textDidChangeInTextField(WebCore::Element*);
			virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*);
			virtual void textWillBeDeletedInTextField(WebCore::Element*);
			virtual void textDidChangeInTextArea(WebCore::Element*);
			
			virtual bool isSelectTrailingWhitespaceEnabled();
			
			virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String& misspelledWord);
			virtual void ignoreWordInSpellDocument(const WTF::String&);
			virtual void learnWord(const WTF::String&);
			virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength);
			virtual void checkGrammarOfString(const UChar*, int length, WTF::Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength);
			virtual void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&);
			virtual void updateSpellingUIWithMisspelledWord(const WTF::String&);
			virtual void showSpellingUI(bool show);
			virtual bool spellingUIIsShowing();
			virtual void getGuessesForWord(const WTF::String&, WTF::Vector<WTF::String>& guesses);
			virtual void willSetInputMethodState();
            virtual void setInputMethodState(bool enabled);

            virtual void uppercaseWord();
            virtual void lowercaseWord();
            virtual void capitalizeWord();
            virtual void showSubstitutionsPanel(bool show);
            virtual bool substitutionsPanelIsShowing();
            virtual void toggleSmartInsertDelete();
            virtual bool isAutomaticQuoteSubstitutionEnabled();
            virtual void toggleAutomaticQuoteSubstitution();
            virtual bool isAutomaticLinkDetectionEnabled();
            virtual void toggleAutomaticLinkDetection();
            virtual bool isAutomaticDashSubstitutionEnabled();
            virtual void toggleAutomaticDashSubstitution();
            virtual bool isAutomaticTextReplacementEnabled();
            virtual void toggleAutomaticTextReplacement();
            virtual bool isAutomaticSpellingCorrectionEnabled();
            virtual void toggleAutomaticSpellingCorrection();
            virtual bool isAsynchronousSpellCheckingEnabled();
            virtual void checkTextOfParagraph(const UChar*, int, uint64_t, Vector<WebCore::TextCheckingResult>&, int caretIdx);
			
			bool isEditing() const { return m_editing; }
	
			virtual void setEditable(bool editable);
			
			void selectingForCopy(bool on);
			void setSelectionGranularityForDesignMode(bool inDesignMode);
			
			// Sticky State
			void translateKeyWithStickyState( unsigned short& key, unsigned short& modifier, bool keyDown );
			virtual int textCaretDecoration();
            void enableSingleAndLockStickyStates(bool enable);
			
			virtual void wordCompletionReady(const WTF::String& prefix, const WTF::String& completion, const WebCore::VisiblePosition& pivot);

			bool canAutoCorrectSpellingErrors();

			bool canDoStickyState();
			bool canUndoAutoReplace();
						
			bool setStickyStateByCurrentSelection(bool forceStateChange);
			
			void allowEditorEvents();
			void disallowEditorEvents();
			bool editorEventsAllowed();
	            
			void setTrackballEnabled(bool enabled);
	
			WebCore::SpellingWidgetController* getSpellWidgetController();

            void clearWordCompletion();

            Palm::ClipboardController* getClipboardController() const { return m_clipboardController; }

            const Palm::WordCompletionController* getWordCompletionController() const { return &m_wordCompController; }

            void filterTextForInsertion(WTF::String& insertionText);

		private:

			struct NoSteTagList {
			public:
				NoSteTagList();
				~NoSteTagList() { };
				bool matchCommonTags(const WebCore::Element* el);

			private:
				void init();
				bool search(const WTF::String& name, const WTF::String& id);

				std::vector<WebCore::RegularExpression> m_tagsRegex; // common tags that indicate no STE
				const WebCore::Element* m_lastElement; // cache the last element
				bool m_lastElementMatched;		 // and the STE decision for the last element
				bool m_initialized;
			};

			class EditHistoryStack {
			public:
				void push(WTF::PassRefPtr<WebCore::EditCommand> command);
				WTF::PassRefPtr<WebCore::EditCommand> pop();

				bool isEmpty() const;
				void clear();
			private:
				WTF::Vector< WTF::RefPtr<WebCore::EditCommand> > m_stack;
			};

		    bool shouldTitleCaseByPosition(int cursorPos, const WTF::String& content);
		    bool shouldAutocapByPosition(int cursorPos, const WTF::String& content);
		    bool shouldAutocapByPreviousSibling();

		    // Used to expand selection to both boundaries of a word 
		    // Pivot word is used to expand selection when a shift + tap occurs
		    void selectForCopyPivotWord();
		    
		    // Used to position cursor to the left boundary of the pivot word
		    // from which selection will be expanded
		    void selectForCopyPivotWordBoundary();
		    
		    // Used to create a selection between the pivot word and a tap location
		    // without losing selection from the pivot word
		    void changeSelectionToWordBoundary();
		    
		    void changeSelectionToSentenceBoundary();
		    
		    void selectFromCursorToMark(const SelectionForCopy::Direction direction, 
		            const WebCore::VisibleSelection& selection);
		    
		    // Used to create a link from text that matches a parser pattern
		    // Called typically after a key event
		    void runTextLinkerAtCursor();
		    
            void checkWordCompletion();
            bool commitWordCompletion();
            void replaceWordCompletion(const WTF::String& completion);
            bool pointInWordCompletion(const WebCore::IntPoint& viewPt);

		    // Used to keep cursor position in a selection visible in browser viewport
		    bool keepSelectionInViewport(const WebCore::VisibleSelection& selection);

			bool allowTrackballEvent(int period);

			void checkWord(const WTF::String& word, unsigned int steState,
					WebCore::TextCheckingResult& result, bool autoCorrectAllowed, bool firstWordOfSentence = false, bool checkCacheOnly = false);
		    
		    // Used to compare position of a given selection to the selection pivot word
		    SelectionForCopy::Direction comparePositionToPivot(const WebCore::VisibleSelection& selection);
		    
		    SelectionForCopy::Direction comparePositionToMark(const WebCore::VisibleSelection& selection);
		    
		    // Used to determine the direction in which we "round off" the selection (word/paragraph granularity)
		    SelectionForCopy::Direction directionOfSelectionExpansion(const WebCore::VisibleSelection& mark, const WebCore::VisibleSelection& selection);

		    // used to expand selection to include adjacent terminal characters
		    void expandSelectionToPunctuation(WebCore::SelectionController::EAlteration alteration);
            void expandSelectionByParagraph(WebCore::SelectionController::EDirection direction);
			unsigned int getDefaultEditorState() const;
			unsigned int getStateForElement(WebCore::Element* element);
			unsigned int getStateForFocusedNode();
		    
		    static void setSTEAttributesFromModestring(WebCore::Element * element);
			static bool isObjectElement(const WebCore::Node* node);
            static bool isFocusableElement(const WebCore::Node* node);
			static bool getElementAttrBoolVal(const WebCore::Element* element, const WTF::AtomicString& valName);
			static bool shouldElementAlwaysHaveSteDisabled(const WebCore::Element* element);
            static bool shouldDisableWordCompletions(const WebCore::Element* el);
			static bool isValueTrue(const WTF::AtomicString& attrValue);
			static bool isValueFalse(const WTF::AtomicString& attrValue);
			static bool attrValTrue(const WebCore::Element* element, const WTF::AtomicString& attrName);
			static bool attrValFalse(const WebCore::Element* element, const WTF::AtomicString& attrName);
		    
		    WTF::String getPreviousWord();
		    bool isWhitespaceToLeft();
		    UChar charToLeft();

            void editorFocusChanged(WebCore::Node* focusedNode);

			Palm::WebPage* m_page;			
			Nova::SmartTextEngine* m_smartTextEngine;
			WTF::String m_lastWordSegment;
			bool m_editing;
			bool m_runningTextLinker;
			
			int m_disableEditorEventCount;
			
			bool m_trackballEnabled;
            bool m_smartInsertDelete;
            bool m_substitutionsPanelVisible;
            bool m_spellingUiVisible;
            bool m_automaticQuoteSubstitutionEnabled;
            bool m_automaticLinkDetectionEnabled;
            bool m_automaticDashSubstitutionEnabled;
			Palm::SpellCheck* m_spellChecker;
			EditHistoryStack	m_undoStack;
			EditHistoryStack	m_redoStack;
			
			EditorStickyState m_stickyState;
			SelectionForCopy m_selectionForCopy;
			
			NoSteTagList noSteTagList;

			WebCore::SpellingWidgetController* m_spellingWidget;

            bool m_allowWordCompletions;
            bool m_hasCompleteWord;

            // We use EditorClient::setInputMethodState() to find out when input focus
            // changes. There is at least one case where focus can change without this method
            // being called, when the currently focused Node is removed from the document by javascript 
            // via Node.removeChild. In this case, only EditorClient::textFieldDidEndEditing() will be 
            // called. We can use the EditorClient::willSetInputMethodState() to determine whether we 
            // should wait to respond to a focus change, or handle it inside EditorClient::textFieldDidEndEditing() 
            // since this method gets called in both cases but will fall in between a willSetInputMethodState and
            // setInputMethodState.
            bool m_expectsInputMethodStateCallback;

            // clipboard & selection widget controller
            Palm::ClipboardController* m_clipboardController;

            Palm::WordCompletionController m_wordCompController;

            friend class Palm::WordCompletionController;
    };
}

#endif

