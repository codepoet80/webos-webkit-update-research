#include "config.h"
#include "Editor.h"

#include "AXObjectCache.h"
#include "ApplyStyleCommand.h"
#include "CharacterNames.h"
#include "CreateLinkCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "ClipboardEvent.h"
#include "DeleteButtonController.h"
#include "DeleteSelectionCommand.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "EditorClient.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "KeyboardEvent.h"
#include "ModifySelectionListLevel.h"
#include "Page.h"
#include "Pasteboard.h"
#include "RemoveFormatCommand.h"
#include "RenderBlock.h"
#include "RenderPart.h"
#include "ReplaceSelectionCommand.h"
#include "Sound.h"
#include "Text.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "markup.h"
#include "visible_units.h"
#include <wtf/UnusedParam.h>
#if PLATFORM(WEBOS)
#include <palmwebpageprivate.h>
#include <palmwebpage.h>
#endif


namespace WebCore {

void Editor::autoReplaceSelectionWithText(const String& text, bool selectReplacement, bool smartReplace)
{
    RefPtr<DocumentFragment> fragment = createFragmentFromText(selectedRange().get(), text);
    if (m_frame->selection()->isNone() || !fragment)
    	return;

    applyCommand(ReplaceSelectionCommand::create(m_frame->document(), fragment, selectReplacement, smartReplace, true, true, false, EditActionAutoReplace));
    revealSelectionAfterEditingOperation();
}


// Called from client once spell checking result for word is ready.
// It will walk through the current text being edited, find all words == word
// that were waiting for spell checking response and repeat spell checking
// process on them. This time around, client is expected to have the response ready
// and return it immediately. (Palm client has a cache of spell checked words)
void Editor::spellCheckForWordReady(const String& word)
{
    if (word.isEmpty())
        return;

    SelectionController* selController = m_frame->selection();
    if (!selController)
        return;

    RefPtr<Node> root(0);
    if (selController->isContentEditable())
        root = highestEditableRoot(selController->selection().start());
    if (!root)
        return;

    RefPtr<Range> range(rangeOfContents(root.get()));
    RefPtr<Range> spellingRange;
    while (spellingRange = frame()->document()->markers()->findRangeForPendingSpellMarker(range.get(), word)) {
        bool markSpelling = isContinuousSpellCheckingEnabled();
        bool markGrammar = markSpelling && isGrammarCheckingEnabled();
        bool performTextCheckingReplacements = isAutomaticQuoteSubstitutionEnabled()
											|| isAutomaticLinkDetectionEnabled()
											|| isAutomaticDashSubstitutionEnabled()
											|| isAutomaticTextReplacementEnabled()
											|| (markSpelling && isAutomaticSpellingCorrectionEnabled());
        if (markSpelling || performTextCheckingReplacements)
            markAllMisspellingsAndBadGrammarInRanges(markSpelling, spellingRange.get(), markGrammar, spellingRange.get(), performTextCheckingReplacements);
        // make sure we clean up pending spelling markers
        frame()->document()->markers()->removeMarkers(spellingRange.get(), DocumentMarker::SpellPending);
    }
}

} // WebCore

