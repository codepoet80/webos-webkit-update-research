#ifndef SPELL_CHECK_H
#define SPELL_CHECK_H

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Deque.h>
#include <wtf/text/WTFString.h>
#include <StringHash.h>
#include <lunaservice.h>
#include <string>

#include "VisiblePosition.h"

namespace WebCore {
    class Frame;
}

namespace Palm {

    class WebPage;

/**
 * Represents the first/last characters of a word in a string of text.
 */
struct WordRange
{
    int	start;	///< The first character (inclusive)
    int end;	///< The last character (exclusive i.e. One beyond)
    bool firstWordInSentence;	///< Is this word the first in a sentence?

    int length() const { return end - start; }
    WordRange() : start(0), end(0), firstWordInSentence(false) {}
    WordRange(const WordRange& rhs) : start(rhs.start), end(rhs.end), firstWordInSentence(rhs.firstWordInSentence) {}
    WordRange(int s, int e, bool f=false) : start(s), end(e), firstWordInSentence(f) {}
    bool operator==(const WordRange& rhs) const {
        return start == rhs.start && end == rhs.end &&
            firstWordInSentence == rhs.firstWordInSentence;
    }
    bool operator!=(const WordRange& rhs) const {
        return start != rhs.start || end != rhs.end ||
            firstWordInSentence != rhs.firstWordInSentence;
    }
};

/**
 * A class to check the spelling/grammar of words.
 */
class SpellCheck
{
public:
	/**
	 * Information about a single guess for a word not in the dictionary.
	 */
	struct WordGuess {
		WTF::String word;	///< The guess (not necessarily the original word).
		bool spellCorrection;	///< Guess is a result of a spelling correction?
		bool autoReplace;		///< Guess is a result of an auto replace db entry.
		bool autoAccept;		///< Spell checker is recommending that we auto accept first guess.

		WordGuess(const WTF::String w);
	};

	/**
	 * Contains the spell check information for a single word.
	 */
	struct SpellCheckWordInfo {
		SpellCheckWordInfo();
		bool isEmpty() const;
		void clear();
		bool isMisspelled() const;
		const WordGuess* getAutoAccept() const;
		const WordGuess* getAutoReplace() const;

		bool	inDictionary;	///< Was this word in any dictionary.
		WTF::Vector<WordGuess> guesses;	///< Collection of guesses (may be empty - even if mispelled.)
	};

	/**
	 * Cache responses from the spell check service for a set of words.
	 */
	struct SpellCheckCache {
	public:
		bool add(const WTF::String& typedWord, const SpellCheckWordInfo& info);
		void evict(const WTF::String& typedWord);
		void clear();
		bool lookup(const WTF::String& typedWord, SpellCheckWordInfo& info) const;

	private:
		WTF::Deque<WTF::String> m_entryWords;
		WTF::HashMap<WTF::String, SpellCheckWordInfo>	m_entries;
	};

	/*
	 * Struct to save context for the last asynchronous spell request.
	 */
	struct SpellCheckResponseCtx {

		RefPtr<WebCore::Frame> frameRef; 	// active frame
		WTF::String word;				// word to spell check

		SpellCheckResponseCtx(WebCore::Frame* frame, const WTF::String& str) {
			frameRef = frame;
			word = str;
		}
	};

    /*
     * Struct to save context for the last asynchronous word completion request.
     */
    struct WordCompletionResponseCtx {

        RefPtr<WebCore::Frame> frameRef; // active frame
        RefPtr<WebCore::Node> targetRef; // target node of the completion
        WebCore::VisiblePosition cursor; // cursor position at the time of the request
        WTF::String prefix; // prefix to complete

        WordCompletionResponseCtx(WebCore::Frame* frame, WebCore::Node* target,
                                    const WebCore::VisiblePosition& cur, const WTF::String& pre) {
            frameRef = frame;
            targetRef = target;
            cursor = cur;
            prefix = pre;
        }
    };

	static SpellCheck*	getSpellCheck();

	enum SpellCheckResult {
		SpellCheckReady,
		SpellCheckPending,
        SpellCheckNotInCache,
		SpellCheckError
	};
	SpellCheckResult getGuessesForWord(const WTF::String& word, SpellCheckWordInfo& info, WebCore::Frame* frame);
    SpellCheckResult getGuessesFromCacheOnly(const WTF::String& word, SpellCheckWordInfo& info);
    void fetchGuessesIgnoreCache(const WTF::String& word, WebCore::Frame* frame);

    bool processingResponse() const { return m_processingResponse; }

	void learnWord(const WTF::String& word);
	void forgetWord(const WTF::String& word);
	static void parseParagraph(const UChar* text, int length, WTF::Vector<WordRange>& words);

    void clearCache();

    enum WordCompletionResult {
        WordCompletionReady,
        WordCompletionPending,
        WordCompletionError
    };
    WordCompletionResult getCompletionForWord(const WTF::String& prefix, WebCore::Frame* frame, WebCore::Node* target, const WebCore::VisiblePosition& cursor);

private:

	SpellCheck();
	~SpellCheck();

	enum DbModifyAction {
		DbModifyAdded,
		DbModifyRemoved
	};

	static bool spellCheckResponse(LSHandle *sh, LSMessage *reply, void *ctx);
	static bool learnWordResponse(LSHandle *sh, LSMessage *reply, void *ctx);
	static bool forgetWordResponse(LSHandle *sh, LSMessage *reply, void *ctx);
	static bool smartKeyStatusChangeCallback(LSHandle *sh, LSMessage *message, void *ctx);
	static bool dictionaryChangedCallback(LSHandle *sh, LSMessage *message, void *ctx);
    static bool languageChangedCallback(LSHandle *sh, LSMessage *message, void *ctx);

    static bool wordCompletionResponse(LSHandle *sh, LSMessage *reply, void *ctx);

	bool attachToServiceBus();
	bool registerForSmartKeyStatus();
	bool subscribeForDictionarySignals();
    bool subscribeForLanguageChangeSignals();
	void forceReSpellCheck(WebCore::Frame* frame, const WTF::String& word);
	void insertWordCompletion(WebCore::Frame* frame, WebCore::Node* target, const WebCore::VisiblePosition& cursor, const WTF::String& prefix, const WTF::String& completion);
	bool notifyDictionaryModify(const std::string& word, DbModifyAction action);

	static SpellCheck*	s_spellCheck;

	LSHandle*         m_serviceClient;
	bool              m_attachedToServiceBus;
	bool              m_serviceBusAttachAttempted;
	bool              m_smartKeySvcUp;
	SpellCheckCache   m_cache;
	LSMessageToken    m_smartKeyStatusToken;
    bool m_processingResponse;
    WTF::HashSet<WTF::String>        m_pendingRequests;
};

}

#endif
