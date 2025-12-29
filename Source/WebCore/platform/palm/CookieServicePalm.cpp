/* ============================================================
 * Author: Renchi Raju
 * Date  : 2007-10-09
 * Copyright 2007 Palm, Inc. All rights reserved.
 * ============================================================ */

#include <config.h>
#include "CookieServicePalm.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wtf/PassRefPtr.h>

#include <SQLiteDatabase.h>
#include <SQLiteStatement.h>
#include <SQLiteTransaction.h>
#include <Database.h>
#include <CString.h>
#include <Vector.h>
#include <KURL.h>
#include <Logging.h>
#include <map>
#include <string>
#include <dirent.h>

#include "CookieLruCachePalm.h"
#include "parsedate.h"
#include "webkitpalmsettings.h"

namespace WebCore {

static const int      sCookieDbVersion = 1;

//static char*          sCookiePath = 0;

static const char*    sKeyDomain  = "domain";
static const char*    sKeyPath    = "path";
static const char*    sKeyMaxAge  = "max-age";
static const char*    sKeySecure  = "secure";
static const char*    sKeyExpires = "expires";

static const unsigned int kMaxCookieLen = 5100;

CookieService::CookieService( const char* appId )
    : m_db(0),
      m_insertCookieStmt(0),
      m_selectCookieStmt(0),
      m_appId( appId ? appId : "" )
{
    m_cache        = new CookieLruCache;
}

CookieService::~CookieService()
{
    shutdown();
	delete m_cache;
}

// Note : this map could grow unbounded, but in reality it will not because
// browserserver tends to not run for very long. The correct solution for
// this is to LRU expire the cookie service instances. 
typedef std::map<std::string,CookieService*> CookieServiceMap;
static CookieServiceMap sCookieServices;

CookieService* CookieService::sharedInstance( const char* appId )
{
	//printf("CookieService::sharedInstance requesting cookies for appid=(%s)\n", ( appId ? appId : "" ) );

	if( !appId )
		appId = "";
	
	CookieServiceMap::iterator it = sCookieServices.find(appId);
	if( it != sCookieServices.end() )
		return it->second;

	CookieService* service = new CookieService(appId);
	sCookieServices[ std::string(appId) ] = service;
	service->init();
	return service;
}

void
CookieService::init()
{
    if (m_db) {
        return;
    }

    m_databasePath = PalmBrowserSettings()->cookieJarPath;

	gchar* cookieJarDir = g_path_get_dirname(m_databasePath.c_str());
    if (::g_mkdir_with_parents(cookieJarDir, 0755) != 0) {
        LOG_ERROR("Failed to create resource dir: %s\n", cookieJarDir);
		g_free(cookieJarDir);
        return;
    }

    g_free(cookieJarDir);

    m_db = new SQLiteDatabase();
    if (!m_db) {
        LOG_ERROR("Failed to create cookies sql db\n");
        return;
    }
    
    if( m_appId.size() ) {
		m_databasePath += ".";
		m_databasePath += m_appId;
	}
    
    //printf(" opening cookies database at %s (m_appId=%s)\n", m_databasePath.c_str(), m_appId.c_str() );

    if (!m_db->open( m_databasePath.c_str() )) {
        LOG_ERROR("Failed to open cookies sql db\n");
        delete m_db;
        m_db = 0;
        return;
    }    

    if (!isValidDatabase()) {
        m_db->clearAllTables();
        if (!createDatabaseTables()) {
            delete m_db;
            m_db = 0;
            return;
        }
    }

    m_insertCookieStmt = new SQLiteStatement(*m_db, "INSERT INTO Cookies VALUES "
                                             "((?), (?), (?), (?), (?), (?), (?))");
    if (!m_insertCookieStmt) {
        delete m_db;
        m_db = 0;
        return;
    }

    if (m_insertCookieStmt->prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare sql statement\n");
        delete m_db;
        delete m_insertCookieStmt;
        m_db = 0;
        m_insertCookieStmt = 0;
        return;
    }

    m_selectCookieStmt = new SQLiteStatement(*m_db,
                                             "SELECT domain_head, domain_tail, path, "
                                             " name, value, expires, secure FROM Cookies "
                                             "WHERE domain_tail = (?) "
                                             "ORDER BY LENGTH(domain_head) DESC, LENGTH(path) DESC;");
    if (!m_selectCookieStmt) {
        delete m_db;
        delete m_insertCookieStmt;
        m_db = 0;
        m_insertCookieStmt = 0;
        return;
    }

    if (m_selectCookieStmt->prepare() != SQLResultOk) {
        LOG_ERROR("Failed to prepare sql statement\n");
        delete m_db;
        delete m_insertCookieStmt;
        delete m_selectCookieStmt;
        m_db = 0;
        m_insertCookieStmt = 0;
        m_selectCookieStmt = 0;
        return;
    }
    
    // Turn synchronous writing off. Boosts performance significantly. there
    // is a risk of db being corrupted if power goes off, but cookie db is
    // not that critical.
    m_db->executeCommand("PRAGMA synchronous = OFF;");

	// Because many Mojo applications create session cookies not realizing that they
	// aren't supposed to stick around we're only clearing the BrowserServer cookies
	// to avoid breaking those apps.
	if (PalmBrowserSettings()->runningInBrowserServer) {
		if ( !clearSessionCookies()) {	// clear the previous run's session cookies
			LOG_ERROR("Failure clearing session cookies");
		}
	}
}

void
CookieService::shutdown()
{
    if (m_insertCookieStmt) {
        delete m_insertCookieStmt;
        m_insertCookieStmt = 0;
    }

    if (m_selectCookieStmt) {
        delete m_selectCookieStmt;
        m_selectCookieStmt = 0;
    }
    
    if (m_db) {
        delete m_db;
        m_db = 0;
    }

    m_cache->clear();
}

bool
CookieService::dumpDatabase(const String& dumpFile, PassRefPtr<DatabaseWatcher> watcher_)
{
	RefPtr<DatabaseWatcher> watcher = watcher_;

	watcher->dumpStarted(0);

	shutdown();

    int statusCode = Database::dump(m_databasePath.c_str(), dumpFile);

	init();

	watcher->dumpStopped(statusCode);

	return true;
}

bool CookieService::getDatabaseModInfo(time_t& mtime, int& numOpenMods) const
{
	mtime = 0;
	numOpenMods = 0;

	if (m_db == NULL)
		return false;

	bool success = false;

	struct stat sinfo;
    if (stat(m_databasePath.c_str(), &sinfo) == 0) {
		mtime = sinfo.st_mtime;
		success = true;
	}

	m_db->getTotalChanges(numOpenMods);

	return success;
}

bool
CookieService::restoreDatabase(const String& dumpFile, PassRefPtr<DatabaseWatcher> watcher_)
{
	RefPtr<DatabaseWatcher> watcher = watcher_;

	watcher->restoreStarted(0);

	shutdown();

    int statusCode = Database::restore(dumpFile, m_databasePath.c_str());

	init();

	watcher->restoreStopped(statusCode);

	return true;
}

bool
CookieService::isValidDatabase()
{
	SQLiteStatement integrityCheckStmt(*m_db, "PRAGMA integrity_check");
	int ret = integrityCheckStmt.prepareAndStep();
	if (ret != SQLITE_ROW || integrityCheckStmt.getColumnText(0) != "ok") {
		printf("@@@@@@@@@@@@@ Cookies database corrupted @@@@@@@@@@@@@@@@@@@@\n");
		shutdown();
		unlink(m_databasePath.c_str());
		init();		
		return false;
	}

    if (!m_db->tableExists("CookiesDbInfo") ||
        !m_db->tableExists("Cookies")) {
        return false;
    }

    if (databaseVersion() < sCookieDbVersion) {
        return false;
    }

    return true;
}

bool
CookieService::createDatabaseTables()
{
    if (!m_db->executeCommand("CREATE TABLE CookiesDbInfo "
                              "( key   TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE, "
                              "  value TEXT NOT NULL ON CONFLICT FAIL);")) {
        LOG_ERROR("Failed to create CookiesDbInfo table in cookies db: %s",
                  m_db->lastErrorMsg());
        return false;
    }
        
    if (!m_db->executeCommand("CREATE TABLE Cookies                            "
                              "(domain_head TEXT NOT NULL ON CONFLICT FAIL,    "
                              " domain_tail TEXT NOT NULL ON CONFLICT FAIL,    "
                              " path        TEXT NOT NULL ON CONFLICT FAIL,    "
                              " name        TEXT NOT NULL ON CONFLICT FAIL,    "
                              " value,                                         "
                              " expires     INTEGER NOT NULL ON CONFLICT FAIL, "
                              " secure      INTEGER NOT NULL ON CONFLICT FAIL, "
                              " UNIQUE (domain_head, domain_tail, path, name) ON CONFLICT REPLACE); ")) {
        LOG_ERROR("Failed to create Cookies table in cookies db: %s",
                  m_db->lastErrorMsg());
        return false;
    }

    if (!m_db->executeCommand("CREATE INDEX CookiesDomainTailIndex "
                              "ON Cookies (domain_tail)")) {
        LOG_ERROR("Failed to create CookiesDomainTailIndex table in cookies db: %s",
                  m_db->lastErrorMsg());
        return false;
    }        

    // Insert the db version
    if (!m_db->executeCommand(String("INSERT INTO CookiesDbInfo VALUES ('Version', ") +
                              String::number(sCookieDbVersion) +
                              String(");"))) {
        LOG_ERROR("Failed to insert Cookies Db version into cookies db: %s",
                  m_db->lastErrorMsg());
        return false;
    }

    return true;
}

int
CookieService::databaseVersion()
{
    if (!m_db)
        return -1;

    return SQLiteStatement(*m_db,
                           "SELECT value from CookiesDbInfo "
                           "WHERE key = 'Version';").getColumnInt(0);
}    

bool
CookieService::parseCookie(const String& cookieStr, const KURL& url,
                           String& domain, String& path,
                           String& name, String& value,
                           int64_t& expires, bool& isSecure)
{
    isSecure  = false;
    expires   = -1; // -1 indicates a session cookie

    LOG(Cookies, "Parse: %s\n", cookieStr.utf8().data());

    // the behavior of String::split is to return a single element in the
    // vector if the string does not contain the separator string. So
    // we should be OK here.
    Vector<String> strVec;
	cookieStr.split(String(";"), strVec);
    if (strVec.isEmpty()) {
        return false;
    }

    String str = strVec.at(0);
    int    splitPos;

    splitPos = str.find("=");
    if (splitPos == -1) {
#if 0
        LOG_ERROR("Failed to find = in the cookie. Invalid cookie\n");
        return false;
#else
        // Firefox behavior: no = means the whole string is the VALUE
        name  = "";
        value = str;
#endif       
    }
    else {
        name  = str.left(splitPos).stripWhiteSpace();
        value = str.substring(splitPos+1);
    }

    if (name.startsWith("$")) {
        // RFC 2109. Names beginning with $ are reserved and is illegal
        // for other purposes
        LOG_ERROR("Cookie name starts with $\n");
        return false;
    }
    
    for (unsigned int i = 1; i < strVec.size(); i++) {

        String key, val;

        str      = strVec.at(i);
        splitPos = str.find("=");

        if (splitPos == -1) {

            if (str.lower().stripWhiteSpace() == sKeySecure) {
                isSecure = true;
            }
            continue;
        }

        key = str.left(splitPos).lower().stripWhiteSpace();
        val = str.substring(splitPos+1).stripWhiteSpace();

        if (key == sKeyDomain) {

            if (!val.startsWith(".")) {
                val = "." + val;
            }

            domain = val;
        }
        else if (key == sKeyPath)
        {
            path = val;
        }
        else if (key == sKeyMaxAge)
        {
            bool   ok;
            time_t now;

            expires = val.toInt64(&ok);
            if (!ok)
            {
                LOG_ERROR("Invalid Max-Age entry in cookie\n");
                return false;
            }

            ::time(&now);

            expires += now;
        }
        else if (key == sKeyExpires)
        {
            if (val.isEmpty()) {
                // this is a session cookie
                expires = -1;
            }
            else {
                expires = parseDate(val.utf8().data());
                if (expires == -1) {
                    LOG_ERROR("Invalid expiry entry in cookie\n");
                    return false;
                }
            }
        }
    }

#if 0
    // Firefox allows nae to be empty
    if (name.isEmpty()) {
        return false;
    }
#endif    

    if (domain.isEmpty()) {
        domain = getDomainForUrl(url);
		if (domain.isEmpty()) {
            LOG_ERROR("Invalid host for url setting the cookie\n");
            return false;
        }
    }

    if (path.isEmpty()) {
        
        // RFC 2109: Defaults to the path of the request URL that
        // generated the Set-Cookie response, up to, but not including,
        // the right-most /

        path = url.path();
        if (path.isEmpty()) {
            LOG_ERROR("Invalid path for url setting the cookie\n");
            return false;
        }

        if (path != "/") {
            int lastSlashPos = path.reverseFind("/");
            if (lastSlashPos != -1) {
                path = path.substring(0, lastSlashPos);
            }
        }
    }
    else {

        // strip off the trailing slash from the specified path to be
        // consisten with above
        if (path != "/" && path[path.length() - 1] == '/')
            path = path.substring(0, path.length() - 1);
    }

    // RFC 2109: Cookie rejection criteria

#if 0    
    // Firefox allows this. So do we
    // 1. Path attribute should be a prefix of the request-URI
    if (!String(url.path()).startsWith(path)) {
        LOG_ERROR("Cookie path attribute is not a prefix of the request-URI: "
                  "urlpath = %s, path = %s\n",
                  url.path().ascii(), path.utf8().data());
        return false;
    }
#endif

    // 2. Values for domain attribute should contain embedded dots.
    //    We ignore the requirement for the domain attribute to
    //    start with a dot. Later we append the dot
    if (!domain.substring(1, domain.length() - 2).contains(".")) {
        LOG_ERROR("Cookie domain attribute contains no dots\n");
        return false;
    }
        
    // 3. Value of request-host should domain-match the Domain attribute
    if (!domainTailMatch(url.host(), domain)) {
        LOG_ERROR("Request-host does not domain-match the cookie domain attribute: "
                  "url host = %s, domain = %s\n",
                  (const char*)url.host().utf8().data(), (const char*)domain.utf8().data());
        return false;
    }

#if 0    
    // Firefox allows this and so do we
    // 4. If the request-host has the form HD, where D is the value
    //    of the Domain attribute and H is a string that contains
    //    one of or more dots.
    String H(url.host());
    H.truncate(H.length() - domain.length());
    if (H.contains(".")) {
        LOG_ERROR("Failed the RFC 2109 requirement that non-tail matching portion "
                  "of the request-host should have no dots\n");
        return false;
    }
#endif

    // Firefox does not allow \t in name or path. We do the same
    if (name.contains('\t') || path.contains('\t'))
        return false;

    LOG(Cookies, "Parsed: %s; %s; %s; %s; %lld, %d\n",
        name.utf8().data(), value.utf8().data(),
        domain.utf8().data(), path.utf8().data(),
        expires, isSecure);
    
    return true;
}

bool
CookieService::domainTailMatch(const String& a, const String& b)
{
    if (a.endsWith(b) || String("." + a) == b) {
        return true;
    }

    return false;
}

bool
CookieService::subPathMatch(const String& a, const String& b)
{
    // Is a shorter than b
    if (a.length() < b.length())
        return false;

    // Is 'b' a substring of 'a'
    if (memcmp(a.characters(), b.characters(), b.length() * sizeof(UChar)) != 0)
        return false;

    // Exact match?
    if (a.length() == b.length())
        return true;

    // ok, 'b' is a substring of 'a'. Either last character in 'a' SHOULD be '/' or
    // next character in 'a' SHOULD be '/'
    if (a[b.length()-1] == '/' || a[b.length()] == '/')
        return true;

    return false;
}

int
CookieService::secondDotFromTailPos(const String& str)
{
    const UChar* chars        = str.impl()->characters();
    const UChar* lastChar     = chars + str.length() - 1;
    int          numDots      = 0;
    
    while (lastChar != chars) {
        if (*lastChar == '.') {
            numDots++;
            if (numDots == 2) {
                break;
            }
        }

        lastChar--; 
    }

    return lastChar - chars;
}

void
CookieService::setCookies(const KURL& url, const String& cookieStr, const String& appId)
{
    if (!m_db) {
        return;
    }

	String cookie = cookieStr;

	// RFC2109: Maximum cookie length we are expected to handle is
	// 4096 bytes long. Otherwise trim. We set this to slightly larger
	if (cookie.length() > kMaxCookieLen) {
		cookie.truncate(kMaxCookieLen);
	}
	
    /*
    LOG(Cookies, "Cookie = %s\n", cookie.utf8().data());
    */
    
    String  domain;
    String  path;
    String  name;
    String  value;
    int64_t expires;
    bool    isSecure;
    time_t  now;

    if (!parseCookie(cookie, url, domain, path, name, value,
                     expires, isSecure)) {
        LOG_ERROR("Failed to parse cookie: %s\n", cookie.utf8().data());
        return;
    }

    ::time(&now);
    if (expires != -1 && expires <= now) {
        LOG(Cookies, "Cookie expired: %s, %s, %s\n",
            domain.utf8().data(),
            path.utf8().data(),
            name.utf8().data());

    
        int secondDotPos = secondDotFromTailPos(domain);

        String domainHead = domain.left(secondDotPos + 1);
        String domainTail = domain.substring(secondDotPos + 1);
        domainTail.append(appId); // see CookieJarPalm.cpp:getAppIdDomainTail
        
        SQLiteTransaction delSqlTrans(*m_db);

        SQLiteStatement delSqlStmt(*m_db,
                                "DELETE FROM Cookies WHERE "
                                " domain_tail = (?) AND domain_head = (?) AND "
                                " name = (?) AND path = (?);");
        delSqlStmt.prepare();

        delSqlTrans.begin();
                
        delSqlStmt.bindText(1, domainTail);
        delSqlStmt.bindText(2, domainHead);
        delSqlStmt.bindText(3, name);
        delSqlStmt.bindText(4, path);
        delSqlStmt.step();
        delSqlStmt.reset();
        delSqlStmt.finalize();
        delSqlTrans.commit();

        m_cache->delCookie(domainHead, domainTail, path, name);
        
        return;
    }
    
    int secondDotPos = secondDotFromTailPos(domain);

    String domainHead = domain.left(secondDotPos + 1);
    String domainTail = domain.substring(secondDotPos + 1);
    domainTail.append(appId); // see CookieJarPalm.cpp:getAppIdDomainTail

    m_insertCookieStmt->bindText(1, domainHead);
    m_insertCookieStmt->bindText(2, domainTail);
    m_insertCookieStmt->bindText(3, path);
    m_insertCookieStmt->bindText(4, name.isEmpty() ? "" : name);
    m_insertCookieStmt->bindText(5, value);
    m_insertCookieStmt->bindInt64(6,  expires);
    m_insertCookieStmt->bindInt64(7,  isSecure);

    if (m_insertCookieStmt->step() != SQLResultDone) {
        LOG_ERROR("Failed in inserting cookie into db: %s\n", m_db->lastErrorMsg());
    }

    m_insertCookieStmt->reset();

    m_cache->addCookie(domainHead, domainTail, path,
                       name, value,
                       isSecure, expires);
}

String
CookieService::cookies(const KURL& url, const String& appId)
{
    String         cookieStr;
    String         cookieDomain;
    String         cookieDomainHead;
    String         cookieDomainTail;
    String         cookieDomainTailWithAppId; // see CookieJarPalm.cpp:getAppIdDomainTail
    String         cookiePath;
    String         cookieName;
    String         cookieValue;
    int64_t        cookieExpires;
    bool           cookieIsSecure;
    int            sqlResult;
    bool           cookieInDb;
    Vector<String> expiredCookies;
    time_t         now;

    ::time(&now);
    
    if (!m_db) {
        return cookieStr;
    }

    String host = url.host();
    String path = url.path();

    // trim trailing dots from host
    if (host.endsWith(".")) {
        host = host.substring(0, host.length() - 1);
    }

    if (host.isEmpty() || path.isEmpty()) {
        return cookieStr;
    }

    /*
    LOG(Cookies, "Get Cookies for URL: %s, %s\n",
        host.utf8().data(), path.utf8().data());
    */
    
    int    secondDotPos = secondDotFromTailPos(host);
    String hostTail;
    if (secondDotPos == 0) {
        hostTail = host.substring(secondDotPos);
    }
    else {
        hostTail = host.substring(secondDotPos + 1);
    }
    hostTail.append(appId);

    // First check if it is in the cache
    CookieEntrySet* entrySet = m_cache->cookieEntrySet(hostTail);
    if (entrySet) {

        for (unsigned int i = 0; i < entrySet->m_entries.size(); i++) {
            CookieEntry* entry = entrySet->m_entries.at(i);

            cookieDomainHead = entry->m_domainHead;
            cookieDomainTail = entry->m_domainTail;
            cookieDomainTailWithAppId = entry->m_domainTail;
            cookieDomainTail.remove(cookieDomainTail.length() - appId.length(), appId.length());

            cookieDomain     = cookieDomainHead + cookieDomainTail;
            cookiePath       = entry->m_path;
            cookieName       = entry->m_name;
            cookieValue      = entry->m_value;
            cookieExpires    = entry->m_expires;
            cookieIsSecure   = entry->m_isSecure;

            if (cookieIsSecure && url.protocol() != "https") {
                LOG(Cookies, "Cookie is secure and protocol is not: %s, %s, %s\n",
                    cookieDomain.utf8().data(),
                    cookiePath.utf8().data(),
                    cookieName.utf8().data());
                continue;
            }

            if (cookieExpires != -1 && cookieExpires < now) {
                LOG(Cookies, "Cookie expired: %s, %s, %s\n",
                    cookieDomain.utf8().data(),
                    cookiePath.utf8().data(),
                    cookieName.utf8().data());
                expiredCookies.append(cookieDomainHead);
                expiredCookies.append(cookieDomainTailWithAppId);
                expiredCookies.append(cookiePath);
                expiredCookies.append(cookieName);
                continue;
            }
            
            if (domainTailMatch(host, cookieDomain)  && subPathMatch(path, cookiePath)) {
                /*
                  LOG(Cookies, "Cookie match: %s; %s; %s; %s; %lld, %d\n",
                  cookieDomain.utf8().data(), cookiePath.utf8().data(),
                  cookieName.utf8().data(), cookieValue.utf8().data(),
                  cookieExpires, cookieIsSecure);
                */
                if (!cookieStr.isEmpty()) {
                    cookieStr += "; ";
                }

                // Firefox behavior. If name is empty, only use the value for the cookie
                if (!cookieName.isEmpty()) {
                    cookieStr += cookieName;
                    cookieStr += "=";
                }
                cookieStr += cookieValue;
            }
        }

        return cookieStr;
    }

    
    m_selectCookieStmt->bindText(1, hostTail);

    cookieInDb = false;

    while (true) {

        sqlResult = m_selectCookieStmt->step();
        if (sqlResult == SQLResultDone)
            break;

        if (sqlResult != SQLResultRow)
            break;

        cookieInDb = true;
        
        cookieDomainHead = m_selectCookieStmt->getColumnText(0);
        cookieDomainTail = m_selectCookieStmt->getColumnText(1);
        cookieDomainTailWithAppId = m_selectCookieStmt->getColumnText(1);
        cookieDomainTail.remove(cookieDomainTail.length() - appId.length(), appId.length());
        cookieDomain     = cookieDomainHead + cookieDomainTail;
        cookiePath       = m_selectCookieStmt->getColumnText(2);
        cookieName       = m_selectCookieStmt->getColumnText(3);
        cookieValue      = m_selectCookieStmt->getColumnText(4);
        cookieExpires    = m_selectCookieStmt->getColumnInt64(5);
        cookieIsSecure   = m_selectCookieStmt->getColumnInt(6);

        m_cache->addCookie(cookieDomainHead, cookieDomainTailWithAppId,
                           cookiePath, cookieName, cookieValue,
                           cookieIsSecure, cookieExpires);
        
        if (cookieIsSecure && url.protocol() != "https") {
            LOG(Cookies, "Cookie is secure and protocol is not: %s, %s, %s\n",
                cookieDomain.utf8().data(),
                cookiePath.utf8().data(),
                cookieName.utf8().data());
            continue;
        }
        
        if (cookieExpires != -1 && cookieExpires < now) {
            LOG(Cookies, "Cookie expired: %s, %s, %s\n",
                cookieDomain.utf8().data(),
                cookiePath.utf8().data(),
                cookieName.utf8().data());
            expiredCookies.append(cookieDomainHead);
            expiredCookies.append(cookieDomainTailWithAppId);
            expiredCookies.append(cookiePath);
            expiredCookies.append(cookieName);
            continue;
        }
        
        if (domainTailMatch(host, cookieDomain)  && subPathMatch(path, cookiePath)) {
            /*
            LOG(Cookies, "Cookie match: %s; %s; %s; %s; %lld, %d\n",
                cookieDomain.utf8().data(), cookiePath.utf8().data(),
                cookieName.utf8().data(), cookieValue.utf8().data(),
                cookieExpires, cookieIsSecure);
            */
            
            if (!cookieStr.isEmpty()) {
                cookieStr += "; ";
            }

            cookieStr += cookieName;
            cookieStr += "=";
            cookieStr += cookieValue;
        }
    }

    m_selectCookieStmt->reset();

    if (!expiredCookies.isEmpty()) {
        SQLiteTransaction delSqlTrans(*m_db);

        SQLiteStatement delSqlStmt(*m_db,
                                "DELETE FROM Cookies WHERE "
                                " domain_tail = (?) AND domain_head = (?) AND "
                                " name = (?) AND path = (?);");
        delSqlStmt.prepare();

        delSqlTrans.begin();
                
        for (unsigned int i = 0; i < expiredCookies.size(); i += 4) {
            delSqlStmt.bindText(1, expiredCookies.at(i + 1));
            delSqlStmt.bindText(2, expiredCookies.at(i    ));
            delSqlStmt.bindText(3, expiredCookies.at(i + 2));
            delSqlStmt.bindText(4, expiredCookies.at(i + 3));
            delSqlStmt.step();
            delSqlStmt.reset();

            m_cache->delCookie(expiredCookies.at(i), expiredCookies.at(i + 1),
                               expiredCookies.at(i + 2), expiredCookies.at(i + 3));
        }

        delSqlStmt.finalize();

        delSqlTrans.commit();
    }

    if (!cookieInDb) {
        // to prevent hitting the db again for a non-existent cookie,
        // we insert a dummy cookie entry in the Cache
        m_cache->addDummyCookieEntrySet(hostTail);
    }
    
    return cookieStr;
}

bool
CookieService::cookiesEnabled()
{
    LOG(Cookies, "Cookies enabled: %s\n", m_db ? "true" : "false");

    if (!m_db) {
        return false;
    }

    return true;
}

// In the case of browserserver we want to clear cookies for all (webview) jars.
// This is most easily done by iterating through all open instances *and* modifying
// the file system.
void CookieService::clearAllCookieJars( )
{
	CookieServiceMap::iterator it;
	
	for( it=sCookieServices.begin(); it != sCookieServices.end(); ++it )
	{
		if( it->second->m_db )
			it->second->m_db->clearAllTables();
		it->second->shutdown();
	}
	
	// Now clear all the container files we created
	std::string path = PalmBrowserSettings()->cookieJarPath;
	std::string filename;
	size_t endpos = path.find_last_of("/");
	if( endpos != std::string::npos )
	{
		filename = path.substr( endpos+1 );
		path = path.substr( 0, endpos+1 );
	}
	
	// cookie databases are this "path" with appIds appended.
	if( DIR* dir = opendir(path.c_str()) )
	{
		struct dirent* entry = readdir(dir);
		while (entry) 
		{
			if( !strncmp( entry->d_name, filename.c_str(), filename.size() ) )
			{
				::unlink( std::string( path + std::string(entry->d_name)).c_str() );
			}
			
			entry = readdir(dir);
		}
		closedir(dir);
	}
	
	for( it=sCookieServices.begin(); it != sCookieServices.end(); ++it )
	{
		it->second->init();
	}
}

void CookieService::clear()
{
    LOG(Cookies, "Clearing cookies...\n");
    
	// quick thing to do here is to drop all tables in the db
	// shutdown the cookies service and then restart it
	if (m_db) {
		m_db->clearAllTables();
	}
	
	shutdown();
	init();
}

bool
CookieService::clearSessionCookies()
{
	LOG(Cookies, "Clearing all session cookies...\n");
	if (m_db == NULL)
		return false;

	SQLiteTransaction delSqlTrans(*m_db);

	SQLiteStatement delSqlStmt(*m_db, "DELETE FROM Cookies WHERE expires = -1;");
	delSqlStmt.prepare();

	delSqlTrans.begin();
			
	delSqlStmt.step();
	delSqlStmt.reset();
	delSqlStmt.finalize();
	delSqlTrans.commit();

	return true;
}

/**
 * Split the domain into a domain head and tail.
 *
 * @return true if successful.
 */
bool
CookieService::splitDomain(const String& domain, String& domainHead, String& domainTail)
{
    int secondDotPos = secondDotFromTailPos(domain);

    if ( secondDotPos > 0 ) {
        domainHead = domain.left(secondDotPos + 1);
        domainTail = domain.substring(secondDotPos + 1);
        return true;
    }
    else {
        domainHead = "";
        domainTail = "";
        return false;
    }
}

/**
 * Clear all cookies for the specified domain.
 *
 * @return true if successful.
 */
bool
CookieService::clearForDomain(const KURL& url)
{
    LOG(Cookies, "Clearing all cookies for '%s'...\n", url.string().utf8().data());
	if (!m_db) {
        return false;
    }

	String domain = getDomainForUrl(url);
	if (domain.isEmpty()) {
		LOG_ERROR("Invalid host for url deleting cookies\n");
		return false;
	}

    String domainHead, domainTail;
    if (!splitDomain(domain, domainHead, domainTail)) {
        LOG_ERROR("Domain not valid '%s'\n", domain.utf8().data());
    }

    SQLiteTransaction delSqlTrans(*m_db);

	SQLiteStatement delSqlStmt(*m_db,
                            "DELETE FROM Cookies WHERE domain_tail = (?) AND domain_head = (?);");

	delSqlStmt.prepare();

	delSqlTrans.begin();
			
	delSqlStmt.bindText(1, domainTail);
	delSqlStmt.bindText(2, domainHead);
	delSqlStmt.step();
	delSqlStmt.reset();
	delSqlStmt.finalize();
	delSqlTrans.commit();

	// To be more efficient we can remove cookies from the cache selectively
	// instead of clearing the whole cache, but this is simpler and doesn't
	// happen that often.
	m_cache->clear();

	return true;
}

/**
 * Return the domain for the given URL.
 *
 * @return the domain string. If empty then there was an error.
 */
String
CookieService::getDomainForUrl(const KURL& url)
{
    // RFC 2109: Defaults to request-host
	String domain = url.host();

	// trim trailing dots from host
    if (domain.endsWith(".")) {
        domain = domain.substring(0, domain.length() - 1);
    }

	if (domain.isEmpty()) {
		return "";
	}

	if (!domain.startsWith(".")) {
		domain = "." + domain;
	}

    return domain;
}

/**
 * Copy all cookies from the source domain to the destination domain.
 *
 * @return true if successful.
 */
bool
CookieService::copyCookiesToDomain(const KURL& srcUrl, const KURL& dstUrl)
{
    if (!m_db) {
        return false;
    }

    String srcDomain = getDomainForUrl(srcUrl);
    String dstDomain = getDomainForUrl(dstUrl);
   
    if (srcDomain.isEmpty() || dstDomain.isEmpty()) {
        LOG_ERROR("Invalid domain(s)\n");
        return false;
    }

    String srcDomainHead, srcDomainTail;
    if (!splitDomain(srcDomain, srcDomainHead, srcDomainTail)) {
        LOG_ERROR("Domain not valid '%s'\n", srcDomain.utf8().data());
        return false;
    }

    String dstDomainHead, dstDomainTail;
    if (!splitDomain(dstDomain, dstDomainHead, dstDomainTail)) {
        LOG_ERROR("Domain not valid '%s'\n", dstDomain.utf8().data());
        return false;
    }

    SQLiteTransaction delSqlTrans(*m_db);

	SQLiteStatement delSqlStmt(*m_db,
                            "INSERT INTO Cookies (domain_head, domain_tail, path, name, value, expires, secure) "
                            "SELECT (?), (?), path, name, value, expires, secure FROM Cookies "
                            "WHERE domain_head = (?) AND domain_tail = (?);");

	delSqlStmt.prepare();

	delSqlTrans.begin();
			
	delSqlStmt.bindText(1, dstDomainHead);
	delSqlStmt.bindText(2, dstDomainTail);
	delSqlStmt.bindText(3, srcDomainHead);
	delSqlStmt.bindText(4, srcDomainTail);
	delSqlStmt.step();
	delSqlStmt.reset();
	delSqlStmt.finalize();
	delSqlTrans.commit();

    m_cache->clear();   // Just in case the destination was in the cache.

    return true;
}
}
