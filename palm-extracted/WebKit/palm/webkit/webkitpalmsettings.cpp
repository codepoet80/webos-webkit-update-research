/*
	Copyright 2008 Palm Inc.
*/

#include "config.h"

#include "palmwebglobal.h"
#include "DiskCachePalm.h"
#include "webkitpalmsettings.h"
#include <DatabaseTracker.h>

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include <string>

#if USE(V8)
#include "v8.h"
#include <wtf/Threading.h> // Palm
#endif

static WebKitPalmSettings*		s_settings;

static void WebKitPalmLoadSettings( const char* configFile );
static void WebKitPalmSettingsDefaults( WebKitPalmSettings* inSettings );
void WebKitPalmApplySettings();


// Expands "1MB" --> 1048576, "2k" --> 2048, etc.
unsigned long MemStringToBytes( const char* ptr )
{
	char number[32];
	unsigned long r = 0;
	const char* s= ptr;
	
	while( *ptr && !WTF::isASCIIAlphanumeric(*ptr) ) // skip whitespace
		ptr++;
	s=ptr;
	
	while( WTF::isASCIIDigit(*ptr) )
		*ptr++;
	
	strncpy( number, s, (size_t)(ptr-s) );
	number[ptr-s]=0;
	
	r = (unsigned long)atol(number);
	switch(*ptr) 
	{
	case 'M': 
		r *= 1024 * 1024; break;
	case 'k': 
	case 'K':
		r *= 1024 ; break;
	}
	
	return r;
}

/**
 * Create default setting (if not yet created) and append settings from configuration
 * file (if specified) and return the settings object.
 *
 * @note Caller can call this with NULL to return the current (default or previously
 *       loaded settings object). May also call multiple times with valid config
 *       file paths to chain setting values from multiple files.
 */
WEBKIT_API WebKitPalmSettings*	PalmBrowserSettings( const char* altConfigFile )
{
    static Mutex settings_lock;

    settings_lock.lock();
	if ( !s_settings ) {
		s_settings = new WebKitPalmSettings();
		if( !s_settings ) {
            settings_lock.unlock();
			return NULL;
        }
		WebKitPalmSettingsDefaults( s_settings );
		if( Palm::WebGlobal::mainLoop() )
			WebKitPalmApplySettings();
	}

	if ( altConfigFile )
		WebKitPalmLoadSettings( altConfigFile );
	
    settings_lock.unlock();
	return s_settings;
}

static void WebKitPalmSettingsDefaults( WebKitPalmSettings* inSettings )
{
	s_settings->private_browsing = false;
	s_settings->java_script_enabled = true;
	s_settings->java_script_can_open_windows_automatically = true;
	s_settings->plugins_enabled = true;
	s_settings->acceptCookies = true;
	s_settings->enableLocalStorage = true;
	s_settings->runningInBrowserServer = false;
	s_settings->enableEnhancedViewport = false;
	
	s_settings->browserViewWidth = 320;
	s_settings->browserViewHeight = 480;
	
	s_settings->memCacheTotalBytes = 1 * 1024 * 1024;
	s_settings->memCacheMinDeadBytes = 0 * 1024 * 1024;
	s_settings->memCacheMaxDeadBytes = 1 * 1024 * 1024;
	s_settings->memCacheTotalBytes = 4 * 1024 * 1024;
	s_settings->pageCacheCapacity = 1;
	
	s_settings->diskCacheEnabled = false;
	s_settings->diskCacheTotalBytes = 4 * 1024 * 1024 ;
	s_settings->diskCacheDebug = false;
	s_settings->diskCacheLargestObjectSize = ( s_settings->diskCacheTotalBytes * 3 ) / 4 ;
	
	s_settings->logURLs= false;
	s_settings->enableTextIndexing= false;
	s_settings->enableFitWidth = true;
	s_settings->enableLayoutPaintThrottling = false;
	s_settings->enableFrameFlattening = false;
	
	s_settings->debuggerServerPort = 5858;
	s_settings->debuggerEnable = false;
	s_settings->enableDeveloperExtras = false;
    s_settings->profilerVerbosity = 0;
	s_settings->debugServiceHandles = false;
    s_settings->outputFpsEnable = false;
    strcpy( s_settings->outputFpsFileName, "/media/internal/webkit-fps.log" );

	s_settings->enableBytecodeCache = false;
	s_settings->lunaCacheResources=false;
	s_settings->disableAlphaBlend=false;
	
	s_settings->disableAutoScroll=false;
	
	s_settings->fontScaleFactor = 1.0f;
    s_settings->clickSearchRectangleTopPadding = 0;
    s_settings->clickSearchRectangleRightPadding = 0;
    s_settings->clickSearchRectangleBottomPadding = 0;
    s_settings->clickSearchRectangleLeftPadding = 0;
    s_settings->enableClickSearchRectangleExpansion = false;
	
	s_settings->showClickedLink = false;
	s_settings->enableFastLocalLoad = false;
	s_settings->enableStickyState = false;
	s_settings->checkSpelling = WebKitPalmSettings::AUTO_CORRECT;
	s_settings->checkGrammar = true;
	s_settings->shortcutChecking = true;
    s_settings->wordCompletionStartLength = 4;
	s_settings->spellWidgetYMargin = 30;
	s_settings->enableSqliteExtensions = false;
    s_settings->enableSynchronousDatabase = false;
	
	s_settings->networkTimeoutSeconds = 90;
	
	strcpy( s_settings->sharedClipboardFile, "/var/tmp/webkit-clipboard" );
	
	s_settings->userAgentIndex = -1;
	strcpy( s_settings->userAgentTokensPath, "/etc/palm/uatokens/" );
    strcpy( s_settings->userAgentOverride, "" );

	s_settings->defaultZoomFactor = 1.0;
	s_settings->defaultVisibleHeight = 0;

	s_settings->minFontSize = 5;
	s_settings->minLogicalFontSize = 5;
	s_settings->defaultFixedFontSize = 14;
	s_settings->defaultFontSize = 14;

	// set this to a non-zero font size to enable text auto size expansion. 14 is a good number
	s_settings->minTextAutoSizeFontSize = 0;

	s_settings->disableGifAnimations = false;
	
	// These are overrides in the .conf file.
	s_settings->v8maxYoungSpaceBytes = 0;
	s_settings->v8maxOldSpaceBytes = 0;
	s_settings->v8StackLimitBytes = 0;
	s_settings->v8ExternalMemoryBytes = 0;
	
	s_settings->scriptMinimumTimerInterval = 0.004;

	s_settings->diskImageCacheTotalBytes = 4 * 1024 * 1024;
	s_settings->diskImageFrameCacheTotalBytes = 24 * 1024 * 1024;
	
	strcpy( s_settings->fontsPath, "/usr/share/fonts/" );
	
	s_settings->logObjects = false;
	
	s_settings->enableMemoryStats = false;

	s_settings->maxActiveConnections = 15;
	
	s_settings->maxInactiveFontData = 120;
	s_settings->targetInactiveFontData = 100;

	s_settings->enableCssOptimizations = false;
	
	s_settings->enableFileAccessWhitelist = false;

	strcpy( s_settings->defaultSerifFontFamilyName, "Times New Roman" );
	strcpy( s_settings->defaultSansSerifFontFamilyName, "Prelude" );
	strcpy( s_settings->defaultFixedFontFamilyName, "Courier New" );
	strcpy( s_settings->defaultStandardFontFamilyName, "Prelude" );
	s_settings->appDatabaseQuota = WebCore::DatabaseTracker::defaultQuotaForTracker();

#if defined(MACHINE_QEMUX86)
    // Emulator (VirtualBox) doesn't yet have OpenGL, and even when it does it can't 
    // render directly to fb1 so never enable accelerated compositing for the emulator.
    s_settings->enableAcceleratedCompositing = false;
    s_settings->showCompositedLayerBorders = false;
    s_settings->sharedTexturesForCompositedLayers = false;
#else
    s_settings->enableAcceleratedCompositing = false;
    s_settings->showCompositedLayerBorders = false;
    s_settings->sharedTexturesForCompositedLayers = false;
#endif
	
	s_settings->cachePluginsInDOM = false;

#if defined(__arm__)
	
	strcpy( s_settings->appDatabasePath, "/var/palm/data/" );
	strcpy( s_settings->webkitDataPath, "/usr/palm/webkit" );
	strcpy( s_settings->s_pluginsPath, "/usr/lib/BrowserPlugins/" );
	strcpy( s_settings->s_userInstalledPluginsPath, "");
	strcpy( s_settings->diskCachePath, "/media/cryptofs/.webdiskcache/" );
	strcpy( s_settings->diskImageCachePath, "/media/cryptofs/.webimagecache/");

    strcpy( s_settings->tempSSLCertLocationPath, "/var/tmp/");
	
	strcpy( s_settings->emoticonImagesPath, "/usr/palm/emoticons/");
	
	strcpy( s_settings->cookieJarPath, "/var/palm/data/cookies.db" );
	strcpy( s_settings->localStoragePath, "/var/palm/data/localstorage" );

	s_settings->enableAppCache = false;
	s_settings->appCacheDbMaxSize = 1024 * 1024 * 1024; // 1GB
	strcpy( s_settings->appCacheDbPath, "/media/internal/dbdata/appcache" );
	
	strcpy( s_settings->logObjectsFile, "/media/internal/objlog" );
	
#else 

	std::string path = getenv("HOME");
	path += "/html5-databases/";
	strcpy( s_settings->appDatabasePath, path.c_str() );
	strcpy( s_settings->webkitDataPath, "/usr/palm/webkit" );

	path = getenv("HOME");
	path += "/diskcache/";
	strcpy( s_settings->diskCachePath, path.c_str() );

	strcpy( s_settings->tempSSLCertLocationPath, path.c_str() );

	path = getenv("HOME");
	path += "/emoticons/";
	strcpy( s_settings->emoticonImagesPath, path.c_str() );
		
	path = getenv("HOME");
	path += "/webimagecache/";
	strcpy( s_settings->diskImageCachePath, path.c_str() );

	path = getenv("HOME");
	path += "/cookies/cookies.db";
	strcpy( s_settings->cookieJarPath, path.c_str() );
	
	path = getenv("HOME");
	path += "/html5-localstorage";
	strcpy( s_settings->localStoragePath, path.c_str() );

	s_settings->enableAppCache = true;
	s_settings->appCacheDbMaxSize = 1024 * 1024 * 1024; // 1GB
	path = getenv("HOME");
	path += "/html5-appcache";
	strcpy( s_settings->appCacheDbPath, path.c_str() );

	if (getenv("LUNA_STAGING")) {
		strcpy(s_settings->s_pluginsPath, getenv("LUNA_STAGING"));
		strcat(s_settings->s_pluginsPath, "/lib/BrowserPlugins/");
		strcpy(s_settings->s_userInstalledPluginsPath, getenv("LUNA_STAGING"));
		strcat(s_settings->s_userInstalledPluginsPath, "/lib/UserInstalledBrowserPlugins/");
	}
	else {
		strcpy(s_settings->s_pluginsPath, getenv("HOME"));
		strcat(s_settings->s_pluginsPath, "/Local/lib/BrowserPlugins/");
		strcpy(s_settings->s_userInstalledPluginsPath, getenv("HOME"));
		strcat(s_settings->s_userInstalledPluginsPath, "/Local/lib/UserInstalledBrowserPlugins/");
	}

	strcpy( s_settings->logObjectsFile, "/tmp/objlog" );
	
#endif

	// Add More ... 
	
	s_settings->sslStrictNameChecking = false;					//by default, make SSL checking relaxed (see platform/network/palm/CurlHandle.cpp)
	s_settings->enhanceExceptions = false;		// by default don't produce exception messages with filename and line
						// number for JS exceptions that are uncaught in JS.
	
	s_settings->enableEmoticons = false;
	strcpy( s_settings->emoticonBaseDictionaryFileAndPath,"/etc/palm/emoticon-base-dictionary.txt");
	s_settings->emoticonClassName[0] = '\0';

	s_settings->enableNoSteCommonTagsCheck = false;
	strcpy( s_settings->noSteCommonTagsFilePath, "/etc/palm/no-ste-tags");

	s_settings->freeMemoryThreshold = 16; // in MB
	s_settings->freeSwapThreshold = 64; // in MB
	s_settings->swapRateThreshold = 200; // in pages / sec.
	s_settings->enableMemoryTracking = true;

	s_settings->enableOrphanedNodeReporting = false;

	s_settings->maxImagePixelCount = 2 * 720 * 720;

	s_settings->htmlDbSize = 1 * 1024 * 1024;
    strcpy( s_settings->defaultPrinterIP, "192.168.1.100" );

    s_settings->clipboardWidgetsEnabled = false;
    s_settings->paintTextSelectionOnly = false;
    s_settings->selectionMarkersTapTargetWidth = 48;
    s_settings->selectionMarkersTapTargetHeight = 78;
    s_settings->selectionHitTestOffset = 20;

    strcpy(s_settings->mediaPlayPipelineOption, "--gst-debug=1");
}

#define KEY_STRING(cat,name,var) \
{\
	gchar* _vs;\
	GError* _error = 0;\
	_vs=g_key_file_get_string(keyfile,cat,name,&_error);\
	if( !_error && _vs ) { var=(const char*)_vs; g_free(_vs); }\
	else g_error_free(_error); \
}

#define KEY_STRING_COPY(cat,name,var) \
{\
	gchar* _vs;\
	GError* _error = 0;\
	_vs=g_key_file_get_string(keyfile,cat,name,&_error);\
	if( !_error && _vs ) { \
		size_t _vsl = strlen(_vs); \
		strncpy( var, (const char*)_vs, _vsl ); g_free(_vs); \
		var[_vsl]=0;\
	}\
	else g_error_free(_error); \
}

#define KEY_MEMORY_STRING(cat,name,var) \
{ \
	gchar* _vs=0; \
	GError* _error = 0; \
	_vs=g_key_file_get_string(keyfile,cat,name,&_error); \
	if( !_error && _vs ) { var=::MemStringToBytes((const char*)_vs); g_free(_vs); } \
	else g_error_free(_error); \
}

#define KEY_BOOLEAN(cat,name,var) \
{\
	gboolean _vb;\
	GError* _error = 0;\
	_vb=g_key_file_get_boolean(keyfile,cat,name,&_error);\
	if( !_error ) { var=_vb; }\
	else g_error_free(_error); \
}

#define KEY_INTEGER(cat,name,var) \
{\
	int _v;\
	GError* _error = 0;\
	_v=g_key_file_get_integer(keyfile,cat,name,&_error);\
	if( !_error ) { var=_v; }\
	else g_error_free(_error); \
}

#define KEY_DOUBLE(cat,name,var) \
{\
	double _v;\
	GError* _error = 0;\
	_v=g_key_file_get_double(keyfile,cat,name,&_error);\
	if( !_error ) { var=_v; }\
	else g_error_free(_error); \
}

#define KEY_FLOAT(cat,name,var) \
{\
	float _v;\
	GError* _error = 0;\
	_v=(float)g_key_file_get_double(keyfile,cat,name,&_error);\
	if( !_error ) { var=_v; }\
	else g_error_free(_error); \
}


static void WebKitPalmLoadSettings( const char* configFileName )
{
	ASSERT( s_settings != NULL );
	ASSERT( configFileName != NULL );
	
	GKeyFile* keyfile;
	GKeyFileFlags flags;
	GError* error = 0;
	gboolean vb;
	int vi;
	gchar* vs;
	
	keyfile = g_key_file_new();
	if(!keyfile)
		return;
	flags = GKeyFileFlags( G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);
	
	// First try to load the file from 'var', if that fails, then load from the 
	// regular location.
	if( !g_key_file_load_from_file( keyfile, std::string( std::string("/var") + std::string(configFileName) ).c_str(), flags, &error ) )
	{
        if (error) {
            g_error_free(error);
            error=0;
        }
		if( !g_key_file_load_from_file( keyfile, configFileName, flags, &error ) )
		{
			fprintf(stderr,"%s not found or parsing error: (%s) using defaults.", configFileName, error->message );
			g_key_file_free( keyfile );
			if (error) g_error_free(error);
			return ;
		}
	}
	
	KEY_BOOLEAN( "General", "PrivateBrowsing", s_settings->private_browsing );
	KEY_BOOLEAN( "General", "PluginsEnabled", s_settings->plugins_enabled );
	KEY_BOOLEAN( "JavaScript", "Enabled", s_settings->java_script_enabled );	
	KEY_BOOLEAN( "JavaScript", "CanOpenWindowsAutomatically", s_settings->java_script_can_open_windows_automatically );

	KEY_BOOLEAN( "General", "RunningInBrowserServer", s_settings->runningInBrowserServer );
	
	KEY_BOOLEAN( "General", "EnableEnhancedViewport", s_settings->enableEnhancedViewport );

	KEY_MEMORY_STRING( "General","MemCacheMinDeadBytes", s_settings->memCacheMinDeadBytes );
	KEY_MEMORY_STRING( "General","MemCacheMaxDeadBytes", s_settings->memCacheMaxDeadBytes );
	KEY_MEMORY_STRING( "General","MemCacheTotalBytes", s_settings->memCacheTotalBytes );
	
	KEY_INTEGER( "General", "PageCacheCapacity", s_settings->pageCacheCapacity );

	KEY_BOOLEAN( "General", "DiskCacheEnabled", s_settings->diskCacheEnabled );
	KEY_MEMORY_STRING( "General", "DiskCacheTotalBytes", s_settings->diskCacheTotalBytes );
	KEY_BOOLEAN( "General", "DiskCacheDebug", s_settings->diskCacheDebug );
	KEY_MEMORY_STRING( "General", "DiskCacheLargestObjectSize", s_settings->diskCacheLargestObjectSize );
	KEY_STRING_COPY( "General", "DiskCachePath", s_settings->diskCachePath );

	KEY_MEMORY_STRING( "General", "DiskImageCacheTotalBytes", s_settings->diskImageCacheTotalBytes );
	KEY_MEMORY_STRING( "General", "DiskImageFrameCacheTotalBytes", s_settings->diskImageFrameCacheTotalBytes );
	
	KEY_BOOLEAN( "General", "LogURLs", s_settings->logURLs );
	KEY_BOOLEAN( "General", "EnableTextIndexing", s_settings->enableTextIndexing );
	KEY_BOOLEAN( "General", "SpellingWidgetYMargin", s_settings->spellWidgetYMargin );
	KEY_BOOLEAN( "General", "EnableFitWidth", s_settings->enableFitWidth );
	KEY_BOOLEAN( "General", "AcceptCookies", s_settings->acceptCookies );
	KEY_BOOLEAN( "General", "EnableLayoutPaintThrottling", s_settings->enableLayoutPaintThrottling );
	KEY_BOOLEAN( "General", "EnableFrameFlattening", s_settings->enableFrameFlattening );
	
	
	KEY_INTEGER( "Debug", "DebuggerServerPort", s_settings->debuggerServerPort );
	KEY_BOOLEAN( "Debug", "DebuggerEnable", s_settings->debuggerEnable );
	KEY_BOOLEAN( "Debug", "DeveloperExtras", s_settings->enableDeveloperExtras );
    KEY_INTEGER( "Debug", "ProfilerVerbosity", s_settings->profilerVerbosity );
	KEY_BOOLEAN( "Debug", "DebugServiceHandles", s_settings->debugServiceHandles );
    KEY_BOOLEAN( "Debug", "OutputFps", s_settings->outputFpsEnable );
    KEY_STRING_COPY( "Debug", "OutputFpsFile", s_settings->outputFpsFileName );


	KEY_INTEGER( "General", "ScreenWidth", s_settings->browserViewWidth );
	KEY_INTEGER( "General", "ScreenHeight", s_settings->browserViewHeight );

	KEY_STRING_COPY("General", "PluginsPath", s_settings->s_pluginsPath);
	KEY_STRING_COPY("General", "UserInstalledPluginsPath", s_settings->s_userInstalledPluginsPath);

	KEY_BOOLEAN( "Luna", "CacheResources", s_settings->lunaCacheResources );
	KEY_BOOLEAN( "Luna", "DisableAutoscroll", s_settings->disableAutoScroll );
	
	KEY_BOOLEAN( "General", "EnableBytecodeCache", s_settings->enableBytecodeCache );
	
	KEY_BOOLEAN( "Luna", "DisableAutoScroll", s_settings->disableAutoScroll );
	
	KEY_FLOAT( "General", "FontScaleFactor", s_settings->fontScaleFactor );
	KEY_BOOLEAN( "General", "ShowClickedLink", s_settings->showClickedLink );
	
	KEY_STRING_COPY( "General", "DatabasePath", s_settings->appDatabasePath );
	KEY_STRING_COPY( "General", "WebKitDataPath", s_settings->webkitDataPath );
	KEY_MEMORY_STRING( "General", "DatabaseQuota", s_settings->appDatabaseQuota );
	KEY_STRING_COPY( "General", "CookieJarPath", s_settings->cookieJarPath );
	
	KEY_BOOLEAN( "General", "FastLocalLoad", s_settings->enableFastLocalLoad );
	KEY_BOOLEAN( "General", "EnableStickyState", s_settings->enableStickyState );
	char spellingValue[128] = {0};
	KEY_STRING_COPY( "General", "CheckSpelling", spellingValue );
	if (strcmp(spellingValue, "disabled") == 0)
		s_settings->checkSpelling = WebKitPalmSettings::DISABLED;
	else if (strcmp(spellingValue, "autoCorrect") == 0)
		s_settings->checkSpelling = WebKitPalmSettings::AUTO_CORRECT;
	else if (strcmp(spellingValue, "underline") == 0)
		s_settings->checkSpelling = WebKitPalmSettings::UNDERLINE;
	else if (spellingValue[0] != '\0') {
		g_warning("Unknown spellChecking flag: '%s'", spellingValue);
	}
	KEY_BOOLEAN( "General", "CheckGrammar", s_settings->checkGrammar );
	KEY_BOOLEAN( "General", "CheckShortcut", s_settings->shortcutChecking );

    KEY_INTEGER( "General", "WordCompletionStartLength", s_settings->wordCompletionStartLength );
	
	KEY_INTEGER( "General", "NetworkTimeout", s_settings->networkTimeoutSeconds );
    KEY_INTEGER( "General", "ClickSearchRectangleTopPadding", s_settings->clickSearchRectangleTopPadding );
    KEY_INTEGER( "General", "ClickSearchRectangleRightPadding", s_settings->clickSearchRectangleRightPadding );
    KEY_INTEGER( "General", "ClickSearchRectangleBottomPadding", s_settings->clickSearchRectangleBottomPadding );
    KEY_INTEGER( "General", "ClickSearchRectangleLeftPadding", s_settings->clickSearchRectangleLeftPadding );
    KEY_BOOLEAN( "General", "EnableClickSearchRectangleExpansion", s_settings->enableClickSearchRectangleExpansion);
	
	KEY_BOOLEAN( "General", "PalmSqliteExt", s_settings->enableSqliteExtensions );
	KEY_BOOLEAN( "General", "SynchronousDatabase", s_settings->enableSynchronousDatabase );
	
	KEY_INTEGER( "General", "UserAgentIndex", s_settings->userAgentIndex );
	KEY_STRING_COPY( "General", "UserAgentTokensPath", s_settings->userAgentTokensPath );
    KEY_STRING_COPY( "General", "UserAgentOverride", s_settings->userAgentOverride );

	KEY_STRING_COPY( "General", "SharedClipboardFile", s_settings->sharedClipboardFile );

	KEY_FLOAT( "General", "DefaultZoomFactor", s_settings->defaultZoomFactor);
	KEY_INTEGER( "General", "DefaultVisibleHeight", s_settings->defaultVisibleHeight);

	KEY_INTEGER( "Fonts", "MinFontSize", s_settings->minFontSize );
	KEY_INTEGER( "Fonts", "MinLogicalFontSize", s_settings->minLogicalFontSize );
	KEY_INTEGER( "Fonts", "DefaultFixedFontSize", s_settings->defaultFixedFontSize );
	KEY_INTEGER( "Fonts", "DefaultFontSize", s_settings->defaultFontSize );
	
	KEY_INTEGER( "FontCache", "MaxInactiveFontData", s_settings->maxInactiveFontData );
	KEY_INTEGER( "FontCache", "TargetInactiveFontData", s_settings->targetInactiveFontData );

	KEY_INTEGER( "Fonts", "MinTextAutoSizeFontSize", s_settings->minTextAutoSizeFontSize);

	KEY_BOOLEAN( "General", "DisableGifAnimations", s_settings->disableGifAnimations );
	
	KEY_MEMORY_STRING( "JavaScript", "YoungSpaceSize", s_settings->v8maxYoungSpaceBytes );
	KEY_MEMORY_STRING( "JavaScript", "OldSpaceSize", s_settings->v8maxOldSpaceBytes );
	KEY_MEMORY_STRING( "JavaScript", "StackLimit", s_settings->v8StackLimitBytes );
	KEY_MEMORY_STRING( "JavaScript", "ExternalMemory", s_settings->v8ExternalMemoryBytes );
	
	KEY_DOUBLE( "JavaScript", "ScriptMinimumTimerInterval", s_settings->scriptMinimumTimerInterval );
	
	KEY_BOOLEAN( "General", "EnableSSLStrictNameChecking", s_settings->sslStrictNameChecking);
	KEY_BOOLEAN( "JavaScript", "EnhanceExceptions", s_settings->enhanceExceptions );
	
	KEY_BOOLEAN("General", "EnableEmoticons",s_settings->enableEmoticons);
	KEY_STRING_COPY("General","EmoticonImagesPath",s_settings->emoticonImagesPath);
	KEY_STRING_COPY("General","EmotionBaseDictionaryFile",s_settings->emoticonBaseDictionaryFileAndPath);
	KEY_STRING_COPY("General","EmoticonClassName",s_settings->emoticonClassName);
	
	KEY_BOOLEAN("General", "EnableNoSteCommonTagsCheck", s_settings->enableNoSteCommonTagsCheck);
	KEY_STRING_COPY("General","NoSteCommonTagsFilePath", s_settings->noSteCommonTagsFilePath);

	KEY_STRING_COPY( "General", "FontsPath", s_settings->fontsPath );
	
	KEY_INTEGER( "General", "MaxActiveConnections", s_settings->maxActiveConnections );

	KEY_INTEGER( "General", "FreeMemoryThreshold", s_settings->freeMemoryThreshold );
	KEY_INTEGER( "General", "FreeSwapThreshold", s_settings->freeSwapThreshold );
	KEY_INTEGER( "General", "SwapRateThreshold", s_settings->swapRateThreshold );
	KEY_BOOLEAN( "General", "EnableMemoryTracking",  s_settings->enableMemoryTracking );

	KEY_INTEGER( "General", "MaxImagePixelCount", s_settings->maxImagePixelCount );

	KEY_MEMORY_STRING( "General", "HtmlDbSize", s_settings->htmlDbSize );

	KEY_BOOLEAN( "General", "EnableOrphanedNodeReporting",  s_settings->enableOrphanedNodeReporting );
	
	KEY_STRING_COPY( "Fonts", "DefaultSerifFontFamilyName", s_settings->defaultSerifFontFamilyName );
	KEY_STRING_COPY( "Fonts", "DefaultSansSerifFontFamilyName", s_settings->defaultSansSerifFontFamilyName );
	KEY_STRING_COPY( "Fonts", "DefaultFixedFontFamilyName", s_settings->defaultFixedFontFamilyName );
	KEY_STRING_COPY( "Fonts", "DefaultStandardFontFamilyName", s_settings->defaultStandardFontFamilyName );

	KEY_BOOLEAN( "General", "EnableMemoryStats", s_settings->enableMemoryStats );
	
	KEY_BOOLEAN( "General", "EnableCSSOptimizations", s_settings->enableCssOptimizations );

	KEY_BOOLEAN( "General", "EnableFileAccessWhitelist", s_settings->enableFileAccessWhitelist);

	KEY_BOOLEAN( "General", "EnableLocalStorage", s_settings->enableLocalStorage );
	KEY_STRING_COPY( "General", "LocalStoragePath", s_settings->localStoragePath );

	KEY_BOOLEAN( "General", "EnableAppCache", s_settings->enableAppCache );
	KEY_STRING_COPY( "General", "AppCacheDbPath", s_settings->appCacheDbPath );
	KEY_INTEGER( "General", "AppCacheDbMaxSize", s_settings->appCacheDbMaxSize );

	KEY_BOOLEAN( "Logging", "LogObjects", s_settings->logObjects );
	KEY_STRING_COPY( "Logging", "LogObjectsFile", s_settings->logObjectsFile );
	
	KEY_BOOLEAN( "General", "CachePluginsInDOM", s_settings->cachePluginsInDOM );

#if !defined(MACHINE_QEMUX86)
    KEY_BOOLEAN( "General", "EnableAcceleratedCompositing", s_settings->enableAcceleratedCompositing );
    KEY_BOOLEAN( "General", "ShowCompositedLayerBorders", s_settings->showCompositedLayerBorders );
    KEY_BOOLEAN( "General", "SharedTexturesForCompositedLayers", s_settings->sharedTexturesForCompositedLayers );    
#endif
	
#if USE(V8)
	// Any V8 flags just need to be set immediately.
	{
		gchar* _vs = 0;
		_vs=g_key_file_get_string(keyfile,"JavaScript","Flags",0);
		if( _vs )  
		    v8::V8::SetFlagsFromString( (const char*)_vs, strlen(_vs) );
	}
#endif
    KEY_STRING_COPY( "Print", "DefaultPrinterIP", s_settings->defaultPrinterIP );

    KEY_BOOLEAN( "Clipboard", "EnableClipboardWidgets", s_settings->clipboardWidgetsEnabled);
    KEY_BOOLEAN( "Clipboard", "PaintTextSelectionOnly", s_settings->paintTextSelectionOnly);
    KEY_INTEGER( "Clipboard", "SelectionMarkersTapTargetWidth", s_settings->selectionMarkersTapTargetWidth);
    KEY_INTEGER( "Clipboard", "SelectionMarkersTapTargetHeight", s_settings->selectionMarkersTapTargetHeight);
    KEY_INTEGER( "Clipboard", "SelectionHitTestOffset", s_settings->selectionHitTestOffset);

    KEY_STRING_COPY( "Media",  "PlayPipelineOption", s_settings->mediaPlayPipelineOption);

	g_key_file_free( keyfile );	

	// WebKit 3.X defines scaling in percent and WebKit4.X uses scale factor
	s_settings->fontScaleFactor *= 100.0f;

}

/**
 * Temporary support for an old setting (enableSmartText). This is a legacy setting
 * to enable SmartKey which was sticky state and auto-capitalization.
 */
bool WebKitPalmSettings::smartTextEnabled() const
{
	return enableStickyState || checkGrammar;
}

void WebKitPalmApplySettings()
{
	WebCore::diskCache()->setMaxCapacity( s_settings->diskCacheTotalBytes, /* prune = */ false );
	if( s_settings->diskCacheDebug )
		printf( "Disk Cache : %s, size=%d k\n", s_settings->diskCacheEnabled ? "enabled" : "disabled", (int)s_settings->diskCacheTotalBytes/1024 );
}
