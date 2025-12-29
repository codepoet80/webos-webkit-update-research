/* ============================================================
 * Date  : 2007-10-09
 * Copyright 2007 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef COOKIESERVICEWEBOS_H
#define COOKIESERVICEWEBOS_H

#include <stdint.h>
#include <string>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

#include "DatabaseWatcher.h"

namespace WebCore {

   class KURL;
   class SQLiteDatabase;
   class SQLiteStatement;
   class CookieLruCache;
   class DatabaseWatcher;
    
   class CookieService {
   public:

       static CookieService* sharedInstance( const char *appId = 0 );
       static void clearAllCookieJars();

       void init();
       void shutdown();

       void setCookies(const KURL& url, const String& value, const String& appId = "");
        
       String cookies(const KURL& url, const String& appId = "");
    
       bool cookiesEnabled();

       void clear();

	   bool clearForDomain(const KURL& url);
       bool dumpDatabase(const String& dumpFile, PassRefPtr<DatabaseWatcher> watcher);
       bool restoreDatabase(const String& dumpFile, PassRefPtr<DatabaseWatcher> watcher);
       bool getDatabaseModInfo(time_t& mtime, int& numOpenMods) const;
       bool copyCookiesToDomain(const KURL& srcUrl, const KURL& dstUrl);

   private:

       CookieService(const char* appId);
       ~CookieService();

       bool isValidDatabase();
       bool createDatabaseTables();
       int  databaseVersion();

       bool parseCookie(const String& cookieStr, const KURL& url,
                        String& domain, String& path,
                        String& name, String& value,
                        int64_t& expires, bool& isSecure);

       bool domainTailMatch(const String& a, const String& b);
       bool subPathMatch(const String& a, const String& b);

	   bool clearSessionCookies();
       static int  secondDotFromTailPos(const String& str);
       static String getDomainForUrl(const KURL& url);
       static bool splitDomain(const String& domain, String& domainHead, String& domainTail);
       
       SQLiteDatabase*	m_db;
       SQLiteStatement*	m_insertCookieStmt;
       SQLiteStatement*	m_selectCookieStmt;
       CookieLruCache* 	m_cache;
       std::string		m_appId;
       std::string		m_databasePath;
   };
}    

#endif /* COOKIESERVICEWEBOS_H */
