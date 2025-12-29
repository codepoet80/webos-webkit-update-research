/* ============================================================
 * Date  : 2007-10-16
 * Copyright 2007 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef COOKIE_LRU_CACHE_WEBOS_H
#define COOKIE_LRU_CACHE_WEBOS_H

#include <PlatformString.h>
#include <wtf/HashMap.h>
#include <StringHash.h>

namespace WebCore {

class CookieEntry {
public:

    String  m_domainHead;
    String  m_domainTail;
    String  m_path;
    String  m_name;
    String  m_value;
    int64_t m_expires;
    bool    m_isSecure;
};

class CookieEntrySet {
public:

    CookieEntrySet() {
        m_next = 0;
        m_prev = 0;
    }

    ~CookieEntrySet() {
        for (unsigned int i = 0; i < m_entries.size(); i++) {
            delete m_entries.at(i);
        }
    }

    String               m_domainTail;
    Vector<CookieEntry*> m_entries;

    CookieEntrySet*      m_next;
    CookieEntrySet*      m_prev;
};    

class CookieLruCache {
public:

    CookieLruCache(unsigned int maxCount = 20);
    ~CookieLruCache();

    void            setMaxCount(unsigned int count);
    unsigned int    maxCount() const;
    unsigned int    count() const;

    void            clear();
    
    void            addCookie(const String& domainHead, const String& domainTail,
                              const String& path, const String& name,
                              const String& value, bool isSecure, int64_t expires);
    void            addDummyCookieEntrySet(const String& domainTail);
    void            delCookie(const String& domainHead, const String& domainTail,
                              const String& path, const String& name);
    
    CookieEntrySet* cookieEntrySet(const String& domainTail);

private:

    void            moveEntrySetToHead(CookieEntrySet* entrySet);
    void            dropLeastUsedEntrySet();

    unsigned int                     m_maxCount;
    unsigned int                     m_count;
    CookieEntrySet*                  m_list;
    HashMap<String, CookieEntrySet*> m_map;
};

}

#endif /* COOKIE_LRU_CACHE_WEBOS_H */
