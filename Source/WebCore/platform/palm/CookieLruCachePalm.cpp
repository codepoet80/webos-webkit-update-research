/* ============================================================
 * Author: Renchi Raju
 * Date  : 2007-10-16
 * Copyright 2007 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h" 
 
#include "CookieLruCachePalm.h"

#include <stdio.h>

namespace WebCore
{

CookieLruCache::CookieLruCache(unsigned int maxCount)
    : m_maxCount(maxCount),
      m_count(0)
{
    if (m_maxCount == 0) {
        m_maxCount = 1;
    }

    m_list = new CookieEntrySet();
    m_list->m_next = m_list;
    m_list->m_prev = m_list;
}

CookieLruCache::~CookieLruCache()
{
    clear();
    delete m_list;
}

void
CookieLruCache::setMaxCount(unsigned int count)
{
    m_maxCount = count;
    if (m_maxCount == 0) {
        m_maxCount = 1;
    }

    while (m_count > m_maxCount) {
        dropLeastUsedEntrySet();
    }
}

unsigned int
CookieLruCache::maxCount() const
{
    return m_maxCount;
}

unsigned int
CookieLruCache::count() const
{
    return m_count;
}

void
CookieLruCache::clear()
{
    CookieEntrySet* e  = m_list->m_next;
    CookieEntrySet* tmp;

    while (e != m_list) {
    	tmp = e->m_next;
        delete e;
        e = tmp;
    }

    m_list->m_next = m_list;
    m_list->m_prev = m_list;

    m_count = 0;
    m_map.clear();
}

void
CookieLruCache::addCookie(const String& domainHead, const String& domainTail,
                          const String& path, const String& name,
                          const String& value, bool isSecure, int64_t expires)
{
    CookieEntrySet* entrySet = 0;
    CookieEntry*    entry    = 0;
    
    // do we already have this cookie entryset?
    entrySet = cookieEntrySet(domainTail);

    if (!entrySet) {
        entrySet = new CookieEntrySet;
        if (!entrySet) {
            return;
        }

        entrySet->m_domainTail = domainTail;

        m_map.set(domainTail, entrySet);
        m_count++;
    }
    
    // Move it to the head of the list
    moveEntrySetToHead(entrySet);

    // Check if we have already have an entry for this item
    for (unsigned int i = 0; i < entrySet->m_entries.size(); i++) {
        CookieEntry* e = entrySet->m_entries.at(i);

        if (e->m_domainHead == domainHead &&
            e->m_path == path &&
            e->m_name == name) {
            entry = e;
            break;
        }
    }

    // If not, create a new one and add it to the entrySet
    if (!entry) {
        entry = new CookieEntry;
        if (!entry) {
            return;
        }

        entrySet->m_entries.append(entry);
    }

    entry->m_domainHead = domainHead;
    entry->m_domainTail = domainTail;
    entry->m_path       = path;
    entry->m_name       = name;
    entry->m_value      = value;
    entry->m_expires    = expires;
    entry->m_isSecure   = isSecure;

    while (m_count > m_maxCount) {
        dropLeastUsedEntrySet();
    }
}

void
CookieLruCache::addDummyCookieEntrySet(const String& domainTail)
{
    CookieEntrySet* entrySet = 0;
    
    // do we already have this cookie entryset?
    entrySet = cookieEntrySet(domainTail);

    if (!entrySet) {
        entrySet = new CookieEntrySet;
        if (!entrySet) {
            return;
        }

        entrySet->m_domainTail = domainTail;

        m_map.set(domainTail, entrySet);
        m_count++;
    }
    
    // Move it to the head of the list
    moveEntrySetToHead(entrySet);

    while (m_count > m_maxCount) {
        dropLeastUsedEntrySet();
    }
}

void
CookieLruCache::delCookie(const String& domainHead, const String& domainTail,
                          const String& path, const String& name)
{
    if (domainTail.isEmpty())
        return;
    
    CookieEntrySet* entrySet = m_map.get(domainTail);
    int             index    = -1;

    if (!entrySet) {
        return;
    }

    for (unsigned int i = 0; i < entrySet->m_entries.size(); i++) {
        CookieEntry* e = entrySet->m_entries.at(i);
        if (e->m_domainHead == domainHead &&
            e->m_path == path &&
            e->m_name == name) {
            delete e;
            index = i;
            break;
        }
    }

    if (index == -1) {
        return;
    }

    entrySet->m_entries.remove(index);
}

CookieEntrySet*
CookieLruCache::cookieEntrySet(const String& domainTail)
{
    if (domainTail.isEmpty())
        return 0;

    CookieEntrySet* entrySet = m_map.get(domainTail);

    if (entrySet) {
        // Move it to the head of the list
        moveEntrySetToHead(entrySet);
    }

    return entrySet;
}

void
CookieLruCache::moveEntrySetToHead(CookieEntrySet* entrySet)
{
    if (m_list->m_next == entrySet) {
        // already at head of list
        return;
    }

    if (entrySet->m_next) {
        // entrySet is in the list. remove it from there first
        entrySet->m_next->m_prev = entrySet->m_prev;
        entrySet->m_prev->m_next = entrySet->m_next;
        entrySet->m_next = 0;
        entrySet->m_prev = 0;
    }

    m_list->m_next->m_prev = entrySet;
    entrySet->m_next = m_list->m_next;
    entrySet->m_prev = m_list;
    m_list->m_next   = entrySet;
}

void
CookieLruCache::dropLeastUsedEntrySet()
{
    CookieEntrySet* entrySet = m_list->m_prev;
    
    if (entrySet == m_list) {
        // list is empty.
        return;
    }

    entrySet->m_prev->m_next = m_list;
    m_list->m_prev           = entrySet->m_prev;

    m_map.remove(entrySet->m_domainTail);
    
    delete entrySet;

    m_count--;
}

}
