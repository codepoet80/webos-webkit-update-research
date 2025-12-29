
// Copyright 2010 Palm Inc.

#include "config.h"

#include <glib.h>
#include <wtf/Vector.h>
#include "CString.h"
#include "SpellCheck.h"
#include <webkitpalmsettings.h>
#include "Frame.h"
#include "Node.h"
#include "FocusController.h"
#include "visible_units.h"
#include "EditorClient.h"

#include "palmwebglobal.h"
#include "palmwebpageprivate.h"
#include "SmartTextEngine.h"
#include "wtf/unicode/Unicode.h"
#include "TextBreakIterator.h"
#include "TextBreakIteratorInternalICU.h"

#include <pbnjson.hpp>

namespace Palm {

SpellCheck*	SpellCheck::s_spellCheck;

static const size_t k_MaxQueueEntries = 4000;

SpellCheck::SpellCheckWordInfo::SpellCheckWordInfo() : inDictionary(true)
{
}

/**
 * Return the first non-auto-replace guess that should be automatically accepted.
 */
const SpellCheck::WordGuess* SpellCheck::SpellCheckWordInfo::getAutoAccept() const
{
	WTF::Vector<WordGuess>::const_iterator i;
	for (i = guesses.begin(); i != guesses.end(); ++i) {
		if (i->autoAccept && !i->autoReplace) {
			return &*i;
		}
	}

	return NULL;
}

/**
 * Return the first guess that should be automatically replaced.
 */
const SpellCheck::WordGuess* SpellCheck::SpellCheckWordInfo::getAutoReplace() const
{
	WTF::Vector<WordGuess>::const_iterator i;
	for (i = guesses.begin(); i != guesses.end(); ++i) {
		if (i->autoAccept && i->autoReplace) {
			return &*i;
		}
	}

	return NULL;
}

/**
 * Is the word mispelled?
 */
bool SpellCheck::SpellCheckWordInfo::isMisspelled() const
{
	return !inDictionary;
}

/**
 * Are there any guesses?
 */
bool SpellCheck::SpellCheckWordInfo::isEmpty() const
{
	return guesses.isEmpty();;
}

/**
 * Clear this word info.
 */
void SpellCheck::SpellCheckWordInfo::clear()
{
	inDictionary = true;
	guesses.clear();
}

SpellCheck::WordGuess::WordGuess(const WTF::String w) :
	  word(w)
	, spellCorrection(false)
	, autoReplace(false)
	, autoAccept(false)
{
}

bool SpellCheck::SpellCheckCache::add(const WTF::String& typedWord, const SpellCheckWordInfo& info)
{
	ASSERT(isMainThread());
	ASSERT(!typedWord.isEmpty());

	bool added = false;
	if (!m_entries.contains(typedWord)) {
		if (m_entryWords.size() == k_MaxQueueEntries) {
			// Purge the oldest (first) word in the queue
			WTF::String word = m_entryWords.first();
			m_entryWords.removeFirst();
			m_entries.remove(word);
		}
		m_entryWords.append(typedWord);
		m_entries.set(typedWord, info);
		added = true;
	}
    else {
        // update info of the checked word
        m_entries.set(typedWord, info);
    }

	ASSERT(static_cast<size_t>(m_entries.size()) == m_entryWords.size());
	ASSERT(static_cast<size_t>(m_entries.size()) <= k_MaxQueueEntries);
	return added;
}

void SpellCheck::SpellCheckCache::evict(const WTF::String& word)
{
	ASSERT(isMainThread());
	ASSERT(static_cast<size_t>(m_entries.size()) == m_entryWords.size());

	// Remove from our HashMap.
	WTF::HashMap<WTF::String, SpellCheckWordInfo>::iterator i = m_entries.begin();
	while (i != m_entries.end()) {
		if (equalIgnoringCase(i->first, word)) {
			m_entries.remove(i);
			i = m_entries.begin();
		}
		else {
			++i;
		}
	}

	// Now also remove from our priority queue which we use for aging the FIFO
restart:
	WTF::Deque<WTF::String>::iterator e = m_entryWords.begin();
	while (e != m_entryWords.end()) {
		if (equalIgnoringCase(*e, word)) {
			m_entryWords.remove(e);
			// Going old-school with my goto here because if I just set e=m_entryWords.begin()
			// here then I got an assertion. I don't have time to debug to determine if this
			// is a programming error on my part of a bug in WTF's Deque class (unlikely).
			goto restart;
		}
		else {
			++e;
		}
	}

	ASSERT(static_cast<size_t>(m_entries.size()) == m_entryWords.size());
	ASSERT(!m_entries.contains(word));
}

void SpellCheck::SpellCheckCache::clear()
{
	m_entries.clear();
	m_entryWords.clear();
}

bool SpellCheck::SpellCheckCache::lookup(const WTF::String& typedWord, SpellCheckWordInfo& info) const
{
	if (m_entries.contains(typedWord)) {
		info = m_entries.get(typedWord);
		return true;
	}
	else {
		return false;
	}
}


SpellCheck::SpellCheck() :
	 m_serviceClient(NULL)
	,m_attachedToServiceBus(false)
	,m_serviceBusAttachAttempted(false)
	,m_smartKeySvcUp(false)
	,m_smartKeyStatusToken(NULL)
    ,m_processingResponse(false)
{
	ASSERT(s_spellCheck==NULL);

    if (PalmBrowserSettings()->checkSpelling != WebKitPalmSettings::DISABLED)
        attachToServiceBus();

	s_spellCheck = this;
}


SpellCheck::~SpellCheck()
{
	s_spellCheck = NULL;

	LSError lserror;
    LSErrorInit(&lserror);

    if (m_serviceClient && !LSUnregister(m_serviceClient, &lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}

bool SpellCheck::attachToServiceBus()
{
	if (!m_serviceBusAttachAttempted) {

        m_serviceBusAttachAttempted = true;

		LSError lserror;
		LSErrorInit(&lserror);

		if (!LSRegister(NULL, &m_serviceClient, &lserror)) {
			g_warning("Can't register: %s", lserror.message);
			LSErrorFree(&lserror);
		}
		else {
			m_attachedToServiceBus = LSGmainAttach(m_serviceClient, Palm::WebGlobal::mainLoop(), &lserror);
			if (!m_attachedToServiceBus) {
				g_warning("Can't attach: %s", lserror.message);
				LSErrorFree(&lserror);
			}
			else {
				registerForSmartKeyStatus();
			}
		}
	}

	return m_attachedToServiceBus;
}

/**
 * Register to be notified by the luna service bus whenever the SmartKey 
 * service connects or disconnects.
 */
bool SpellCheck::registerForSmartKeyStatus()
{
	ASSERT(m_serviceClient != NULL);

    LSError error;
    LSErrorInit(&error);

    bool succeeded = LSCall(m_serviceClient, "palm://com.palm.bus/signal/registerServerStatus",
                      "{\"serviceName\":\"com.palm.smartKey\"}",
                      smartKeyStatusChangeCallback, this, &m_smartKeyStatusToken,
                      &error);
    if (!succeeded) {
        g_critical("Failed in calling palm://com.palm.bus/signal/registerServerStatus: %s",
                   error.message);
        LSErrorFree(&error);
    }

	return succeeded;
}

/**
 * Called by WebKit whenever the SmartKey service connects/disconnects to/from the 
 * Luna service bus.
 */
bool SpellCheck::smartKeyStatusChangeCallback(LSHandle *sh, LSMessage *message, void *ctx)
{
    if (!message)
        return true;

	SpellCheck* checker = static_cast<SpellCheck*>(ctx);
    const char* payload = LSMessageGetPayload(message); 
	
	static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

    pbnjson::JDomParser parser;
    if (parser.parse(payload, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();

		if (parsed.hasKey("connected")) {
			if (parsed["connected"].asBool()) {
				checker->m_smartKeySvcUp = true;
				checker->m_cache.clear();
                checker->m_pendingRequests.clear();
				checker->subscribeForDictionarySignals();
                checker->subscribeForLanguageChangeSignals();
			}
			else {
				checker->m_smartKeySvcUp = false;
			}
		}
	}

	return true;
}

/**
 * Called whenever the palm://com.palm.smartKey/signals/databaseModified is called.
 */
bool SpellCheck::dictionaryChangedCallback(LSHandle *sh, LSMessage *message, void *ctx)
{
	SpellCheck* checker = static_cast<SpellCheck*>(ctx);

	std::string jsonRaw = LSMessageGetPayload(message);
    
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

    pbnjson::JDomParser parser;
    if (parser.parse(jsonRaw, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();

		if (parsed.hasKey("database")) {
			const std::string action = parsed["action"].asString();
			if (action == "added" || action == "removed") {
				const std::string database = parsed["database"].asString();
				if (database == "user" || database == "volatile") {
					const std::string word = parsed["word"].asString();
					checker->m_cache.evict(WTF::String::fromUTF8(word.c_str()));
				}
				else if (database == "auto-replace") {
					const std::string shortcut = parsed["shortcut"].asString();
					checker->m_cache.evict(WTF::String::fromUTF8(shortcut.c_str()));
				}
			}
		}
    }

	return true;
}

bool SpellCheck::languageChangedCallback(LSHandle *sh, LSMessage *message, void *ctx)
{
    // purge the cache when the user switches languages
    SpellCheck* checker = static_cast<SpellCheck*>(ctx);
    checker->clearCache();
    g_debug("%s: cache purged", __FUNCTION__);
    return true;
}


/**
 * Subscribe to be notified by the SmartKey service whenever it has changed one of
 * its databases (AKA dictionarlies).
 */
bool SpellCheck::subscribeForDictionarySignals()
{
	LSError error;
	LSErrorInit(&error);

	bool succeeded = LSCall(m_serviceClient,
				 "palm://com.palm.bus/signal/addmatch", "{\"category\":\"/com/palm/smartKey\", \"method\":\"databaseModified\"}",
				 dictionaryChangedCallback, this, NULL, &error);
	if (!succeeded) {
		g_critical("Failed in calling palm://com.palm.smartKey/signals/databaseModified: %s",
				   error.message);
		LSErrorFree(&error);
	}

	return succeeded;
}

/**
 * Subscribe to be notified by the SmartKey service whenever it has changed languages (AKA virtual keyboard changed)
 */
bool SpellCheck::subscribeForLanguageChangeSignals()
{
    LSError error;
    LSErrorInit(&error);

    bool succeeded = LSCall(m_serviceClient,
                "palm://com.palm.bus/signal/addmatch", "{\"category\":\"/com/palm/smartKey\", \"method\":\"languageChanged\"}",
                languageChangedCallback, this, NULL, &error);
    if (!succeeded) {
        g_critical("Failed in calling palm://com.palm.smartKey/signals/languageChanged: %s",
                error.message);
        LSErrorFree(&error);
    }

    return succeeded;
}

SpellCheck*	SpellCheck::getSpellCheck()
{
	if (s_spellCheck == NULL) {
		s_spellCheck = new SpellCheck();
	}
	return s_spellCheck;
}

void SpellCheck::forceReSpellCheck(WebCore::Frame* frame, const WTF::String& word)
{
	if (!frame)
        return;

	WebCore::Node* focusedNode = frame->document()->focusedNode();
	if (focusedNode &&
		(focusedNode->nodeType() == WebCore::Node::TEXT_NODE || focusedNode->nodeType() == WebCore::Node::ELEMENT_NODE) &&
        frame->editor())
    {
        m_processingResponse = true;
        frame->editor()->spellCheckForWordReady(word);
        m_processingResponse = false;
    }
}

void SpellCheck::insertWordCompletion(WebCore::Frame* frame, WebCore::Node* target, const WebCore::VisiblePosition& cursor,
                                    const WTF::String& prefix, const WTF::String& completion)
{
    if (!frame || !target)
        return;

    // make sure that the target for this completion is still acceptable
    if (frame->document()->focusedNode() != target)
        return;

    if (!frame->editor() || !frame->editor()->client())
        return;

    m_processingResponse = true;
    frame->editor()->client()->wordCompletionReady(prefix, completion, cursor);
    m_processingResponse = false;
}

bool SpellCheck::spellCheckResponse(LSHandle *sh, LSMessage *reply, void *ctx)
{
	const char* const k_pszResponseSchema = "{}";
	SpellCheckResponseCtx* spellCheckResponseCtx = static_cast<SpellCheckResponseCtx*>(ctx);
	if (!spellCheckResponseCtx)
		return true;

	SpellCheck* checker = SpellCheck::getSpellCheck();
	std::string jsonRaw = LSMessageGetPayload(reply);
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment(k_pszResponseSchema);

    // make sure to clear out all pending responses regardless of their outcome
    checker->m_pendingRequests.remove(spellCheckResponseCtx->word);

    pbnjson::JDomParser parser;
    if (parser.parse(jsonRaw, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();

		if (parsed["returnValue"].asBool()) {
			SpellCheckWordInfo info;
			pbnjson::JValue guesses = parsed["guesses"];
			if (guesses.isArray()) {
				for (int i = 0; i < guesses.arraySize(); i++) {
					pbnjson::JValue val = guesses[i];
					SpellCheck::WordGuess guess(WTF::String::fromUTF8(val["str"].asString().c_str()));
					if (val.hasKey("sp")) {
						guess.spellCorrection = val["sp"].asBool();
					}
					if (val.hasKey("auto-replace")) {
						guess.autoReplace = val["auto-replace"].asBool();
					}
					if (val.hasKey("auto-accept")) {
						guess.autoAccept = val["auto-accept"].asBool();
					}
					info.guesses.append(guess);
				}
			}

			// treat presence in dictionary as indicative of correct spelling
			info.inDictionary = parsed["spelledCorrectly"].asBool();
			checker->m_cache.add(spellCheckResponseCtx->word, info);
			checker->forceReSpellCheck(spellCheckResponseCtx->frameRef.get(), spellCheckResponseCtx->word);
		}
    }

    delete spellCheckResponseCtx;
	return true;
}

SpellCheck::SpellCheckResult SpellCheck::getGuessesForWord(const WTF::String& word, SpellCheckWordInfo& info, WebCore::Frame* frame)
{
	const char* const k_pszCallSchema = "{\"type\" : \"object\", \
									\"properties\" : { \
										\"guess\" : {\"type\" : \"string\"} \
									} \
								}";

	ASSERT(info.isEmpty());

	if (m_cache.lookup(word, info)) {
		return SpellCheckReady;
	}

	if (!m_smartKeySvcUp)
		return SpellCheckError;

	if (attachToServiceBus() && !m_pendingRequests.contains(word)) {
		
		pbnjson::JValue callData = pbnjson::Object();
		callData.put("query", std::string(word.utf8().data()));
        if (m_pendingRequests.size() > 0)
            callData.put("quick", true);	// if requests are pending, a quick answer is better

		pbnjson::JGenerator serializer(NULL);
		std::string payload;
		pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);

		if (serializer.toString(callData, callSchema, payload)) {
			LSError lserror;
			LSErrorInit(&lserror);
			SpellCheckResponseCtx* ctx = new SpellCheckResponseCtx(frame, word);
			if (!LSCallOneReply(m_serviceClient, "palm://com.palm.smartKey/search",
					payload.c_str(), spellCheckResponse, static_cast<void*>(ctx), NULL, &lserror)) {
				LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                delete ctx;
                return SpellCheckError;
			}
            m_pendingRequests.add(word);
		}
	}

	return SpellCheckPending;
}

SpellCheck::SpellCheckResult SpellCheck::getGuessesFromCacheOnly(const WTF::String& word, SpellCheckWordInfo& info)
{
    ASSERT(info.isEmpty());
    return (m_cache.lookup(word, info) ? SpellCheckReady : SpellCheckNotInCache);
}

void SpellCheck::fetchGuessesIgnoreCache(const WTF::String& word, WebCore::Frame* frame)
{
    const char* const k_pszCallSchema = "{\"type\" : \"object\", \
                                         \"properties\" : { \
                                             \"guess\" : {\"type\" : \"string\"} \
                                            } \
                                        }";

    if (!m_smartKeySvcUp)
        return;

    if (attachToServiceBus()) {

        pbnjson::JValue callData = pbnjson::Object();
        callData.put("query", std::string(word.utf8().data()));
        if (m_pendingRequests.size() > 0)
            callData.put("quick", true);	// if requests are pending, a quick answer is better

        pbnjson::JGenerator serializer(NULL);
        std::string payload;
        pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);

        if (serializer.toString(callData, callSchema, payload)) {
            LSError lserror;
            LSErrorInit(&lserror);
            SpellCheckResponseCtx* ctx = new SpellCheckResponseCtx(frame, word);
            if (!LSCallOneReply(m_serviceClient, "palm://com.palm.smartKey/search",
                        payload.c_str(), spellCheckResponse, static_cast<void*>(ctx), NULL, &lserror)) {
                LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                delete ctx;
            }
        }
    }
}

/**
 * Let luna-systemui know that the user db has been modified so that it can display
 * a message to the user.
 */
bool SpellCheck::notifyDictionaryModify(const std::string& word, DbModifyAction action)
{
	pbnjson::JValue callData = pbnjson::Object();

	callData.put("event", "textAssistNotification");
	pbnjson::JValue message = pbnjson::Object();
	message.put("word", word);
	switch (action) {
		case DbModifyAdded:
			message.put("action", "added");
			break;
		case DbModifyRemoved:
			message.put("action", "removed");
			break;
	}

	callData.put("message", message);

	const char* const k_pszCallSchema = "{\"type\" : \"object\", \
									\"properties\" : { \
										\"event\" : {\"type\" : \"string\"}, \
										\"message\" : {\"type\" : \"object\"}, \
										\"properties\" : { \
											\"action\" : {\"type\" : \"string\"}, \
											\"word\" : {\"type\" : \"string\"} \
										} \
									} \
								}";

	pbnjson::JGenerator serializer(NULL);
	std::string payload;
	pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);
	if (serializer.toString(callData, callSchema, payload)) {

		LSError lserror;
		LSErrorInit(&lserror);
		if (LSCall(m_serviceClient, "palm://com.palm.systemmanager/publishToSystemUI",
				payload.c_str(), learnWordResponse, const_cast<SpellCheck*>(this), NULL, &lserror)) {
		}
		else {
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
	}

	return true;
}

bool SpellCheck::learnWordResponse(LSHandle *sh, LSMessage *reply, void *ctx)
{
	SpellCheck* checker = static_cast<SpellCheck*>(ctx);

	const char* const k_pszResponseSchema = "{}";
	static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment(k_pszResponseSchema);

	std::string jsonRaw = LSMessageGetPayload(reply);

    pbnjson::JDomParser parser;
    if (parser.parse(jsonRaw, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();

		if (parsed["returnValue"].asBool() && parsed.hasKey("word")) {

			checker->notifyDictionaryModify(parsed["word"].asString(), DbModifyAdded);
		}
	}

	return true;
}

void SpellCheck::learnWord(const WTF::String& word)
{
	if (!m_smartKeySvcUp)
		return;

	const char* const k_pszCallSchema = "{\"type\" : \"object\", \
									\"properties\" : { \
										\"word\" : {\"type\" : \"string\"} \
									} \
								}";

	ASSERT(!word.isEmpty());

	m_cache.evict(word);

	if (attachToServiceBus()) {
		
		pbnjson::JValue callData = pbnjson::Object();
		callData.put("word", std::string(word.utf8().data()));

		pbnjson::JGenerator serializer(NULL);
		std::string payload;
		pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);

		if (serializer.toString(callData, callSchema, payload)) {
			LSError lserror;
			LSErrorInit(&lserror);
			if (LSCall(m_serviceClient, "palm://com.palm.smartKey/addUserWord",
					payload.c_str(), learnWordResponse, const_cast<SpellCheck*>(this), NULL, &lserror)) {
			}
			else {
				LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
			}
		}
	}
}

bool SpellCheck::forgetWordResponse(LSHandle *sh, LSMessage *reply, void *ctx)
{
	SpellCheck* checker = static_cast<SpellCheck*>(ctx);

	const char* const k_pszResponseSchema = "{}";
	static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment(k_pszResponseSchema);

	std::string jsonRaw = LSMessageGetPayload(reply);

    pbnjson::JDomParser parser;
    if (parser.parse(jsonRaw, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();

		if (parsed["returnValue"].asBool() && parsed.hasKey("word")) {

			checker->notifyDictionaryModify(parsed["word"].asString(), DbModifyRemoved);
		}
	}

	return true;
}

void SpellCheck::forgetWord(const WTF::String& word)
{
	if (!m_smartKeySvcUp)
		return;

	const char* const k_pszCallSchema = "{\"type\" : \"object\", \
									\"properties\" : { \
										\"word\" : {\"type\" : \"string\"} \
									} \
								}";

	ASSERT(!word.isEmpty());

	if (attachToServiceBus()) {
		
		pbnjson::JValue callData = pbnjson::Object();
		callData.put("word", std::string(word.utf8().data()));

		pbnjson::JGenerator serializer(NULL);
		std::string payload;
		pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);

		if (serializer.toString(callData, callSchema, payload)) {
			LSError lserror;
			LSErrorInit(&lserror);
			if (LSCall(m_serviceClient, "palm://com.palm.smartKey/removeUserWord",
					payload.c_str(), forgetWordResponse, const_cast<SpellCheck*>(this), NULL, &lserror)) {
			}
			else {
				LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
			}
		}
	}
}

//
// Don't forget to run the unit tests when modifying this function!!!
//
void SpellCheck::parseParagraph(const UChar* text, int length, WTF::Vector<WordRange>& words)
{
	WordRange range;
	range.firstWordInSentence = false; // not used for now

	WebCore::TextBreakIterator* boundary = WebCore::wordBreakIterator(text, length);
	if (!boundary)
		return;

	int32_t start = WebCore::textBreakFirst(boundary);
	for (int32_t end = WebCore::textBreakNext(boundary); end != WebCore::TextBreakDone; start = end, end = WebCore::textBreakNext(boundary)) {
		if (u_isalnum(text[start])) {
			range.start = start;
			range.end = end;
			words.append(range);
		}
	}
}

void SpellCheck::clearCache()
{
    m_cache.clear();
}

bool SpellCheck::wordCompletionResponse(LSHandle *sh, LSMessage *reply, void *ctx)
{
    const char* const k_pszResponseSchema = "{}";
    WordCompletionResponseCtx* wordCompletionResponseCtx = static_cast<WordCompletionResponseCtx*>(ctx);
    if (!wordCompletionResponseCtx)
        return true;

    SpellCheck* checker = SpellCheck::getSpellCheck();
    std::string jsonRaw = LSMessageGetPayload(reply);
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment(k_pszResponseSchema);

    pbnjson::JDomParser parser;
    if (parser.parse(jsonRaw, inputSchema)) {
        pbnjson::JValue parsed = parser.getDom();

        if (parsed["returnValue"].asBool()) {
            WTF::String completion;
            if (parsed["comp"].isString()) {
                completion = WTF::String::fromUTF8(parsed["comp"].asString().c_str());
            }

            checker->insertWordCompletion(wordCompletionResponseCtx->frameRef.get(),
                                            wordCompletionResponseCtx->targetRef.get(),
                                            wordCompletionResponseCtx->cursor,
                                            wordCompletionResponseCtx->prefix,
                                            completion);
        }
    }

    delete wordCompletionResponseCtx;
    return true;
}

SpellCheck::WordCompletionResult SpellCheck::getCompletionForWord(const WTF::String& prefix, 
		WebCore::Frame* frame, WebCore::Node* target, const WebCore::VisiblePosition& cursor)
{
    const char* const k_pszCallSchema = "{\"type\" : \"object\", \
                                    \"properties\" : { \
                                        \"prefix\" : {\"type\" : \"string\"} \
                                    } \
                                }";

    ASSERT(!prefix.isEmpty());

    if (!m_smartKeySvcUp)
        return WordCompletionError;

    if (attachToServiceBus()) {

        pbnjson::JValue callData = pbnjson::Object();
        callData.put("prefix", std::string(prefix.utf8().data()));

        pbnjson::JGenerator serializer(NULL);
        std::string payload;
        pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(k_pszCallSchema);

        if (serializer.toString(callData, callSchema, payload)) {
            LSError lserror;
            LSErrorInit(&lserror);
            WordCompletionResponseCtx* ctx = new WordCompletionResponseCtx(frame, target, cursor, prefix);
            if (!LSCallOneReply(m_serviceClient, "palm://com.palm.smartKey/getCompletion",
                payload.c_str(), wordCompletionResponse, static_cast<void*>(ctx), NULL, &lserror)) {
                LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                delete ctx;
                return WordCompletionError;
            }
        }
    }

    return WordCompletionPending;
}

}
