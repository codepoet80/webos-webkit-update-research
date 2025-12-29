/* ============================================================
 * Date  : 2008-09-22
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#include <syslog.h>

#include <assert.h>
#include "config.h"
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include <math.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/file.h>

#if ENABLE(DOM_STORAGE)
#include "StorageNamespace.h"
#include "StorageArea.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "ApplicationCacheStorage.h"
#endif

#include "RuntimeEnabledFeatures.h"
#include "WebSocket.h"

#include "palmwebglobal.h"
#include "palmwebpageclient.h"
#include "palmwebpageprivate.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"

#include "webkitpalmsettings.h"
#include "webkitpalmstrings.h"
#include "CookieServicePalm.h"
#include "CookieJar.h"
#include "Cache.h"
#include "PageCache.h"
#include "DiskCachePalm.h"
#include "CString.h"
#include "CurlHandle.h"
#include "CurlHandlePool.h"
#include "FontCache.h"
#include "FontCatalogPG.h"
#include "SecurityOrigin.h"
#include "SQLiteStatement.h"
#include "EditorClientPalm.h"
#include <DOMWindow.h>
#include <Frame.h>
#include "PluginDatabase.h"

#include "lunaservice.h"
#include <pbnjson.hpp>

#if USE(V8)
	#include <v8.h>
	#include <v8-debug.h>
	#include "V8Proxy.h"
#else
	#include "runtime/InitializeThreading.h"
	#include "GCController.h"
#endif

#include "LunaResources.h"
#include "DatabaseTracker.h"
#include "Database.h"
#include "TextIndexer.h"
#include "Logging.h"

#include "weboseventreporter_webkit.h"
#if !defined(WEBOS_DESKTOP)
#include "sandbox.h"
#endif

using namespace WebCore;

// libsandbox requires that it's owner define this.
char** environ = NULL;

// Round a value up to the nearest MB size.
#define LOG_MB_BITS 20
#define MB_ROUND_UP(sz) (((sz >> LOG_MB_BITS) + 1) << LOG_MB_BITS)

namespace Palm
{
	
static int MeasureTotalRAM();
static void ReadSettingsFromSysService();
static bool sysServicePrefsCallback(LSHandle *lshandle, LSMessage *message, void *ctx);
static void explodeStlString(const std::string &inString, const std::string &separator, std::list<std::string>& outTokens );

#if USE(V8)

// #define USE_V8_IDLE_NOTIFICATION to enable idle notifications.
#define USE_V8_IDLE_NOTIFICATION

//============================================================================
// Idle/timed follow-up GC handler:
//
// Note: the current implementation chooses to not wait for idle.  Uncomment
// the #define GC_WAIT_FOR_IDLE to enable it.  Instead, the current preferred
// implementation is to do the follow-up GC based solely on a timer.
//
// The GC will be done by the v8 via its IdleNotification().

#undef GC_WAIT_FOR_IDLE
//#define GC_WAIT_FOR_IDLE

class IdleNotifier {
  public:
    IdleNotifier(GMainLoop *mainLoop,
                 gboolean (*callback)(IdleNotifier *, gpointer), 
                 gpointer arg,
                 unsigned int idleIntervalMs = DEFAULT_IDLE_INTERVAL_MS);
    ~IdleNotifier();

    void start(unsigned int intervalMs);
    void stop();
    void setIdleIntervalMs(unsigned int intervalMs) {
        m_idleIntervalMs = intervalMs;
    }

  private:
    void startIdle();
    void startTimeout();

    static gboolean handleIdle(gpointer arg);
    static gboolean handleTimeout(gpointer arg);

    static const unsigned int DEFAULT_IDLE_INTERVAL_MS = 1000;
#ifdef GC_WAIT_FOR_IDLE
	static const int MIN_DEBOUNCE_COUNT = 3;
	static const unsigned long MAX_JITTER_MS = 5;
#else
    // Since we're not waiting for the idle phase, set these to disable the
    // the idle jitter and debounce checks.
    // Note: a high jitter means that we'll tolerates anything i.e. no waiting.
	static const int MIN_DEBOUNCE_COUNT = 0;
	static const unsigned long MAX_JITTER_MS = 9999;
#endif

    GSource *m_source;
    GMainLoop *m_mainLoop;
    gboolean (*m_callback)(IdleNotifier *, gpointer);
    gpointer m_arg;
    unsigned long m_idleIntervalMs;
    unsigned long m_minIdleIntervalMs;

    // For debouncing jitter between idle calls:
    unsigned long m_startMs;
    unsigned long m_previousDebounceMs;
    int m_debounceCount;
};

#ifdef USE_V8_IDLE_NOTIFICATION
static IdleNotifier *gIdleNotifier = NULL;
static gboolean idleGCCallback(IdleNotifier *notifier, gpointer);
#endif

#endif // USE(V8)

static GMainLoop* gMainLoop = NULL;
static WebKitEventListener* gEventListener = NULL;
static std::string gLocaleString = "en-US";
static std::string gLocaleCollatorString = "en-US";
static std::string gUaProf;
static std::string gSwUpdateLocaleFile = "/var/lib/software/system_settings.info";
static bool gIsSystemSuspending = false;
static bool gIsInitialized = false;	///< Is WebKit initialized?
static std::string gActiveApplicationId;

// If we ever want to migrate the "external databases" i.e. "ext:dbName"
// from one place to another then we can specify a new path here. For
// the moment we're leaving them the same.
#if defined(WEBOS_DESKTOP)
static const char* kOldExtDirPath = "/tmp/app-storage";
static const char* kNewExtDirPath = kOldExtDirPath;
#else		
static const char* kOldExtDirPath = "/media/internal/.app-storage";
static const char* kNewExtDirPath = kOldExtDirPath;
#endif


void WebGlobal::initUsingSnapshot(GMainLoop* loop, const char* snapshotFile, WebKitEventListener* eventListener )
{
    if (gIsInitialized)
        return;
    
	WebGlobal::init( loop, eventListener );
	
	if( snapshotFile )
	{
		// Attempt to load the file:
		gchar* buf = 0;
		gsize len = 0;
		
		if( TRUE == g_file_get_contents( snapshotFile, &buf, &len, 0 ) )
		{
			printf(" Using snapshot %s for v8.\n", snapshotFile );
			if( !v8::V8::SnapshotLoad( buf, len ) )
			{
				g_unlink( snapshotFile );
			}
			free( buf );
		}
		else
		{
			// Generate the snapshot.
			printf(" Snapshot %s not found, generating.\n", snapshotFile );
			v8::Persistent<v8::Context> context = v8::Context::New();
			context->Enter();
			void* save_buf=0;
			int save_len=0;
			if( v8::V8::SnapshotSave( save_buf, save_len ) )
			{
				printf(" --> saved context is %d k\n", save_len / 1024 );
				g_unlink( snapshotFile );
				g_file_set_contents( snapshotFile, (const gchar*)save_buf, (gssize)save_len, 0 );
				v8::V8::SnapshotFreeBuffer(save_buf);
			}
			context->Exit();
			context.Dispose();
		}
		
	}
}


void WebGlobal::init(GMainLoop* loop,WebKitEventListener* eventListener )
{
    if (gIsInitialized)
        return;

    gIsInitialized = true;

	// set our global event loop
    gMainLoop = loop;

	gEventListener = eventListener;
	
	WTF::initializeThreading();
    WebCore::InitializeLoggingChannelsIfNecessary();

    // Set up the JS heap paramters based on physical memory. This can be
    // overridden in the browser*.conf files.
    int physRam = MeasureTotalRAM();
#if USE(V8)
    int jsNewSpaceSizeBytes = 4*1024*1024; // 4MB

    // The JS old space heap size is calculated to be 50% of the total
    // physical RAM on disk (rounded to the nearest MB).  It should be 64MB
    // at a minimum, however.  We also cap it at 1GB because we don't want
    // to hog up too much of the 4GB address space of a 32bit system.
    int jsOldSpaceSizeBytes =
        std::max(64*1024*1024 /* 64MB */,
                 MB_ROUND_UP((int)((physRam*1024)*(0.50))));

    const int oneGB = 1024*1024*1024;
    if (jsOldSpaceSizeBytes > oneGB)
        jsOldSpaceSizeBytes = oneGB;

    if (physRam < 220000) {
        jsNewSpaceSizeBytes = 2*1024*1024;
    }

    v8::ResourceConstraints constraints;
    constraints.set_max_young_space_size(jsNewSpaceSizeBytes);
    constraints.set_max_old_space_size(jsOldSpaceSizeBytes);
    constraints.set_stack_limit(0);
#endif

	// Load our webkit-palm conf settings file.
	PalmBrowserSettings();
	
#if USE(V8)
	// V8 limit overrides -- only if specified in the .conf file.
    int overriddenYoungGenBytes = ::PalmBrowserSettings()->v8maxYoungSpaceBytes;
	if (overriddenYoungGenBytes > 0) {
        constraints.set_max_young_space_size(overriddenYoungGenBytes);
    }

    int overriddenOldGenBytes = ::PalmBrowserSettings()->v8maxOldSpaceBytes;
    if (overriddenOldGenBytes > 0) {
        constraints.set_max_old_space_size(overriddenOldGenBytes);
    }

    uint32_t stackSize = ::PalmBrowserSettings()->v8StackLimitBytes;
	if (stackSize) {
        // For the normal stack which moves from high to low memory when frames
        // are pushed, we can compute the limit as stackSize bytes below the
        // the address of a stack variable (e.g. &stackSize) as an approximation
        // of the start of the stack (we're assuming that we haven't pushed a lot
        // of frames yet).
        uint32_t *stackLimit = &stackSize - (stackSize / sizeof(uint32_t));
        if (stackSize == 0) {
            stackLimit = NULL; // If size is not specified, tell v8 to use its default.
        }
        constraints.set_stack_limit(stackLimit);
    }

    // The constraints need to be set before initializing v8.  During the
    // initialization, the contraints will be baked in, and cannot be changed
    // thereafter:
    v8::SetResourceConstraints( &constraints );
	
	v8::V8::SetIdleGCRequestCallback(&WebGlobal::requestIdleGC);

	v8::V8::SetCanReadFileCallback(&WebGlobal::canV8ContextReadFile);

	if (!v8::V8::Initialize()) {
		// an error occurred in initialize
		syslog(LOG_USER | LOG_EMERG, "%s\n",
			"Failed to allocate memory to initialize the javascript heap!  Terminating.");
		exit(-1);
	}
	
#endif
	
	// Set Cache Policies (from Cache.h)
    //  - minDeadBytes: The maximum number of bytes that dead resources should consume when the cache is under pressure.
    //  - maxDeadBytes: The maximum number of bytes that dead resources should consume when the cache is not under pressure.
    //  - totalBytes: The maximum number of bytes that the cache should consume overall.	
    cache()->setCapacities( 
		::PalmBrowserSettings()->memCacheMinDeadBytes,
		::PalmBrowserSettings()->memCacheMaxDeadBytes,
		::PalmBrowserSettings()->memCacheTotalBytes );
    pageCache()->setCapacity( ::PalmBrowserSettings()->pageCacheCapacity );

	// Webkit4 PageGroup::setShouldTrackVisitedLinks(true);	
	
	FontCatalog::instance( ::PalmBrowserSettings()->defaultSansSerifFontFamilyName );

    // Initialize the cookie service
    WebCore::CookieService* cookieService = WebCore::CookieService::sharedInstance();
    if (cookieService)
        cookieService->init();

	String oldDatabasePath;
	String newDatabasePath;

	const char* dbPath = ::PalmBrowserSettings()->appDatabasePath;
	const char* delim = strchr(dbPath, ';');
	if (delim == NULL) {
		newDatabasePath = String(dbPath);
	}
	else {
		newDatabasePath = String(delim + 1);
		oldDatabasePath = String(dbPath, delim-dbPath);

		// We're migrating from one tracker origin to another. When we do this we
		// also need to specify a different "external database" path because if
		// we don't then we may have a collision of database file names. P.S. an
		// empty new path means don't put db's with the rest of the db's.
		if (strcmp(kOldExtDirPath, kNewExtDirPath) == 0 && kNewExtDirPath[0] != '\0') {
			g_error("Migrating from one tracker db to another but ext db paths match");
			exit(EBADF);
		}
	}


	// The two processes which use WebKit (BrowserServer & LunaSysMgr)
	// share the same tracker database (probably not a good idea). 
	// However, sysmgr would (ignoring apps containing iframes) only create
	// db's for the "file" scheme and BrowserServer would only create them
	// for "http/https". We use the origin condition to "tease" them apart
	// putting them into separate tracker databases.
	String fileOriginsCondition;
	if (::PalmBrowserSettings()->runningInBrowserServer) {
		fileOriginsCondition = "http%";
	}
	else {
		fileOriginsCondition = "file_%";
	}

	bool needToMoveDbs = !oldDatabasePath.isEmpty() &&
						DatabaseTracker::numOriginsInTracker(oldDatabasePath, fileOriginsCondition) > 0;

    String databasePaths;
	if (needToMoveDbs) {
		
		if (DatabaseTracker::moveTrackerDatabases(oldDatabasePath, kOldExtDirPath,
					fileOriginsCondition, newDatabasePath, kNewExtDirPath)) {
			if (eventListener) {
				eventListener->dbMoveStatus(0 /*successful*/);
			}
            databasePaths = newDatabasePath + DB_PATH_SEPARATOR + String(kNewExtDirPath);
		}
		else if (eventListener) {
			eventListener->dbMoveStatus(-1 /*generic failure*/);
            databasePaths = oldDatabasePath + DB_PATH_SEPARATOR + String(kOldExtDirPath);
		}
	}
	else {
        databasePaths = newDatabasePath + DB_PATH_SEPARATOR + String(kNewExtDirPath);
    }

    WebCore::DatabaseTracker::initializeTracker(databasePaths);
    DatabaseTracker& tracker =  WebCore::DatabaseTracker::tracker();
    tracker.setTrackerQuota( ::PalmBrowserSettings()->appDatabaseQuota );

#if USE(V8)	
	// In order to implement a tracker-wide quota mechanism we need to force the tracker
	// to load all of it's origins. This is normally done lazily as the databases are
	// accessed.
	{
		Vector<RefPtr<SecurityOrigin> >trackerDbOrigins;
		tracker.origins(trackerDbOrigins, fileOriginsCondition);

		unsigned long long total(0LL);
		for (unsigned i = 0; i < trackerDbOrigins.size(); ++i) {
			// Calling usageForOrigin will load the origin into the quota manager
			// and all of it's databases.
			total += tracker.usageForOrigin(trackerDbOrigins[i].get());
		}
		//g_message("Current database usage: %llu bytes", total);
	}

	// Enable + start debugger agent from .conf file. This is a convinience,
	// normally the debugger would be started up by sysmgr calling the API fn.
	if( PalmBrowserSettings()->debuggerEnable ) {		
		WebGlobal::setJavaScriptFlags( "--debugger_agent" );
		v8::Debug::EnableAgent("webOS.sysmgr",5858);
	}

    // Make sure that we are in sync with the v8 values for these enums:
    assert(int(AVOID_ALL) == int(v8::V8::AVOID_ALL));
    assert(int(AVOID_OLDGEN) == int(v8::V8::AVOID_OLDGEN));

#ifdef USE_V8_IDLE_NOTIFICATION
    // Setup the idle notifier for GCs:
    gIdleNotifier = new IdleNotifier(loop, idleGCCallback, NULL, 1000);
#endif
#endif	


#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (::PalmBrowserSettings()->enableAppCache) {
    	WebCore::cacheStorage().setCacheDirectory(::PalmBrowserSettings()->appCacheDbPath);
    	WebCore::cacheStorage().setMaximumSize(::PalmBrowserSettings()->appCacheDbMaxSize);
    }
#endif

#ifndef WEBOS_DESKTOP
    webOS::Reporter::init(loop);
#endif

#if ENABLE(WEB_SOCKETS)
	WebCore::WebSocket::setIsAvailable(true);
#endif

#if ENABLE(DOM_STORAGE)
	WebCore::RuntimeEnabledFeatures::setLocalStorageEnabled(true);
#endif

    webOS::Reporter::setVerbosity(PalmBrowserSettings()->profilerVerbosity);

	// Read customization from system service. This is here because it needs to be both in browserserver
	// and in sysmgr.
	ReadSettingsFromSysService();
	
	// Apply any settings that were waiting on webkit to fully initialize
	WebKitPalmApplySettings();
}

GMainLoop* WebGlobal::mainLoop()
{
	return gMainLoop;    
}

void WebGlobal::setCookies(const char* url, const char* cookie)
{
    WebCore::KURL   _url(ParsedURLString,url);
    WTF::String _cookie(cookie);

    WebCore::setCookies(0, _url, _cookie);    
}

std::string WebGlobal::cookies(const char* url)
{
    WebCore::KURL   _url(ParsedURLString,url);
    WTF::String _cookie;

    _cookie = WebCore::cookies(0, _url);
    if (_cookie.isEmpty()) {
        return 0;
    }

    return _cookie.utf8().data();
}

void WebGlobal::clearFontCache()
{
	fontCache()->purgeInactiveFontData();	
}

/*
 * deleteLocalStorageForDomain
 *
 * called from deleteDatabasesForDomain when application is removed
 * to delete application's local storage db
 */
static void deleteLocalStorageForDomain(PassRefPtr<SecurityOrigin> pOrigin)
{
#if ENABLE(DOM_STORAGE)
    RefPtr<SecurityOrigin> origin = pOrigin;
    RefPtr<StorageNamespace> localStorage = StorageNamespace::localStorageNamespaceIfExists(PalmBrowserSettings()->localStoragePath);
    if (localStorage && localStorage->localStorageAreaInUse(origin->databaseIdentifier()))
    {
    	g_message("deleteLocalStorageForDomain local storage is in use, scheduling a clear");
        // it's not safe to delete the db file while db is in use, so just schedule a clear
    	// clear will delete all records from the db. Should never happen since db can't be in use
    	// when app is getting deleted.
        RefPtr<StorageArea> storageArea = localStorage->storageArea(origin);
        storageArea->clear();
        return;
    }

    // delete local storage db file
    String dbPath(PalmBrowserSettings()->localStoragePath);
    if (!dbPath.endsWith("/")) {
        dbPath.append("/");
    }
    dbPath.append(origin->databaseIdentifier() + ".localstorage");

    g_message("deleteLocalStorageForDomain final dbPath %s", dbPath.utf8().data());
    if (remove(dbPath.utf8().data())) {
        syslog(LOG_ERR, "Failed to remove localStorage db %s", dbPath.utf8().data());
    }

#endif
}

/*
 * clearLocalStorage
 *
 * delete all local storage dbs found under PalmBrowserSettings()->localStoragePath
 * This method will delete only dbs created from the BrowserServer (name starts with "http")
 * This method will not delete any dbs currently in use with StorageNamespace
 */
void WebGlobal::clearLocalStorage() {
#if ENABLE(DOM_STORAGE)
    const char* start = "http";
    const char* end = ".localstorage";
    int startLen = strlen(start);
    int endLen = strlen(end);
    char dbPath[kMaxFilePath + 1];
    struct dirent* dirEntry;
    char* localStorageDirPath = PalmBrowserSettings()->localStoragePath;
    DIR* locStDir = opendir(localStorageDirPath);

    if (!locStDir)
    	return;

    RefPtr<StorageNamespace> localStorage = StorageNamespace::localStorageNamespaceIfExists(localStorageDirPath);

    strncpy(dbPath, localStorageDirPath, strlen(localStorageDirPath));
    if (dbPath[strlen(dbPath) - 1] != '/') {
        strcat(dbPath, "/");
    }
    char *dbPathFilename = dbPath + strlen(dbPath);

    while ((dirEntry = readdir(locStDir)) != NULL) {
        char* lastDot = strrchr(dirEntry->d_name, '.');

        if (lastDot && !strncmp(dirEntry->d_name, start, startLen) &&
            !strncmp(lastDot, end, endLen)) {

            if (localStorage) {
                String dbName(dirEntry->d_name, lastDot - dirEntry->d_name);

                if (localStorage->localStorageAreaInUse(dbName)) {
                    // don't delete dbs that are currently in use
                    continue;
                }
            }

            // delete local storage db
            strncpy(dbPathFilename, dirEntry->d_name, strlen(dirEntry->d_name));

            if (remove(dbPath)) {
                syslog(LOG_ERR, "Failed to remove localStorage db %s", dbPath);
            }
        }
    }

    closedir(locStDir);
#endif
}

void WebGlobal::clearCache()
{
	// Disk cache
	diskCache()->clear();
	
	// Font Cache
	fontCache()->purgeInactiveFontData();

    // Clear local storage
    clearLocalStorage();

    // Clear app cache
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (::PalmBrowserSettings()->enableAppCache) {
    	WebCore::cacheStorage().empty();
    }
#endif
}

void WebGlobal::clearDOMCache()
{
	cache()->setDisabled(true);
	cache()->setDisabled(false);
}

// This is no longer supported because display list was removed
void WebGlobal::clearDiskImageCache()
{

}

void WebGlobal::clearCookies()
{
    WebCore::CookieService* cookieService = WebCore::CookieService::sharedInstance();
    if (cookieService) {
        cookieService->clear();
    }    

    if( PalmBrowserSettings()->runningInBrowserServer ) {
    	WebCore::CookieService::clearAllCookieJars();
    }

    // In the future, when other plugins support the ClearSites NPAPI,
    // we should walk through all the plugins in the database. however,
    // since flash is the only plugin that currently supports ClearSites,
    // let's not unnecessarily spin our wheels with the other plugins
    PluginDatabase* pluginDatabase = PluginDatabase::installedPlugins();
    if (pluginDatabase) {
        PluginPackage* flashPlugin = pluginDatabase->pluginForMIMEType(pluginDatabase->MIMETypeForExtension("swf"));
        if (flashPlugin)
            flashPlugin->clearSiteData();
    }
}

void WebGlobal::resetDateCache()
{
#if USE(V8)
    V8Proxy *proxy = V8Proxy::getProxies();
    while (proxy) {
        if (proxy->windowShell()->isContextInitialized()) {
            v8::HandleScope handleScope;
            v8::Handle<v8::Context> context = proxy->context();
            context->Enter();
            v8::Date::ResetCache();
            context->Exit();
        }
        proxy = proxy->next();
    }
#endif
}


static void garbageCollectAndPruneCallback(void*)
{
#if USE(V8)	
	v8::V8::CollectAllGarbage();
#endif	
	cache()->prune();
}

void WebGlobal::garbageCollectNow()
{
#if USE(V8)
	WTF::callOnMainThread( garbageCollectAndPruneCallback, 0 );
#else	
	// TODO JSC
#endif
}

void WebGlobal::garbageCollectSoon()
{
#if USE(V8)
	WTF::callOnMainThread( garbageCollectAndPruneCallback, 0 );
#else	
	// TODO JSC
#endif
}

#if USE(V8)

#ifdef USE_V8_IDLE_NOTIFICATION

//============================================================================
// Idle/timed follow-up GC handler:

IdleNotifier::IdleNotifier(GMainLoop *mainLoop,
                           gboolean (*callback)(IdleNotifier *, gpointer),
                           gpointer arg, unsigned int idleIntervalMs)
    : m_source(NULL),
      m_mainLoop(mainLoop),
      m_callback(callback),
      m_arg(arg),
      m_idleIntervalMs(idleIntervalMs),
	  m_startMs(0),
      m_previousDebounceMs(0),
      m_debounceCount(0)
{
}

IdleNotifier::~IdleNotifier() {
    stop();
}

void IdleNotifier::start(unsigned int intervalMs) {
    bool haveNewStart = true;
    if (m_startMs) {
        haveNewStart = false;
        unsigned long current_ms = palm_monotonic_time_ms();
        unsigned long expired_ms = current_ms - m_startMs;
        unsigned long remaining_ms = m_idleIntervalMs - expired_ms;

        // If the timer in progress has remaining time that is more than
        // the new amount of requested time, then just stay on the existing
        // timer.  Otherwise, start a timer with the new time limit instead:
        if (remaining_ms < intervalMs) {
            haveNewStart = true;
        }
    }
    if (haveNewStart) {
        stop();  // Stop the existing timer if needed.
        setIdleIntervalMs(intervalMs); // Set the new time limit.
        startTimeout(); // Start it.
    }
}

void IdleNotifier::startIdle() {
    m_previousDebounceMs = palm_monotonic_time_ms();
#ifdef GC_WAIT_FOR_IDLE
    m_source = g_idle_source_new();
    g_source_set_priority(m_source, G_PRIORITY_LOW);
    g_source_set_callback(m_source, handleIdle, this, NULL);
    g_source_attach(m_source, g_main_loop_get_context(m_mainLoop));
#else
    handleIdle(this);
#endif
}

void IdleNotifier::startTimeout() {
    m_startMs = palm_monotonic_time_ms(); // Record the start time.
    m_source = g_timeout_source_new(m_idleIntervalMs);
    g_source_set_priority(m_source, G_PRIORITY_LOW);
    g_source_set_callback(m_source, handleTimeout, this, NULL);
    g_source_attach(m_source, g_main_loop_get_context(m_mainLoop));
}

void IdleNotifier::stop() {
    if (m_source) {
        g_source_destroy(m_source);
        g_source_unref(m_source);
        m_source = NULL;
        m_startMs = 0; // Record the start time.
    }
}

gboolean IdleNotifier::handleIdle(gpointer arg) {
    IdleNotifier *notifier = (IdleNotifier *)arg;
    gboolean needToContinue = TRUE;
    unsigned long currentMs = palm_monotonic_time_ms();

    notifier->stop(); // Stop the existing phase.
	if ((currentMs - notifier->m_previousDebounceMs) < MAX_JITTER_MS) {
        notifier->m_previousDebounceMs = currentMs;
        notifier->m_debounceCount++;

		if (notifier->m_debounceCount >= MIN_DEBOUNCE_COUNT) {

            // Call the callback:
            needToContinue = notifier->m_callback(notifier, notifier->m_arg);
            if (needToContinue) {
                notifier->startTimeout(); // Start the timer callback phase.
            }
        }
        return needToContinue;
    }

    // Failed the debounce.  Start over:
    notifier->m_previousDebounceMs = currentMs;
	notifier->m_debounceCount = 0;

    return needToContinue;
}

gboolean IdleNotifier::handleTimeout(gpointer arg) {
    IdleNotifier *notifier = (IdleNotifier *)arg;

    notifier->stop(); // Stop the existing phase.
    notifier->startIdle(); // Start the idle callback phase.

    return FALSE;
}

//============================================================================
// Callback from the idle notifier to trigger GCs:

static gboolean idleGCCallback(IdleNotifier *notifier, gpointer) {
    // Once we've fired for the first time, speed up the rate to once every 1s:
    notifier->setIdleIntervalMs(1000);
    bool done = WebGlobal::notifyIdle();
    if (done) {
        // When we're done GCing, then there may be an opportunity to reclaim
        // even more memory from the caches.  Go prune unused entries:
        cache()->prune();        

        // The idle handler has indicated that it's done processing this
        // idle cycle.  Hence, we can stop the notifier.
        notifier->stop();
        return FALSE; // No need to continue.
    }
    // Unlike the initial timeout of 5s, we want the next idle callback in 1s:
    return TRUE; // Yes, need to continue.
}

#endif // USE_V8_IDLE_NOTIFICATION


//============================================================================
// Wrappers around the avoid GC APIs to setup idle/timed follow-up GCs:

static void doAvoidGC(v8::V8::GCAvoidType type, unsigned long timeoutInMs)
{
	v8::V8::AvoidGC(type, timeoutInMs);
#ifdef USE_V8_IDLE_NOTIFICATION
    if (gIdleNotifier) {
        // Note: we do not stop the exisiting timeout first because this
        // new one may be satisfied by the existing one.  The notifier will
        // figure it all out.
        gIdleNotifier->start(v8::V8::GetGCAvoidTimeMs());
    }
#endif
}

static void doReallowGC(v8::V8::GCAvoidType type)
{
	v8::V8::ReallowGC(type);
}

static void doResetGCAvoids()
{
	v8::V8::ResetGCAvoids();
#ifdef USE_V8_IDLE_NOTIFICATION
    if (gIdleNotifier) {
        gIdleNotifier->stop();      // Stop everything.
        gIdleNotifier->start(5000); // Start a new one.
    }
#endif
}

//============================================================================
// Callbacks to be called from the webkit main thread:

#define AVOID_ARG(type_, timeout_) \
    ((void *)((type_ & 0xff) | (timeout_ << 8)))
#define AVOID_ARG_TYPE(arg)    v8::V8::GCAvoidType((unsigned long)(arg) & 0xff)
#define AVOID_ARG_TIMEOUT(arg) ((unsigned long)(arg) >> 8)

static void avoidGCCallback(void* arg)
{
	doAvoidGC(AVOID_ARG_TYPE(arg), AVOID_ARG_TIMEOUT(arg));
}

static void reallowGCCallback(void* arg)
{
	doReallowGC(AVOID_ARG_TYPE(arg));
}

static void resetGCAvoidsCallback(void*)
{
	doResetGCAvoids();
}
#endif // USE(V8)

//============================================================================
// Exposed WebGlobal avoid GC APIs:

void WebGlobal::avoidGC(GCAvoidType type, unsigned long timeoutInMs)
{
#if USE(V8)
    if (WTF::isMainThread()) {
        doAvoidGC(v8::V8::GCAvoidType(type), timeoutInMs);
    } else {
        WTF::callOnMainThread(avoidGCCallback, AVOID_ARG(type, timeoutInMs));
    }
#endif
}

void WebGlobal::reallowGC(GCAvoidType type)
{
#if USE(V8)
    if (WTF::isMainThread()) {
        doReallowGC(v8::V8::GCAvoidType(type));
    } else {
        WTF::callOnMainThread(reallowGCCallback, AVOID_ARG(type, 0));
    }
#endif
}

void WebGlobal::resetGCAvoids()
{
#if USE(V8)
    if (WTF::isMainThread()) {
        doResetGCAvoids();
    } else {
        WTF::callOnMainThread(resetGCAvoidsCallback, 0);
    }
#endif
}

bool WebGlobal::notifyIdle() {
#if USE(V8)
  // Grab a proxy, any proxy:
  V8Proxy *proxy = V8Proxy::getProxies();
  while (proxy && !proxy->windowShell()->isContextInitialized()) {
    proxy = proxy->next();
  }
  if (!proxy) {
    return true; // done.
  }

  // Enter a v8 context before calling the idle notification so that it can
  // call JS functions if needed:
  v8::HandleScope scope;
  v8::Handle<v8::Context> context = proxy->context();
  v8::Context::Scope context_scope(context);

  return v8::V8::IdleNotification();
#else
  return true;
#endif
}


void WebGlobal::requestIdleGC() {
#if USE(V8) && defined(USE_V8_IDLE_NOTIFICATION)
    if (gIdleNotifier) {
        // Note: we do not stop the exisiting timeout first because this
        // new one may be satisfied by the existing one.  The notifier will
        // figure it all out.
        gIdleNotifier->start(7000);
    }
#endif
}

static const Palm::WebFrame* getContextWebFrame(const v8::Context* context) {
    v8::Handle<v8::Context> ctxt(const_cast<v8::Context*>(context));
    WebCore::DOMWindow* window = V8Proxy::retrieveWindow(ctxt);
	if (!window)
		return 0;
	WebCore::Frame* frame = window->frame();
	if (!frame)
		return 0;
    return kit(frame);
}

static const char* getContextAppId(const v8::Context* context) {
    const Palm::WebFrame* frame = getContextWebFrame(context);
    if (frame) {
        Palm::WebFrameClient* client = frame->client();
        if (client)
            return client->getIdentifier();
    }
    return 0;
}

/**
 * Determine if a v8 context is allowed to access (read) a given file.
 */
bool WebGlobal::canV8ContextReadFile(const v8::Context* context, const char* fileName) {
#if USE(V8)
    // PalmGetResource isn't registered by v8 when running in BrowserServer so
    // this theoretically shouldn't happen.
    ASSERT(!PalmBrowserSettings()->runningInBrowserServer);
    const char* appId = getContextAppId(context);
    if (appId) {
        char* strippedAppId = strdup(appId);
        char* space = strchr(strippedAppId, ' ');
        if (space) {
            *space = '\0'; // Strip the space and processID parts.
        }
#if defined(WEBOS_DESKTOP)
        bool bCanAccess = true;
#else
        bool bCanAccess = SBIsPathAllowed(fileName, strippedAppId, SB_READ);
#endif
        if (!bCanAccess) {
            g_warning("\"%s\" DENIED access to \"%s\".", strippedAppId, fileName);
        }
        free(strippedAppId); // Free the strdup'ed string.
        return bCanAccess;
    }
    else {
        g_warning("Cannot find appId for context. Access to \"%s\" DENIED.", fileName);
        return false;
    }
#else
    return true;
#endif
}


bool WebGlobal::notifySleep() {
#if USE(V8)
  // Grab a proxy, any proxy:
  V8Proxy *proxy = V8Proxy::getProxies();
  while (proxy && !proxy->windowShell()->isContextInitialized()) {
    proxy = proxy->next();
  }
  if (!proxy) {
    return true; // done.
  }

  // Enter a v8 context before calling the sleep notification so that it can
  // call JS functions if needed:
  v8::HandleScope scope;
  v8::Handle<v8::Context> context = proxy->context();
  v8::Context::Scope context_scope(context);

  return v8::V8::SleepNotification();
#else
  return true;
#endif
}


bool WebGlobal::notifyWake() {
#if USE(V8)
  // Currently, we know that v8's implementation of WakeNotification does not
  // execute any JS code.  So, don't waste any effort to set up the context.  
  return v8::V8::WakeNotification();
#else
  return true;
#endif
}


void WebGlobal::notifyLowMemory() {
#if USE(V8)
  // Grab a proxy, any proxy:
  V8Proxy *proxy = V8Proxy::getProxies();
  while (proxy && !proxy->windowShell()->isContextInitialized()) {
    proxy = proxy->next();
  }
  if (!proxy) {
    return; // done.
  }

  // Enter a v8 context before calling the notification so that it can
  // call JS functions if needed:
  v8::HandleScope scope;
  v8::Handle<v8::Context> context = proxy->context();
  v8::Context::Scope context_scope(context);

  v8::V8::LowMemoryNotification();
#endif
}


bool WebGlobal::handleSignal(int sig, siginfo_t *info, void *context) {
#if USE(V8)
    // If the crash is in the webkit thread, then check if v8 will hanble
    // it.  Otherwise, indicate that we failed to handle it (i.e. return
    // false),
    if (g_thread_self() == V8Proxy::v8thread()) {
        return v8::V8::HandleSignal(sig, info, context);
    }
    return false;
#endif
}

void WebGlobal::reportCrashDiagnostics(int sig, siginfo_t *info, void *context,
                                       int logfileFD, bool doVerboseCrashLogging,
                                       volatile bool *hasCrashedInHandler) {
#if USE(V8)
    v8::V8::ReportCrashDiagnostics(sig, info, context, logfileFD,
                                   g_thread_self() == V8Proxy::v8thread(),
                                   doVerboseCrashLogging, hasCrashedInHandler);
#endif
}


std::string WebGlobal::runTextIndexerOnHtml( const std::string& inHtml, unsigned int flags )
{
    return WebCore::RunIndexerOnUtf8(inHtml, flags);
}

std::string WebGlobal::version()
{
#if USE(V8)	
	#if defined(__arm__)
		return "Webkit4/V8; device";
	#else
		return "Webkit4/V8; desktop";
	#endif
#else
	#if defined(__arm__)
		return "Webkit4/JSC; device";
	#else
		return "Webkit4/JSC; desktop";
	#endif
#endif
}

void WebGlobal::addAppendedHTTPHeader( const char* name, const char* value )
{
	WebCore::CurlHandle::addAppendedHTTPHeader( name, value );
}

/**
 * Set the UAProf string. Can be empty, and if so the UAProf header value will not be sent.
 *
 * @see http://en.wikipedia.org/wiki/UAProf
 */
void WebGlobal::setUaProf( const std::string& uaProf )
{
	const char* uaProfName = "x-wap-profile";
	gUaProf = uaProf;
	if (gUaProf.empty()) {
		WebCore::CurlHandle::removeAppendedHTTPHeader( uaProfName );
	}
	else {
		addAppendedHTTPHeader( uaProfName, gUaProf.c_str() );
	}
}

/**
 * Return the UAProf value.
 *
 * @see http://en.wikipedia.org/wiki/UAProf
 */
const std::string& WebGlobal::getUaProf()
{
	return gUaProf;
}

/**
 * Set the locale used by WebKit.
 * The locale is of the form "ll_CC" - or "<language>_<country code>" where the language is
 * a 2 character lower case abbreviation as defined by <a href="http://en.wikipedia.org/wiki/ISO_639-1">ISO 639-1</a>.
 * The country code is two uppercase characters as defined by
 * <a href="http://www.iso.org/iso/english_country_names_and_code_elements">ISO 3166-1</a>.
 *
 * @remarks The language can be supplied as lowercase characters. If so it will be converted
 *          to uppercase. So setLocale("en_us") will return "en_US" when getLocale() is called.
 *
 * @param newLocale The locale to use which is made up of language and country code. If this does not
 *                  contain an underscore then it will be ignored and the locale will be set to "en_US".
 */
void WebGlobal::setLocale( const char* newLocale )
{
	gLocaleString = newLocale;
	
	//LEGACY: software update is helpless and can't maintain its own state via subscriptions...so chew the food and spit this out to a file for them
	std::string::size_type splitidx;
	
	std::string localeCountry;
	std::string localeLang;
	splitidx = gLocaleString.find("_");
	if ((splitidx != std::string::npos) && (splitidx != 0) && (splitidx != (gLocaleString.length()-1))) 
	{
		localeLang = gLocaleString.substr(0,splitidx);
		localeCountry = gLocaleString.substr(splitidx+1);
	}
	else
	{
		//malformed locale...default to english
		localeLang = "en";
		localeCountry = "us";
	}
	
	// write to the file
	std::string ftmpName = gSwUpdateLocaleFile + ".tmp";
	FILE *flocale = fopen(ftmpName.c_str(), "w");
	if (flocale) {
		fprintf(flocale, "localeCountry=%s\n", localeCountry.c_str());
		fprintf(flocale, "localeLanguage=%s\n", localeLang.c_str());
		fsync(fileno(flocale));
		rename(ftmpName.c_str(), gSwUpdateLocaleFile.c_str());
		fclose(flocale);
	}
	
	for (size_t i = 0; i < localeCountry.length(); i++) {
		localeCountry[i] = toupper(localeCountry[i]);
	}
	gLocaleString = localeLang + '_' + localeCountry;

    // For China, we want to specify a collation locale for "pinyin" based sorting
    // rather than the standard sorting order for the "zh_CN" locale.  For all
    // other locales, we will sort by their standard sorting order.  This fixes DFISH-24918.
    // FIXME: webOS SHOULD (but does not currently) support a way for LunaSysMgr
    // or BrowserServer to set the collation locale independent of the locale.
    // When those components, which are clients of WebKit, gain that capability,
    // this override for "zh_CN" should be removed and they should directly
    // call setLocaleCollator with the appropriate collation locale.

    if (gLocaleString == "zh_CN")
        setLocaleCollator("zh_CN@collation=pinyin");
    else
        setLocaleCollator(gLocaleString.c_str());

    // We want lowercase locale, language like "en-us,en;q=0.5"
	// http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.4
	std::string acceptLanguage = gLocaleString + "," + localeLang;
	for (size_t i = 0; i < acceptLanguage.length(); i++) {
		acceptLanguage[i] = tolower(acceptLanguage[i]);
		if (acceptLanguage[i] == '_')
			acceptLanguage[i] = '-';
	}
	acceptLanguage += ";q=0.5"; // Quality factor.

	addAppendedHTTPHeader("Accept-Language", acceptLanguage.c_str());
}

void WebGlobal::setLocaleCollator(const char* newLocaleCollator)
{
    gLocaleCollatorString = newLocaleCollator;
#if USE(V8)
    // Inform V8 of the collation locale, so that it can
    // sort strings correctly in String.localeCompare.
    v8::V8::SetLocaleCollator(newLocaleCollator);
#endif
}

const char* WebGlobal::getLocale( )
{
	return gLocaleString.c_str();
}

const char* WebGlobal::getLocaleCollator()
{
    return gLocaleCollatorString.c_str();
}

void WebGlobal::enableDebugger( bool toggle )
{
#if USE(V8)	
	if( toggle )
		v8::Debug::EnableAgent("webOS.sysmgr", 5858);
#endif	
}

void WebGlobal::setJavaScriptFlags( const char* flagsString )
{
#if USE(V8)	
	v8::V8::SetFlagsFromString( flagsString, strlen(flagsString) );
#endif	
}

void WebGlobal::setJavaScriptTimeoutMs( unsigned long inTimeoutMs )
{
#if USE(V8)	
	char buffer[64];
	sprintf(buffer,"--timeout_script_timeout=%ld", inTimeoutMs );
	v8::V8::SetFlagsFromString( buffer, strlen(buffer) );
#else
#endif
}

void WebGlobal::enableJavaScriptTimeout( bool bEnable )
{
#if USE(V8)	
	v8::V8::EnableTimeoutCheck(bEnable);
#else
#endif
}

/**
 * Copy all databases from one domain to a new one. This will replicate all data associated
 * with a domain (security origin) which currently includes the HTML5 databases as well as
 * cookies.
 *
 * @note This is an administrative function that should only be used for things like
 *       moving Mojo applications from one location to another on disk.
 *
 * @return true if successful, false if not. If this function fails then it will not cleanup
 *         any successfully copied cookies or databases. It is the responsibility of the caller
 *         to call WebGlobal::deleteDatabasesForDomain().
 */
bool WebGlobal::copyDatabasesToDomain(const std::string& strSrcUrl, const std::string& strDstUrl)
{
    g_message("Migrating data from '%s' to '%s'", strSrcUrl.c_str(), strDstUrl.c_str());

	if (strSrcUrl.empty() || strDstUrl.empty()) {
		g_warning("Invalid src/dst url");
		return false;
	}

	if (strSrcUrl == strDstUrl) {
		g_warning("Source and dest domain's are the same.");
		return false;
	}

	if (!gIsInitialized) {
		g_critical("WebKit must be initialized before calling copyDatabasesToDomain");
		return false;
	}

	// Move the cookies over
	KURL srcUrl( ParsedURLString, strSrcUrl.c_str() );
	KURL dstUrl( ParsedURLString, strDstUrl.c_str() );

	// Now move the databases over.
	RefPtr<SecurityOrigin> srcOrigin = SecurityOrigin::create( srcUrl );
	RefPtr<SecurityOrigin> dstOrigin = SecurityOrigin::create( dstUrl );

	if (srcOrigin->equal(dstOrigin.get())) {
        ASSERT(false);
		g_warning("Source and dest origins's are the same.");
        // Hopefully the caller isn't calling deleteDatabasesForDomain in this case else they will
        // remove all app data.
		return false;
	}

    bool succeeded(false);
    WebCore::CookieService* cookieService = WebCore::CookieService::sharedInstance();
    if (cookieService) {
        succeeded = cookieService->copyCookiesToDomain(srcUrl, dstUrl);
        if (!succeeded) {
            g_warning("Failed copying cookies from %s to %s", strSrcUrl.c_str(), strDstUrl.c_str());
        }
    }

    if (succeeded) {
        // ensure origins are populated.
        DatabaseTracker& tracker = DatabaseTracker::tracker();
        // GREG TODO tracker.originQuotaManager(); 
        
        succeeded = tracker.copyOriginDatabases( srcOrigin.get(), dstOrigin.get(), &tracker );
        if (succeeded) {
            g_message("Successfully copied Db's from %s to %s", strSrcUrl.c_str(), strDstUrl.c_str());
        }
        else {
            g_warning("Failure copying Db's from '%s' to '%s'", strSrcUrl.c_str(), strDstUrl.c_str());
        }
    }

	return succeeded;
}

/**
 * Delete all databases and cookies for the specified domain.
 */
void WebGlobal::deleteDatabasesForDomain( const char* inUrl )
{
	KURL url( ParsedURLString,inUrl );
	RefPtr<SecurityOrigin> origin = SecurityOrigin::create( url );
	g_message("Deleting databases for domain %s", inUrl);
	
	// ensure origins are populated.
	// GREG TODO DatabaseTracker::tracker().originQuotaManager(); 
	
	DatabaseTracker::tracker().deleteOrigin( origin.get() );

	// Also delete all cookies for the specified domain
	WebCore::CookieService* cookieService = WebCore::CookieService::sharedInstance();
    if (cookieService) {
		cookieService->clearForDomain(url);
    }

    // delete local storage db
    deleteLocalStorageForDomain(origin);
}

/**
 * Gets called by the Database class when it does things of interest.
 * @note All calls are guaranteed to be in the main loop.
 */
class DbWatcher : public DatabaseWatcher
{
public:
	DbWatcher	(const std::string& url, const std::string& dbName, void* userData)
		: m_userData(userData)
	{
		m_status.url = url;
		m_status.dbname = dbName;
		m_status.err = 0;
	}
protected:
	virtual void dumpStarted(int errVal) {
		m_status.err = errVal;
		if (gEventListener) {
			gEventListener->dbDumpStarted(m_status, m_userData);
		}
	}

	virtual void dumpStopped(int errVal) {
		m_status.err = errVal;
		if (gEventListener) {
			gEventListener->dbDumpStopped(m_status, m_userData);
		}
	}

	virtual void restoreStarted(int errVal) {
		m_status.err = errVal;
		if (gEventListener) {
			gEventListener->dbRestoreStarted(m_status, m_userData);
		}
	}
	
	virtual void restoreStopped(int errVal) {
		m_status.err = errVal;
		if (gEventListener) {
			gEventListener->dbRestoreStopped(m_status, m_userData);
		}
	}
private:
	DbBackupStatus	m_status;
	void*			m_userData;
};

/**
 * Start a dump of the specified database. If the database is currently open then
 * a dump is scheduled in another thread and will happen without affecting the 
 * open db. If the db is closed then a dump is done and this routine will
 * block until that dump is finished.
 *
 * @param url      The URL of the app (security origin) to backup. This can also
 *                 have the special value of k_PhonyCookieUrl to backup the cookie
 *                 database.
 * @param dbname   The database name to backup. Ignored if backing up the cookie db.
 * @param dumpFile The output file to dump the database to.
 * @param userData A pointer for use by the calling function.
 * @param cookieAppID    Specified appID when dumping the cookie database; ignored for other db types
 *
 * @return true if successfully started, false on error.
 */
bool WebGlobal::startDatabaseDump(const std::string& url_, const std::string& dbname, 
                                  const std::string& dumpFile, void *userData, const std::string& cookieAppId)
{
	if (url_.empty() || dumpFile.empty()) {
		return false;
	}

	if (!gIsInitialized) {
		g_critical("Cannot dump a database prior to initializing WebKit");
		return false;
	}

	RefPtr<DbWatcher> dbWatcher = new DbWatcher(url_, dbname, userData);
	if (dbWatcher == NULL)
		return false;

	if (url_ == k_PhonyCookieUrl) {
        CookieService* service = CookieService::sharedInstance(cookieAppId.c_str());
		if (service == NULL)
			return false;

		return service->dumpDatabase(dumpFile.c_str(), dbWatcher);
	}
	else {
		if (dbname.empty()) {
			return false;
		}
		KURL url( ParsedURLString,url_.c_str() );
		RefPtr<SecurityOrigin> origin = SecurityOrigin::create( url );
		if (origin == NULL) {
			return false;
		}

		DatabaseTracker& tracker = DatabaseTracker::tracker();

		// GREG TODO tracker.originQuotaManager();

		return tracker.startDatabaseDump( origin, dbname.c_str(), dumpFile.c_str(), dbWatcher );
	}
}

/**
 * Start a restore of a HTML5 database. At present databases are not asynchronously
 * restored so when this method returns the restore has finished.
 *
 * @param url The application (security origin) url.
 * @param dbname The database name to restore.
 * @param dumpFile The sql dump file to import.
 * @param userData A user supplied pointer passed back to the watcher.
 * @param cookieAppId Specified appID when restoring the cookie database; ignored for other db types
 *
 * @return true if successfully started (or restored).
 */
bool WebGlobal::startDatabaseRestore(const std::string& url_, const std::string& dbname,
                                     const std::string& dumpFile, void* userData, const std::string& cookieAppId)
{
	if (url_.empty() || dbname.empty() || dumpFile.empty()) {
		return false;
	}

    g_debug("Restoring db:'%s' from:'%s' for:'%s'", dbname.c_str(), dumpFile.c_str(), url_.c_str());

	if (!gIsInitialized) {
		g_critical("Cannot restore a database prior to initializing WebKit");
		return false;
	}

	RefPtr<DbWatcher> dbWatcher = new DbWatcher(url_, dbname, userData);
	if (dbWatcher == NULL)
		return false;

	if (url_ == k_PhonyCookieUrl) {
        CookieService* service = CookieService::sharedInstance(cookieAppId.c_str());
		if (service == NULL)
			return false;

		return service->restoreDatabase(dumpFile.c_str(), dbWatcher);
	}
	else {
		KURL url( ParsedURLString,url_.c_str() );
		RefPtr<SecurityOrigin> origin = SecurityOrigin::create( url );
		if (origin == NULL) {
			return false;
		}

		DatabaseTracker& tracker = DatabaseTracker::tracker();

		// GREG TODO tracker.originQuotaManager();

		return tracker.startDatabaseRestore( origin, dbname.c_str(), dumpFile.c_str(), dbWatcher );
	}
}

/**
 * Get information on the last time a database was modified.
 *
 * @param url         The security origin URL.
 * @param dbname      The database name.
 * @param mtime       Time of last database file modification.
 * @param numOpenMods The number of modifications made since database connection was
 *                    made. Zero of database is closed.
 * @param cookieAppId Specified appID when checking the cookie database; ignored for other db types
 *
 * @return true of successful, false if not.
 */
bool WebGlobal::getDatabaseModInfo(const std::string& url_, const std::string& dbname,
                                   time_t& mtime, int& numOpenMods, const std::string& cookieAppId)
{
	if (url_.empty()) {
		return false;
	}
	
	if (!gIsInitialized) {
		g_critical("Cannot get db mod info prior to initializing WebKit");
		return false;
	}

	if (url_ == k_PhonyCookieUrl) {
        CookieService* service = CookieService::sharedInstance(cookieAppId.c_str());
		if (service == NULL)
			return false;

		return service->getDatabaseModInfo(mtime, numOpenMods);
	}
	else {
		if (dbname.empty())
			return false;

		KURL url( ParsedURLString,url_.c_str() );
		RefPtr<SecurityOrigin> origin = SecurityOrigin::create( url );
		if (origin == NULL) {
			return false;
		}

		DatabaseTracker& tracker = DatabaseTracker::tracker();

		// GREG TODO tracker.originQuotaManager();

		return tracker.getDatabaseModInfo( origin, dbname.c_str(), mtime, numOpenMods );
	}
}

WebGlobal::CacheStatistics WebGlobal::getCacheStatistics()
{
	Cache::Statistics s = cache()->getStatistics();

	CacheStatistics stat;
	stat.images.count = s.images.count;
	stat.images.size = s.images.size;
	stat.images.liveSize = s.images.liveSize;
	stat.images.decodedSize = s.images.decodedSize;

	stat.cssStyleSheets.count = s.cssStyleSheets.count;
	stat.cssStyleSheets.size = s.cssStyleSheets.size;
	stat.cssStyleSheets.liveSize = s.cssStyleSheets.liveSize;
	stat.cssStyleSheets.decodedSize = s.cssStyleSheets.decodedSize;

	stat.scripts.count = s.scripts.count;
	stat.scripts.size = s.scripts.size;
	stat.scripts.liveSize = s.scripts.liveSize;
	stat.scripts.decodedSize = s.scripts.decodedSize;

	stat.fonts.count = s.fonts.count;
	stat.fonts.size = s.fonts.size;
	stat.fonts.liveSize = s.fonts.liveSize;
	stat.fonts.decodedSize = s.fonts.decodedSize;

	return stat;
}

void WebGlobal::restartNetworking()
{
	CurlHandlePool::instance()->restart();    
}

void WebGlobal::setSystemSuspending(bool val)
{
	gIsSystemSuspending = val;    
}

bool WebGlobal::isSystemSuspending()
{
    return gIsSystemSuspending;
}

static std::string escapeStringForJson( const String& str )
{
	std::string r;	
	const char* s = str.utf8().data();
	
	while( *s )
	{
		if( *s == '\"' || *s == '\\' ) {
			r += "\\";
			r += *s;
		} else if( *s == 0x0a )
			r += "\\n";
		else if( *s == 0x0d )
			r += "\\r";
		else
			r += *s;
		
		s++;
	}
	
	return r;
}

// This will return a JSON-formatted database query result of the browser history.
// This is app-specific (the browser app in webOS) and really belongs in a native
// service for the browser application.
bool WebGlobal::queryBrowserHistoryBookmarks( const char* filterText, int maxOut, std::string& jsonOut )
{
	// Find the browser's database.
	KURL url( ParsedURLString,"file:///usr/palm/applications/com.palm.app.browser/index.html" );
	RefPtr<SecurityOrigin> origin = SecurityOrigin::create( url );
	// GREG TODO DatabaseTracker::tracker().originQuotaManager();

	String dbPath = DatabaseTracker::tracker().fullPathForDatabase( origin.get(), "browser_data", false );

	if( !dbPath.length() )
		return false;
	
	jsonOut = "{";
	
	SQLiteDatabase db;
	if( db.open( dbPath, DontValidateDatabase ) )
	{
		jsonOut += "\"history\":[";
		{
			// Note: this query is the same as history-store.js from the browser application.
			const char* kHistoryStatement = "SELECT url,title FROM history WHERE (url LIKE ? ) or (title LIKE ?) ORDER BY date DESC";
			SQLiteStatement statement(db,kHistoryStatement);
			
			String filt = String("%") + String(filterText) + String("%");
			statement.prepare();
			statement.bindText(1,filt);
			statement.bindText(2,filt);
			
			int result;
			int count = 0;
			while( SQLResultRow == (result = statement.step()) && ( count <= maxOut ) )
			{		
				if( count )
					jsonOut += ",";
				String url = statement.getColumnText(0);
				String title = statement.getColumnText(1);
				jsonOut += "{\"title\":\""; 
				jsonOut += escapeStringForJson(title);
				jsonOut += "\",\"url\":\""; 
				jsonOut += url.utf8().data();
				jsonOut += "\"}";
				count++;
			}		
			statement.finalize();
		}
		jsonOut += "],";
		
		jsonOut += "\"bookmarks\":[";
		{
			// Note: this query is the same as history-store.js from the browser application.
			const char* kBoomarksStatement = "SELECT url,title,thumbnailFile FROM bookmarks WHERE (url LIKE ? ) or (title LIKE ?) ORDER BY date DESC";
			SQLiteStatement statement(db,kBoomarksStatement);
			
			String filt = String("%") + String(filterText) + String("%");
			statement.prepare();
			statement.bindText(1,filt);
			statement.bindText(2,filt);
			
			int result;
			int count = 0;
			while( SQLResultRow == (result = statement.step()) && ( count <= maxOut ) )
			{		
				if( count )
					jsonOut += ",";
				String url = statement.getColumnText(0);
				String title = statement.getColumnText(1);
				String thumbnailFile = statement.getColumnText(2);
				jsonOut += "{\"title\":\""; 
				jsonOut += escapeStringForJson(title);
				jsonOut += "\",\"url\":\"";
				jsonOut += url.utf8().data();
				jsonOut += "\",\"thumbnail\":\"";
				jsonOut += thumbnailFile.utf8().data();
				jsonOut += "\"}";
				count++;
			}		
			statement.finalize();
		}
		jsonOut += "]";
		
		db.close();
	}

	jsonOut += "}";
	
	return true;
}

bool WebGlobal::getSystemMemoryInfoMb(int& memTotal, int& memFree, int& swapTotal,
									  int& swapFree, int& cached, int& swapCached)
{
    std::ifstream memInfo("/proc/meminfo");
    if (!memInfo)
		return false;
	
    std::string field;
    std::string label;
    int value;

	memTotal = -1;
	memFree = -1;
	swapTotal = -1;
	swapFree = -1;
	cached = -1;
	swapCached = -1;

    while(memInfo >> field) {
        // strip off the ':' on the end of each label
        field = field.substr(0, field.length() - 1);
        memInfo >> value;
        memInfo >> label;

        // we want memory in terms of megabytes
        // so divide by 1024 if the label is for kilobytes
        // and then if it's not megabytes, presume it's in bytes so
        // divide by 1024 * 1024
        if (strcasecmp(label.c_str(), "kb") == 0)
            value /= 1024;
        else if (strcasecmp(label.c_str(), "mb") != 0)
            value /= 1024 * 1024;

        if (field == "MemTotal")
            memTotal = value;
        else if (field == "MemFree")
			memFree = value;
        else if (field == "SwapTotal")
			swapTotal = value;
        else if (field == "SwapFree")
			swapFree = value;
        else if (field == "Cached")
			cached = value;
        else if (field == "SwapCached")
            swapCached = value;
    }
	
    memInfo.close();

	return true;
}

float WebGlobal::getSystemAvailableMemoryScaleFactor()
{
	int memTotal, memFree, swapTotal;
	int swapFree, cached, swapCached;

	if (!getSystemMemoryInfoMb(memTotal, memFree, swapTotal, swapFree,
							   cached, swapCached))
		return 1.0f;

	static const int kBaselineMemoryTotal = 256;
	if (memTotal > kBaselineMemoryTotal)
		return (memTotal * 1.0f) / kBaselineMemoryTotal;

	return 1.0f;
}

/**
 * Receive a response to the Palm::WebView::smartKeySearch request.
 *
 * @return true if the response was accepted, false if not.
 */
bool WebGlobal::smartKeySearchResponse(int requestId, const std::string& match)
{
	// TODO: This is deprecated. Delete once BS/LSM stop calling it.
	return false;
}

// Reporting (profiling) interface
void WebGlobal::profilerEventStart( const std::string& name, const std::string& value )
{
    webOS::Reporter::Event::logStart(0, name.c_str(), value.c_str());
}

void WebGlobal::profilerEventEnd( const std::string& name, const std::string& value )
{
    webOS::Reporter::Event::logStop(0, name.c_str(), value.c_str());
}


void WebGlobal::setActiveApplicationId( const std::string& id )
{
    int idLength = id.find(' ');
    if (idLength)
        gActiveApplicationId = id.substr(0, idLength);
    else
        gActiveApplicationId = id;
}

const std::string& WebGlobal::getActiveApplicationId()
{
    return gActiveApplicationId;
}

// Parse the total RAM installed out of /proc/meminfo.
int MeasureTotalRAM()
{
	gchar* buffer;
	gsize sz;
	int memTotal = 0;
	
	if( !g_file_get_contents( "/proc/meminfo", &buffer, &sz, 0 ) )
		return 0;
	
	char* ptr = strtok( buffer, ": \x0a\x0d" );
	while( ptr )
	{
		if( !strncmp( ptr,"MemTotal",8) )
		{
			// next token is the ram
			if( ( ptr = strtok( 0, ": \x0a\x0d" ) ) )
			{
				memTotal = atoi( ptr );
			}
			
			break;
		}
		
		ptr = strtok( 0, ": \x0a\x0d" );
	}
	
	g_free(buffer);
	return memTotal;
}


void ReadSettingsFromSysService()
{
	static LSHandle* s_lsHandle;
    LSError lserror;
    LSErrorInit(&lserror);

	if (s_lsHandle == NULL) {
        if (LSRegister(NULL, &s_lsHandle, &lserror)) {
            if (!LSGmainAttach(s_lsHandle, Palm::WebGlobal::mainLoop(), &lserror)) {
               if (!LSUnregister(s_lsHandle, &lserror))
                    LSErrorFree(&lserror);

				s_lsHandle = NULL;
			}
            else
                LSErrorFree(&lserror);
		}
        else
            LSErrorFree(&lserror);
	}	
	
	
	bool r = LSCall(s_lsHandle, "palm://com.palm.systemservice/getPreferences", 
				"{ \"subscribe\": false, \"keys\":[\"x_palm_http_headers\"]}",
				sysServicePrefsCallback, NULL, NULL, &lserror);
	
	if( !r )
		LSErrorFree(&lserror);
}

// tokenize an stl string
void explodeStlString(const std::string &inString, const std::string &separator, std::list<std::string>& outTokens )
{
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	while ((end = inString.find (separator, start)) != std::string::npos)
	{
		outTokens.push_back (inString.substr (start, end-start));
		start = end + separator.size();
	}

	outTokens.push_back (inString.substr (start));
}


bool sysServicePrefsCallback(LSHandle *lshandle, LSMessage *message, void *ctx)
{
	std::string jsonRaw = LSMessageGetPayload(message);
	pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
	pbnjson::JDomParser parser;
    if (!parser.parse(jsonRaw, inputSchema)) {
    	return true;
    }
    
    pbnjson::JValue json = parser.getDom();
    if (json.hasKey("x_palm_http_headers")) {
    	std::string v = json["x_palm_http_headers"].asString();
    	
    	std::list<std::string> tokens;
    	explodeStlString(v,":",tokens);
    	
    	for( std::list<std::string>::iterator it=tokens.begin(); it != tokens.end(); ++it )
    	{
    		const std::string name = *it;
    		++it;
    		if( it != tokens.end() )
    		{
    			const std::string value = *it;
    			WebCore::CurlHandle::addAppendedHTTPHeader( name.c_str(), value.c_str() );
    		}    		
    	}

    }

	return true;
}


}

extern "C" {
GMainLoop* webkit_palm_get_mainloop()
{
	return Palm::WebGlobal::mainLoop();
}
}
