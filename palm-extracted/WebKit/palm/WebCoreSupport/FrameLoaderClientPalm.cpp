
// Copyright 2007,2008,2009 Palm Inc.
 
#include "config.h"
#include "FrameLoaderClientPalm.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "FrameLoader.h"                     
#include "FrameView.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLMetaElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "CString.h"
#include "ProgressTracker.h"
#include "ScrollView.h"
#include "NameNodeList.h"
#include "ResourceHandle.h"
#include "RenderPart.h"
#include "HistoryItem.h"

#include "PluginDatabase.h"
#include "PluginView.h"
#include "EventNames.h"


#include <stdio.h>
#include <map>
#include <string>
#include <syslog.h>
#include <dirent.h>
#include <sys/stat.h>

#include "palmwebframe.h"
#include "palmwebpage.h"
#include "palmwebpageclient.h"
#include "palmwebpageprivate.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebview.h"
#include <palmwebglobal.h>
#include "palmwebviewclient.h"
#include <palmerrorcodes.h>
#include "webkitpalmsettings.h"
#include "webosDeviceUATokens.h"
#include <Event.h>
#include <WebKitVersion.h>
#include <WebKitSubmission.h>

#if USE(JSC)
#include <JavaScriptCore/APICast.h>
#endif

#if USE(V8)
#include "V8Proxy.h"
#endif

#if defined(__arm__)
#include <lunaprefs.h>
#endif

/** Define this to get more verbose logging. */
#undef VERBOSE_MESSAGES

#ifdef VERBOSE_MESSAGES
#define FLTRACE(...) {printf("FLOADER: " __VA_ARGS__); fflush(stdout);}
#else
#define FLTRACE(...) (void)0
#endif

using namespace WebCore;

namespace WebKit {

typedef std::map<std::string,std::string> QueryStringItemSet;
void ParseQueryString( const String& qs, QueryStringItemSet& outItems );

FrameLoaderClient::FrameLoaderClient(Palm::WebFrame* frame)
    : m_frame(frame)
    , m_firstData(false)
	, m_ProgressIndicator( 0 )
    , m_pluginView(0) 
    , m_hasSentResponseToPlugin(false)
	, m_lastPercentSent(-1)
	, m_userAgentInitialized(false)

{
    ASSERT(m_frame);
	
}

FrameLoaderClient::~FrameLoaderClient()
{
}


void FrameLoaderClient::initializeUserAgentTokens()
{
	if( m_userAgentInitialized )
		return;
	
	const char* kUATokensFolder = PalmBrowserSettings()->userAgentTokensPath;
	
	// The contents of each file gets appended onto the useragent.
	DIR* dir = opendir(kUATokensFolder);
	if( dir )
	{
		struct stat fileStats;
		struct dirent* entry = readdir(dir);
		while( entry )
		{
			String token;
			std::string fullpath = kUATokensFolder + std::string(entry->d_name);
			FILE* f = fopen( fullpath.c_str(), "r" );
			if( f ) {
				int ch = fgetc(f);
				while( ch != -1 ) {
					if( 0x0a != ch && 0x0d != ch && 0x20 != ch )
						token.append( char(ch) );
					ch = fgetc(f);
				}
				if( token.length() ) 
					m_UATokens.append( token );
				fclose(f);
			}
			entry = readdir(dir);
		}	
		closedir(dir);
	}
		
	m_userAgentInitialized=true;	
}

// This function is copied from luna-sysmgr - DeviceInfo.cpp
// Shall be eventually ported to the desktop
static bool getPrefValue(const char* key, String& value)
{
#if defined(__arm__)
	char* str = 0;
	if (LP_ERR_NONE == LPSystemCopyStringValue(key, &str) && str) {
		value = str;
		g_free((gchar*) str);
		return true;
	}	
#endif

	value = "";
	return false;
}

// Once the 'getPrefValue' function is ported to the desktop this function shall be fixed
static const String getWebOSVersion ()
{
	static String version;

    if (!getPrefValue("com.palm.properties.browserOsVersion", version)) {
        if (getPrefValue("com.palm.properties.version", version)) {
            if (version.startsWith("Palm webOS ", true)) {
                version = version.substring(11);
            } else if (version.startsWith("HP webOS ", true)) {
                version = version.substring(9);
            }
        } else {
#if defined(__arm__)
            version = "unknown";
#else	
            version = "2.0"; // Desktop version
#endif
        }
    }
    return version;
}

// Once the 'getPrefValue' function is ported to the desktop this function shall be fixed
static const String getDeviceName ()
{
	static String name;
	if ( ! getPrefValue("com.palm.properties.productLineName", name) ) {
#if defined(__arm__)
		name = "unknown";
#else	
		name = "Desktop"; // Desktop name
#endif
	}
	return name;
}

// Once the 'getPrefValue' function is ported to the desktop this function shall be fixed
static const String getDeviceVersion ()
{
	static String version;
	if ( ! getPrefValue("com.palm.properties.productLineVersion", version) ) {
#if defined(__arm__)
		version = "unknown";
#else
		version = "1.0"; // Desktop version
#endif
	}
	return version;
}

// Once the 'getPrefValue' function is ported to the desktop this function shall be fixed
static const String getDeviceClass()
{
    static String devClass;
    if (!getPrefValue("com.palm.properties.productClass", devClass)) {
#if defined(__arm__)
        devClass = "hp-device";
#else
        devClass = "hp-desktop"; // Device class
#endif
    }
    return devClass;
}

static const String getPlatformName()
{
    static String platformName;
    if (!getPrefValue("com.palm.properties.browserOsName", platformName)) {
        platformName = "hpwOS";
    }
    return platformName;
}

static const char* kFixedUserAgents[] = 
{
	"Mozilla/5.0 (%s; Linux; %s/%s; U; %s) AppleWebKit/" FULL_VERSION_STRING " (KHTML, like Gecko) %s/" SUBMISSION_NUMBER_STRING " Safari/" FULL_VERSION_STRING " %s/%s",
};

String FrameLoaderClient::userAgent(const KURL&)
{
	static String ua;
	static String lastWkLocale;	
	static String os_version;
	static String device_name;
	static String device_version;
    static String deviceClass;
    static String platformName;
	
    if (PalmBrowserSettings()->userAgentOverride[0] != '\0') {
        static String override;

        if (override.isEmpty()) {
            override = PalmBrowserSettings()->userAgentOverride;
        }

        return override;
    }

	if ( os_version.isEmpty () ) {
		os_version = getWebOSVersion ();
	}
	if ( device_name.isEmpty () ) {
		device_name = getDeviceName ();
	}
	if ( device_version.isEmpty () ) {
		device_version = getDeviceVersion ();
	}
    if (deviceClass.isEmpty()) {
        deviceClass = getDeviceClass();
    }
    if (platformName.isEmpty()) {
        platformName = getPlatformName();
    }
	
	const char* wkLocale = Palm::WebGlobal::getLocale();
	if (ua.isEmpty() || lastWkLocale != wkLocale) {
		lastWkLocale = wkLocale;

		String uaLocale = wkLocale;
		// The UA string delimits the language and country with a hyphen
		uaLocale.replace('_', '-');

		int idx = PalmBrowserSettings()->userAgentIndex;
		if (idx < 0 || idx >= (int)( sizeof(kFixedUserAgents) / sizeof(kFixedUserAgents[0])) )
			idx = 0;

		ua = kFixedUserAgents[ idx ];
		if( strstr( kFixedUserAgents[ idx ], "%s" ) ) {
			char* temp(NULL);
			if (-1 != ::asprintf(&temp, kFixedUserAgents[ idx ],
                                        deviceClass.utf8().data(),
                                        platformName.utf8().data(),
			                            os_version.utf8().data (),
			                            uaLocale.utf8().data(),
                                        PalmBrowserSettings()->runningInBrowserServer ? "wOSBrowser" : "wOSSystem",
			                            device_name.utf8().data (),
			                            device_version.utf8().data ())) {
				ua = temp;
				::free(temp);
			}
			else {
				ua = kFixedUserAgents[ idx ];
			}
		}

		initializeUserAgentTokens();
	
		// Add additional tokens to the user-agent 
		for( unsigned i=0; i<m_UATokens.size(); i++ ) 
		{
			ua.append("; ");
			ua.append(m_UATokens[i]);
		}		
	}

	return ua;	
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClient::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
	FLTRACE( "FrameLoaderClient::createDocumentLoader( url=%s protocol=%s path=%s ).\n",  request.url().string().utf8().data(),request.url().protocol().utf8().data(), request.url().path().utf8().data() );
	
	if( PalmBrowserSettings()->logURLs )
	{
		syslog( 6, "NOVA-BROWSER: %s", request.url().string().utf8().data() );
	}
	
	// Check for local/calculated built-in pages.
	if( request.url().protocolIs("about") )
	{
		CString content;
		String contentMimeType = "text/html";
		String contentEncoding = "utf8";
		RefPtr<SharedBuffer> sharedBuffer;

		// Construct data path. Suppose to be "/usr/palm/webkit/data/en_us/"
		String data_path (PalmBrowserSettings()->webkitDataPath);
                data_path += String("/data");
		data_path += String("/en_us/");
	
		if( request.url().path() == String("plugins") )
		{
			data_path += String("plugins.html");
			sharedBuffer = SharedBuffer::createWithContentsOfFile (data_path);    
		}
		else if( request.url().path() == String("blank") )
		{
                    sharedBuffer = SharedBuffer::create(content.data(), content.length());
		}
		else
		{
			data_path += String("about.html");
			sharedBuffer = SharedBuffer::createWithContentsOfFile (data_path);
		}

		SubstituteData data(sharedBuffer.release(), contentMimeType, contentEncoding, request.url(), request.url());
		RefPtr<DocumentLoader> loader = DocumentLoader::create(request, data);
		return loader.release();
	}
   
	
    RefPtr<DocumentLoader> loader = DocumentLoader::create(request, substituteData);
    return loader.release();
}

void FrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction policyFunction,  PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyUse);
}


void FrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
	FLTRACE("FrameLoaderClient::committedLoad\n");
    if (!m_pluginView) {
        FrameLoader *fl = loader->frameLoader();
        fl->writer()->setEncoding(m_response.textEncodingName(), false);
        fl->addData(data, length);
    }

    // addData() above might have set a plugin view, so check again here
    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
            // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
            // to null
            if (!m_pluginView)
                return;
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    }
}

void FrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge& challenge)
{
	FLTRACE("FrameLoaderClient::dispatchDidReceiveAuthenticationChallenge\n");
	Palm::WebPage* page = m_frame->page();
	if (page->client()) {

		std::string userName;
		std::string password;
		CString realm = challenge.protectionSpace().realm().utf8();

        if (page->client()->dialogUserPassword(realm.data(), userName, password)) {
            Credential credential(userName.c_str(), password.c_str(), CredentialPersistenceNone);
            
            challenge.sourceHandle()->receivedCredential(challenge, credential);
        }
        else {
            challenge.sourceHandle()->receivedCancellation(challenge);
        }
    }
}

void  FrameLoaderClient::dispatchDidReceiveSSLCertValidationFail(DocumentLoader *,Palm::SSLValidationInfo& sslInfo)
{
	FLTRACE("FrameLoaderClient::dispatchDidReceiveAuthenticationChallenge\n");
	Palm::WebPage* page = m_frame->page();
	if (page->client()->dialogSSLConfirm(sslInfo) )
	{
		//dialog response succeeded
		//TODO: potential confusion between what dialog returns and the SSLValidation AcceptDecision enums...fix
		//		also, no need for get/set anymore...just make it consistent from the top
		if (sslInfo.getAcceptDecision() == 0)			//dialog 0 is a reject
			sslInfo.setAcceptDecision(Palm::SSLValidationInfo::AcceptDecision_Reject);
		else if (sslInfo.getAcceptDecision() == 1)	//dialog 1 is an accept always
			sslInfo.setAcceptDecision(Palm::SSLValidationInfo::AcceptDecision_AcceptPermanently);
		else							//dialog 2 is an accept once
			sslInfo.setAcceptDecision(Palm::SSLValidationInfo::AcceptDecision_AcceptForSessionOnly);
	}
	else {
		//dialog failed for some reason
		sslInfo.setAcceptDecision(Palm::SSLValidationInfo::AcceptDecision_Reject);
	}
	
}

void FrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
	FLTRACE("FrameLoaderClient::dispatchDidCancelAuthenticationChallenge\n");
    notImplemented();
}

void FrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, unsigned long , ResourceRequest& request, const ResourceResponse&)
{
	FLTRACE("FrameLoaderClient::dispatchWillSendRequest. req=%s\n", request.url().prettyURL().utf8().data());
    notImplemented();

    Palm::WebPage* page = m_frame->page();

    if (page && page->client()) {
        page->client()->loadActive();
    }
}

void FrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest& request)
{
	FLTRACE("FrameLoaderClient::assignIdentifierToInitialRequest. req=%s\n", request.url().prettyURL().utf8().data());
    notImplemented();   

    Palm::WebPage* page = m_frame->page();

    if (page && page->client()) {
        page->client()->loadActive();
    }
}


void FrameLoaderClient::postProgressStartedNotification()
{
	FLTRACE("FrameLoaderClient::postProgressStartedNotification\n");
	Palm::WebPage* page = m_frame->page();
	Palm::WebPageClient* pageClient = page->client();

	if (pageClient) {
		pageClient->loadStarted();

		// notify plugins that a page load is in progress
		WebCore::Frame* coreFrame = core(m_frame);
		if (coreFrame) {
			ITERATE_PLUGIN(PageLoading, coreFrame, false);
		}

		// FIXME: Why do we send 10?
		// -- we do this to tell the page that the load
		// has started.
		pageClient->loadProgress(10);
		m_lastPercentSent = 10;
	}

}



void FrameLoaderClient::postProgressEstimateChangedNotification()
{
	Palm::WebPage* page = m_frame->page();
	Page* corePage = core(page);

	m_ProgressIndicator = lround(corePage->progress()->estimatedProgress()*100 ) ;
	if( m_ProgressIndicator >= 100 )
		m_ProgressIndicator = 0;

	if( m_lastPercentSent == m_ProgressIndicator )
		return;

	FLTRACE("FrameLoaderClient::postProgressEstimateChangedNotification: %ld\n", m_ProgressIndicator);

	Palm::WebPageClient* pageClient = page->client();
	if (pageClient) {
		m_lastPercentSent=m_ProgressIndicator;
		pageClient->loadProgress(m_ProgressIndicator);
	}
}

void FrameLoaderClient::postProgressFinishedNotification()
{
	FLTRACE("FrameLoaderClient::postProgressFinishedNotification\n");
	Palm::WebPage* page = m_frame->page();
	Palm::WebPageClient* pageClient = page->client();

	if (pageClient) {
		pageClient->loadProgress(100);
		pageClient->loadStopped();
		// notify plugins that a page load completed
		WebCore::Frame* coreFrame = core(m_frame);
		if (coreFrame) {
			ITERATE_PLUGIN(PageLoaded, coreFrame, false);
		}
		m_lastPercentSent=100;
	}
}

void FrameLoaderClient::frameLoaderDestroyed()
{
	delete m_frame;
    m_frame = 0;
	
    delete this;
}

void FrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
    m_firstData = true;
}

void FrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction& na, const ResourceRequest& resource, PassRefPtr<FormState>, const String& frameName)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    
    Palm::WebPage* page = m_frame->page();
    if (!page) {
    	return;
    }
    
    bool willOpenHandler = page->client()->interceptPageNavigation(resource.url().string().utf8().data(), false);
    
	// FIXME: Need to verify that JS popups are still blocked 
    // because they may use NavigationTypeOther, which this permits. If this is true,
    // must create custom NavigationType for plugins. Flash could use a modified NPN_GetUrl 
    // with NavigationTypeLinkClicked... not likely since it's a standard function
	if (!willOpenHandler 
		 && (na.type() == NavigationTypeLinkClicked 
	             || na.type() == NavigationTypeFormSubmitted 
	             ||na.type() == NavigationTypeOther /*e.g. click link inside flash plugin*/))
	{
		(core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyUse);
	} 
	
	(core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyIgnore);
}

void FrameLoaderClient::dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction policyFunction, const WebCore::NavigationAction& na, const WebCore::ResourceRequest& req, WTF::PassRefPtr<WebCore::FormState>)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
	FLTRACE("FrameLoaderClient::dispatchDecidePolicyForNavigationAction, type=%d \n", na.type());

	PolicyAction action = PolicyUse;
	Palm::WebPage* page = m_frame->page();
	
	if( NavigationTypeLinkClicked == na.type() || NavigationTypeOther == na.type() ) {

		Palm::WebView* view = page->view();

		if (view && view->client()) {
	
			String url = req.url().prettyURL();
			CString utf8 = url.utf8();
			const char* strUrl = utf8.data();
			bool isInitialOpen = na.event() == getOpenUrlEvent();
			if (!isInitialOpen && view->getInterrogateClicks()) {
				FLTRACE("Client is interrogating %s\n", strUrl);
				view->client()->linkClicked(strUrl);
				action = PolicyIgnore;
			}
			else if (isMainFrame() && page->client()->interceptPageNavigation(strUrl, isInitialOpen)) {
				FLTRACE("Client is redirecting %s\n", strUrl);
				action = PolicyIgnore;
			}
		}
	}
	
	FLTRACE("FrameLoaderClient::dispatchDecidePolicyForNavigationAction: action=%u\n", action);
	(core(m_frame)->loader()->policyChecker()->*policyFunction)(action);
}

PassRefPtr<WebCore::Widget> FrameLoaderClient::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    if (!PalmBrowserSettings()->plugins_enabled) {
        return 0;
    }
   
	FLTRACE("FrameLoaderClient::createPlugin\n");

    ASSERT(paramNames.size() == paramValues.size());
    RefPtr<PluginView> pluginView = PluginView::create(core(m_frame), pluginSize, (Element*)element, url, paramNames, paramValues, mimeType, loadManually);
	if( !pluginView.get() )
		return 0;

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;
	
    return 0;
}


PassRefPtr<WebCore::Frame> FrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                          const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
	FLTRACE("FrameLoaderClient::createFrame\n");

	Palm::WebFrameData frameData;
	frameData.url = url;
	frameData.name = name;
	frameData.ownerElement = ownerElement;

	Palm::WebFrame* webFrame = new Palm::WebFrame(m_frame, &frameData);

	RefPtr<Frame> childFrame = adoptRef(core(webFrame));
	
    childFrame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());

    // FIXME: All of the below should probably be moved over into WebCore, but it hasn't been yet
    childFrame->tree()->setName(name);
    core(m_frame)->tree()->appendChild(childFrame);

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}


void FrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
	m_hasSentResponseToPlugin = false;
}

PassRefPtr<WebCore::Widget> FrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL,
                                                  const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

void FrameLoaderClient::didDisplayInsecureContent()
{
}

void FrameLoaderClient::didRunInsecureContent(SecurityOrigin*)
{
}

void FrameLoaderClient::didCreateScriptContextForFrame()
{
}

void FrameLoaderClient::didDestroyScriptContextForFrame()
{
}

void FrameLoaderClient::didCreateIsolatedScriptContext()
{
}


ObjectContentType FrameLoaderClient::objectContentType(const KURL& url, const String& mimeType)
{
	String type = mimeType;
    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
	if( type.isEmpty() )
		type = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.')+ 1));
	
	if (type.isEmpty())
        return WebCore::ObjectContentNone;
	
	if( MIMETypeRegistry::isSupportedImageMIMEType(type) ) 
		return WebCore::ObjectContentImage;
    
    if (PalmBrowserSettings()->plugins_enabled && PluginDatabase::installedPlugins()->isMIMETypeRegistered(type))
        return WebCore::ObjectContentNetscapePlugin;
    
	if( MIMETypeRegistry::isSupportedNonImageMIMEType(type) )
		return WebCore::ObjectContentFrame;
	
   if (url.protocol() == "about")
        return ObjectContentFrame;
	
	return WebCore::ObjectContentNone;

}

String FrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClient::dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*)
{
    Frame* coreFrame = core(m_frame);
    ASSERT(coreFrame);
    
#if USE(V8)
    
	v8::HandleScope hs;
	v8::Handle<v8::Context> existingContext = coreFrame->script()->proxy()->context();
    if( existingContext.IsEmpty() ){
    	// This does not seem to exist anymore in V8Proxy ... 
    	// GREG TODO coreFrame->script()->proxy()->initContextIfNeeded();
    	// We'll catch this on the other side
    	return;
    }
	
#else
    // TODO: Consider using g_signal_has_handler_pending() to avoid the overhead
    // when there are no handlers.
    JSGlobalContextRef context = toGlobalRef(coreFrame->script()->globalObject()->globalExec());
    JSObjectRef windowObject = toRef(coreFrame->script()->globalObject());
    ASSERT(windowObject);
#endif

    Palm::WebFrameClient* frameClient = m_frame->client();
    if (frameClient)
        frameClient->jsObjectCleared();
}

void FrameLoaderClient::didPerformFirstNavigation() const
{
	FLTRACE("FrameLoaderClient::didPerformFirstNavigation\n");
}

void FrameLoaderClient::registerForIconNotification(bool) 
{ 
    notImplemented(); 
} 
    

void FrameLoaderClient::setMainFrameDocumentReady(bool bReady) 
{
	FLTRACE("FrameLoaderClient::setMainFrameDocumentReady: %c\n", bReady ? 'Y' : 'N');
	// NOTE: his is only interesting once we provide an external API for the DOM
	
	if( !bReady )
		return;

	Palm::WebPage* page = m_frame->page();
	Palm::WebPageClient* pageClient = page->client();
	Palm::WebFrameClient* frameClient = m_frame->client();
	if (!pageClient)
		return;
	
	// update the URL.
    DocumentLoader* dl = core(m_frame)->loader()->documentLoader();
    if (dl && frameClient) {
        m_lastTitleSent = core(m_frame)->document()->title().utf8();
        m_lastUrlSent = dl->url().prettyURL().utf8();
        frameClient->urlTitleChanged(m_lastUrlSent.data(), m_lastTitleSent.data());
    }

	// FIXME: Do we allow viewport setting from sub-frames?
	if (m_frame != page->mainFrame())
		return;

	Palm::WebView* view = page->view();
    if (!view)
        return;

	Palm::WebViewClient* viewClient = view->client();
    if (!viewClient)
        return;

	ScrollView* scrollView = core(m_frame)->view();
    if (!scrollView)
        return;

	DocumentType* doctype = core(m_frame)->document()->doctype();
	if (doctype && (doctype->publicId().contains("mobile", false) || doctype->systemId().contains("mobile", false)))
	{
	    // Mimic the iPhone and simulate viewport at device width for mobile DocTypes
	    int deviceWidth, deviceHeight;
	    viewClient->getScreenSize( deviceWidth, deviceHeight );
	    scrollView->setFixVirtualWidth(deviceWidth);
	}

/* This has moved to Document::processViewport. Remove eventually	
	Palm::WebView* view = page->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

	ScrollView* scrollView = core(m_frame)->view();
	if (!scrollView)
		return;
	
	
	if( core(m_frame)->document()->url().startsWith("file://") )
	{
		// For local Luna apps, we want to allow all access.
		FrameLoader::setRestrictAccessToLocal( false );
	}
	
	// Process the META viewport tag (if there is one)
	// This is compatible with iPhone 1.1.1
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html#//apple_ref/doc/uid/TP40008193
	Document* doc = core(m_frame)->document();
	ASSERT(doc);

	int deviceWidth, deviceHeight;
	viewClient->getScreenSize( deviceWidth, deviceHeight );
	
	RefPtr<NodeList> list = doc->getElementsByTagName( "meta" );

	for( unsigned int i=0; i<list->length(); i++ )
	{
		Node* n = list->item(i);
		HTMLMetaElement* meta = static_cast<HTMLMetaElement*>(n);
		if( meta )
		{            
			if( equalIgnoringCase( meta->name(), String("viewport") ) )
			{
				double viewportInitialScale = 0.0; // not set
				double viewportMinimumScale = 0.0; // not set
				double viewportMaximumScale = 0.0; // not set
				int viewportWidth = 0; // not set
				int viewportHeight = 0; // not set
				bool viewportUserScalable = true;
	
				String content = meta->content();
				size_t len = (size_t) content.length();
				char * buffer = new char[ len+1 ];
				if( buffer )
				{
					const char* kTokenDelimiters = "\t =,;" ;
					strcpy( buffer, content.utf8().data() );
					char* token;
					
					//tokenize the string.
					token=strtok(buffer,kTokenDelimiters);
					while( token )
					{
						if( !strcmp(token,"initial-scale" ) )
						{
							token=strtok(0,kTokenDelimiters);
							float init_scale = atof(token);
							scrollView->setInitialScale(init_scale);
							viewportInitialScale = init_scale;
						}
						else if( !strcmp(token,"width" ) )
						{
							token=strtok(0,kTokenDelimiters);
							if( token )
							{
								int width = deviceWidth; 
								if( strcmp(token,"device-width"))
									width = atoi(token);
								scrollView->setFixVirtualWidth(width);
								viewportWidth = width;
							}
						}
						else if( !strcmp(token,"height" ) )
						{
							if(( token=strtok(0,kTokenDelimiters)))
							{
								int height = deviceHeight; 
								if( strcmp(token,"device-height"))
									height = atoi(token);
								scrollView->setFixVirtualHeight(height);
								viewportHeight = height;
							}
						}
						else if( !strcmp(token,"minimum-scale" ) )
						{
							if((token=strtok(0,kTokenDelimiters)))
							{
								float v = atof(token);
								scrollView->setMinScale(v);
								viewportMinimumScale = v;
							}
						}
						else if( !strcmp(token,"maximum-scale" ) )
						{
							if((token=strtok(0,kTokenDelimiters)))
							{
								float v = atof(token);
								scrollView->setMaxScale(v);
								viewportMaximumScale = v;
							}
						}
						else if( !strcmp(token,"user-scalable" ) )
						{
							if((token=strtok(0,kTokenDelimiters)))
							{
								bool userScalable=false;
								if( !strcmp(token,"yes") )
									userScalable=true;
								scrollView->setUserCanScale(userScalable);
								viewportUserScalable = userScalable;
							}
						}
													
						token=strtok(0,kTokenDelimiters);
					}
					
					delete[] buffer;
					
					core(m_frame)->forceLayout();
					core(m_frame)->view()->adjustViewSize();

					viewClient->viewportTagParsed(viewportInitialScale, viewportMinimumScale, viewportMaximumScale,
												  viewportWidth, viewportHeight, viewportUserScalable);
				}
				
			}
		}
	}
*/	
}

bool FrameLoaderClient::hasWebView() const
{                                  
    return true;
}

/**
 * Called more than once for a single page, but less frequently than dispatchDidFinishLoading.
 */
void FrameLoaderClient::dispatchDidFinishLoad() 
{ 
    // This is the caller of didFinishLoadForFrame in WebKit, but there are sites
    // that don't fire this event until js is interrupted. This needs to be
    // further investigated, because this method dispatches the "true" 
    // didFinish --AD
    
    // GREG g_signal_emit_by_name(m_frame, "load_done", true);
}

void FrameLoaderClient::frameLoadCompleted() 
{
	if( isMainFrame() ) {
		FLTRACE("FrameLoaderClient::frameLoadCompleted: main frame.\n");
	}
    notImplemented();
}

void FrameLoaderClient::saveViewStateToItem(HistoryItem* item)
{
	FLTRACE( "FrameLoaderClient::saveViewStateToItem: '%s'\n", item->url().prettyURL().utf8().data() );
    notImplemented(); 
}

void FrameLoaderClient::restoreViewState()
{
	FLTRACE("FrameLoaderClient::restoreViewState\n");
    notImplemented(); 
}

bool FrameLoaderClient::shouldGoToHistoryItem(HistoryItem* item) const 
{
	FLTRACE( "FrameLoaderClient::shouldGoToHistoryItem: '%s'\n", item->url().prettyURL().utf8().data() );

    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}


void FrameLoaderClient::makeRepresentation(DocumentLoader*) {  }
void FrameLoaderClient::forceLayout() { notImplemented(); }
void FrameLoaderClient::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClient::setCopiesOnScroll() { notImplemented(); }
void FrameLoaderClient::detachedFromParent1() {  }
void FrameLoaderClient::detachedFromParent2() {  }
void FrameLoaderClient::detachedFromParent3() { m_frame  = 0; }

void FrameLoaderClient::dispatchDidHandleOnloadEvents() {  }
void FrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad() 
{
	FLTRACE("FrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad\n");
	notImplemented();
}

void FrameLoaderClient::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url, double, double) 
{ 
	notImplemented(); 
}

void FrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    Palm::WebPage* page = m_frame->page();
    if (!page)
        return;

    Palm::WebPageClient* pageClient = page->client();
    if (!pageClient)
        return;

    // FIXME: Do we allow viewport setting from sub-frames?
    if (m_frame != page->mainFrame())
        return;

    // Update the URL.
    DocumentLoader* loader = core(m_frame)->loader()->documentLoader();
    if (loader) {
        CString titleString = core(m_frame)->document()->title().utf8();
        CString urlString = loader->url().prettyURL().utf8();
        if (m_lastUrlSent != urlString || m_lastTitleSent != titleString) {
            pageClient->urlTitleChanged(urlString.data(), titleString.data());
            m_lastTitleSent = titleString;
            m_lastUrlSent = urlString;
        }
    }
}

void FrameLoaderClient::dispatchWillClose() { notImplemented(); }

void FrameLoaderClient::dispatchDidReceiveIcon()
{
	FLTRACE("FrameLoaderClient::dispatchDidReceiveIcon\n");
    // WebKitPalmPage* page = getPageFromFrame(m_frame);
    // GREG g_signal_emit_by_name(page, "icon_loaded", m_frame);
	notImplemented();
}

void FrameLoaderClient::dispatchDidStartProvisionalLoad()
{
	FLTRACE("FrameLoaderClient::dispatchDidStartProvisionalLoad\n");
}

void FrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
	FLTRACE("FrameLoaderClient::dispatchDidReceiveTitle: '%s'\n", title.utf8().data());
    notImplemented();
}

void FrameLoaderClient::dispatchDidCommitLoad() 
{ 
	FLTRACE("FrameLoaderClient::dispatchDidCommitLoad\n");
	notImplemented(); 
}

/**
 * Called after each document has finished loading.
 */
void FrameLoaderClient::dispatchDidFinishDocumentLoad() 
{ 
	Palm::WebPage* page = m_frame->page();
	if( isMainFrame() ) {
		// Only happens once for the mainFrame, but there are other loads that finish
		// after the main frame (and some before).
		FLTRACE("FrameLoaderClient::dispatchDidFinishDocumentLoad: The main frame.\n");
	}
	else {
		FLTRACE("FrameLoaderClient::dispatchDidFinishDocumentLoad: child frame.\n");
	}

    // assume page load is completed when called in main frame
    if (page && page->client() && m_frame == page->mainFrame()) {
            page->client()->didFinishDocumentLoad();  
    }
}

void FrameLoaderClient::dispatchDidFirstLayout() { notImplemented(); }
void FrameLoaderClient::dispatchShow() { notImplemented(); }
void FrameLoaderClient::cancelPolicyCheck() { notImplemented(); }

void FrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader* loader) 
{
	FLTRACE("FrameLoaderClient::dispatchDidLoadMainResource.\n");
}

void FrameLoaderClient::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClient::clearUnarchivingState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClient::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClient::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->url()); }

void FrameLoaderClient::finishedLoading(DocumentLoader* documentLoader) 
{ 
	ASSERT(documentLoader->frame());
	
	if( m_frame->page()->mainFrame() == m_frame ) {
		FLTRACE("FrameLoaderClient::finishedLoading: The main frame.\n");
	}
	// Setting the encoding on the frame loader is our way to get work done that is normally done
    // when the first bit of data is received, even for the case of a document with no data (like about:blank).
    String encoding = documentLoader->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = documentLoader->response().textEncodingName();
    documentLoader->frameLoader()->writer()->setEncoding(encoding, userChosen);

    if (m_pluginView) {
        if (m_pluginView->status() == PluginStatusLoadedSuccessfully)
            m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClient::finalSetupForReplace(DocumentLoader*) 
{ 
	notImplemented(); 
}

void FrameLoaderClient::setDefersLoading(bool) 
{ 
	notImplemented(); 
}

bool FrameLoaderClient::isArchiveLoadPending(ResourceLoader*) const 
{ 
	notImplemented(); 
	return false; 
}

void FrameLoaderClient::cancelPendingArchiveLoad(ResourceLoader*) 
{ 
	notImplemented(); 
	
}

void FrameLoaderClient::clearArchivedResources() 
{ 
	notImplemented(); 
}

/**
 * See if this frame loader client can handle the request.
 * Seems to be called once for each client's main URL.
 *
 * @return true if this request can be handled, false if not.
 */
bool FrameLoaderClient::canHandleRequest(const ResourceRequest& req) const 
{
	CString url = req.url().string().utf8();
	CString protocol = req.url().protocol().utf8();

	bool canHandle = m_frame->page()->client()->shouldHandleScheme(protocol.data());

	if (!canHandle) {
		Palm::WebPageClient* client =  m_frame->page()->client();
		if (client) {
			client->mimeNotHandled("unknown", url.data());
		}
	}
	
	FLTRACE( "FrameLoaderClient::canHandleRequest: %s (%s) = '%c'\n", url.data(), protocol.data(), canHandle ? 'Y' : 'N' );

	return canHandle;
}

/**
 * Determine if a MIME type is invalid.
 */
bool FrameLoaderClient::mimeIsInvalid(const String& mimeString)
{
	return mimeString.isEmpty() || equalIgnoringCase( mimeString, String("unknown") );
}

/**
 * Called once the mime type for a resource has been determined. We decide here if we want to use, download, or ignore
 * the resource request. If a resource request is ignored then the download will be cancelled and cannotShowMIMETypeError
 * will be called. The "policy" functions are for security related decisions like cross-domain-script attacks.
 *
 * @remarks This is called before canShowMIMEType.
 * @remarks Called only for the main frame's URL and not for every referenced resource from that URL.
 * @remarks Seems to be called once for each frame's main URL.
 */
void FrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String& mimeString, const ResourceRequest& req)
{
	CString url = req.url().string().utf8();
	CString mime = mimeString.utf8();

	FLTRACE( "FrameLoaderClient::dispatchDecidePolicyForMIMEType url=%s mime='%s'\n", url.data(), mime.data() ); 

	PolicyAction action = PolicyUse;

	// Only make policy decisions for the main document. The loader has alrady called canShowMIMEType()
	// for this resource so we know we can handle it if we want.
	Palm::WebPageClient* client = m_frame->page()->client();

	if( mimeIsInvalid(mimeString) ) {
		FLTRACE("FrameLoaderClient::dispatchDecidePolicyForMIMEType: Unknown MIME type '%s' for %s\n",
					mime.data(), url.data());
	}
	else if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeString) ||
		(PalmBrowserSettings()->plugins_enabled && PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeString))) {

		// No need to ask about this because WebKit can handle these.
	}
	else if (MIMETypeRegistry::isSupportedImageMIMEType(mimeString)) {
		if (client && !client->displayStandaloneImages() && isMainFrame()) {
			action = PolicyIgnore;
		}
	}

	if (client && action == PolicyIgnore) {
		FLTRACE( "FrameLoaderClient::dispatchDecidePolicyForMIMEType Ignoring url=%s mime='%s'\n",
				url.data(), mime.data() );
		// Normally we'd return an error and send the message in dispatchUnableToImplementPolicy, but
		// that routine doesn't get the MIME type passed to it.
		client->mimeHandoffUrl(mime.data(), url.data());
	}

	if (policyFunction) {
		(core(m_frame)->loader()->policyChecker()->*policyFunction)(action);
	}
}

/**
 * Can WebKit show this specified MIME type?
 *
 * @remarks Called after dispatchDecidePolicyForMIMEType.
 * @remarks Called only for the main frame's resource and not for every referenced resource from that resource.
 */
bool FrameLoaderClient::canShowMIMEType(const String& mimeType) const 
{
	// If we're not given a MIME type the assume we can show it.
	bool canShow = mimeIsInvalid(mimeType) ||
		MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType)
		|| (MIMETypeRegistry::isSupportedImageMIMEType(mimeType) && (!isMainFrame() ||
					m_frame->page()->client()->displayStandaloneImages()))
        || PalmBrowserSettings()->plugins_enabled && PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType);
	
	FLTRACE( "FrameLoaderClient::canShowMIMEType '%s' = %c.\n", mimeType.utf8().data(), canShow ? 'Y' : 'N');

	// Normally a good place to inform our host that we can't handle a MIME type, but we don't have the URL 
	// at this point in time. dispatchDecidePolicyForMIMEType (already called) will send the notification.

	return canShow;
}

bool FrameLoaderClient::representationExistsForURLScheme(const String& scheme) const 
{ 
	FLTRACE( "FrameLoaderClient::representationExistsForURLScheme '%s'\n", scheme.utf8().data() );
	notImplemented();
	return false;
}

String FrameLoaderClient::generatedMIMETypeForURLScheme(const String& scheme) const 
{ 
	FLTRACE( "FrameLoaderClient::generatedMIMETypeForURLScheme '%s'\n", scheme.utf8().data() );
	notImplemented();
	return String(); 
}

void FrameLoaderClient::provisionalLoadStarted()
{
	FLTRACE("FrameLoaderClient::provisionalLoadStarted.\n");
    notImplemented();
}

void FrameLoaderClient::didFinishLoad() 
{
	FLTRACE("FrameLoaderClient::didFinishLoad.\n");
    notImplemented();
}

void FrameLoaderClient::prepareForDataSourceReplacement() 
{ 
	notImplemented(); 
}

void FrameLoaderClient::setTitle(const String& title, const KURL& url)
{
	FLTRACE("FrameLoaderClient::setTitle: '%s'.\n", title.utf8().data());

	Palm::WebFrameClient* frameClient = m_frame->client();
	if (!frameClient)
		return;
	
    CString titleString = title.utf8(); 
    CString urlString = url.prettyURL().utf8();
	
	FLTRACE( "%s\n", urlString.data() );
	
	if (m_lastUrlSent != urlString || m_lastTitleSent != titleString) {
		frameClient->urlTitleChanged(urlString.data(), titleString.data());
		m_lastUrlSent = urlString;
		m_lastTitleSent = titleString;
	}		
}

void FrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long  identifier, int lengthReceived) 
{ 
	notImplemented(); 
}

/**
 * This appears to be called after every resource referenced by a document is loaded.
 */
void FrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long  identifier) 
{
	if( m_frame->page()->mainFrame() == m_frame ) {
		FLTRACE("dispatchDidFinishLoading: '%s', id:%lu\n", loader->request().url().prettyURL().utf8().data(), identifier);
	}
	// This too chatty and we don't care about the info.
#if 0
	FLTRACE("dispatchDidFinishLoading: '%s', id:%lu\n", loader->request().url().prettyURL().utf8().data(), identifier);
	Palm::WebPage* page = m_frame->page();

	if (loader && page && page->client()) {
		page->client()->dispatchDidFinishLoading(loader->requestURL().string().utf8().data());
	}
#endif
    Palm::WebPage* page = m_frame->page();

    if (page && page->client()) {
        page->client()->loadActive();
    }
}

void FrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, unsigned long  identifier, const ResourceError& err) 
{ 
	CString domain = err.domain().utf8();
	CString url = err.failingURL().utf8();
	CString desc = err.localizedDescription().utf8();

	FLTRACE("FrameLoaderClient::dispatchDidFailLoading: domain:'%s', err:%d, url:'%s', msg:'%s'.\n", 
			domain.data(), err.errorCode(), url.data(), desc.data());
	
	if( m_frame )
	{
		Palm::WebPage* page = m_frame->page();
	
		if (page && page->client()) {
			page->client()->dispatchFailedLoad(domain.data(), err.errorCode(), url.data(), desc.data());
		}
	}
}

bool FrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest&, const ResourceResponse&, int length)
{
	FLTRACE("FrameLoaderClient::dispatchDidLoadResourceFromMemoryCache: '%s'.\n", loader->requestURL().string().utf8().data());
	notImplemented();
	return false;
}

void FrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& err)
{
	FLTRACE("FrameLoaderClient::dispatchDidFailProvisionalLoad: domain:'%s', err:%d, url:'%s', msg:'%s'.\n", 
			err.domain().utf8().data(), err.errorCode(),
			err.failingURL().utf8().data(), err.localizedDescription().utf8().data());
	notImplemented();
}

void FrameLoaderClient::dispatchDidFailLoad(const ResourceError& err)
{
	CString domain = err.domain().utf8();
	CString url = err.failingURL().utf8();
	CString desc = err.localizedDescription().utf8();

	FLTRACE("FrameLoaderClient::dispatchDidFailLoad: domain:'%s', err:%d, url:'%s', msg:'%s'.\n", 
			domain.data(), err.errorCode(), url.data(), desc.data());
	Palm::WebPage* page = m_frame->page();

	if (page && page->client()) {
		page->client()->dispatchFailedLoad(domain.data(), err.errorCode(), url.data(), desc.data());
	}
}

void FrameLoaderClient::download(ResourceHandle* handle, const ResourceRequest& req1, const ResourceRequest& req2, const ResourceResponse& resp) 
{
	// We don't use WebKit's built-in download capability. It's done in LunaSysMgr using curl for all apps.
	FLTRACE("FrameLoaderClient::download.\n");
	notImplemented();
}

ResourceError FrameLoaderClient::cancelledError(const ResourceRequest& request) 
{ 
    return ResourceError("", Palm::ERR_WK_FLOADER_CANCELLED, request.url().prettyURL(), "Resource load cancelled" );
}

ResourceError FrameLoaderClient::blockedError(const ResourceRequest& request)
{
	FLTRACE("FrameLoaderClient::blockedError: '%s'\n", request.url().prettyURL().utf8().data());
	return ResourceError("", Palm::ERR_WK_FLOADER_BLOCKED, request.url().prettyURL(), "Blocked URL");
}

ResourceError FrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
	FLTRACE("FrameLoaderClient::cannotShowURLError: '%s'\n", request.url().prettyURL().utf8().data());

	return ResourceError("", Palm::ERR_WK_FLOADER_CANNOT_SHOW, request.url().prettyURL(), "Cannot Show URL");
}

// We get called here when PolicyIgnore is set on a ResourceRequest.
ResourceError FrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest& request) 
{ 
    FLTRACE("FrameLoaderClient::interruptForPolicyChangeError: '%s'\n", request.url().prettyURL().utf8().data());
	Palm::WebPage* page = m_frame->page();
	Palm::WebPageClient* pageClient = page->client();

	if (pageClient) {
		pageClient->loadProgress(100);
		pageClient->loadStopped();
	}
    
	m_lastPercentSent=100;
	return ResourceError();
}

/**
 * Called when a MIME type cannot be shown.
 *
 * @remarks Is only called when canShowMIMEType() returns false. Is <em>not</em> called when
 *          a MIME type is not shown because of policy (dispatchDecidePolicyForMIMEType).
 */
ResourceError FrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response) 
{
	FLTRACE("FrameLoaderClient::cannotShowMIMETypeError: '%s'\n", response.url().prettyURL().utf8().data());

	Palm::WebPageClient* client = m_frame->page()->client();
	if (client) {
		client->mimeNotHandled( response.mimeType().utf8().data(), response.url().string().utf8().data() );
	}
	return ResourceError("", Palm::ERR_WK_FLOADER_CANNOT_SHOW_MIME, response.url().prettyURL(), "Cannot Show MIME type");
}

ResourceError FrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response) 
{ 
	FLTRACE("FrameLoaderClient::fileDoesNotExistError: '%s'\n", response.url().prettyURL().utf8().data());
	return ResourceError("", Palm::ERR_SYS_FILE_DOESNT_EXIST, response.url().prettyURL(), "File does not exist");
}

ResourceError FrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response)
{
	return ResourceError("", Palm::ERR_WK_FLOADER_CANNOT_SHOW_MIME, response.url().prettyURL(), "Plugin will handle load");
}

bool FrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, unsigned long  identifier)
{
	notImplemented();
	return false;
}

void FrameLoaderClient::dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const ScriptString&)
{
}

void FrameLoaderClient::documentElementAvailable()
{
}

void FrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout()
{
}

bool FrameLoaderClient::shouldFallBack(const ResourceError& error) 
{ 
	return error.errorCode() != Palm::ERR_WK_FLOADER_CANCELLED;
}

bool FrameLoaderClient::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL& originalURL) const 
{ 
	notImplemented(); 
	return false; 
}

bool FrameLoaderClient::canCachePage() const 
{ 
	return true;
}

Frame* FrameLoaderClient::dispatchCreatePage() 
{
	FLTRACE("FrameLoaderClient::dispatchCreatePage\n");

    // We want to create a new page instance (webkit_page_new) but
	// we need to do it HERE and return the Frame* reference now. This is
	// because JavaScript maintains a logic reference to it ...

	Palm::WebPage* page = m_frame->page();
	Palm::WebPageClient* pageClient = page->client();

	if (!pageClient)
		return 0;

	Palm::WebPage* newPage = pageClient->createPage(-1, -1, 0, 0);
    if (newPage)
        return core(newPage->mainFrame());
	
	return 0;
}

/**
 * Called if the request cannot be implemented (like if canHandleRequest() returns false or 
 * dispatchDecidePolicyForMIMEType() return PolicyIgnore).
 */
void FrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError& err) 
{ 
	FLTRACE("FrameLoaderClient::dispatchUnableToImplementPolicy: domain:'%s', err:%d, url:'%s', msg:'%s'\n", 
			err.domain().utf8().data(), err.errorCode(),
			err.failingURL().utf8().data(), err.localizedDescription().utf8().data());

	// Normally this is a perfect place to inform our host of an unsupported main documet
	// MIME type. Unfortunately that MIME type isn't passed as an argument.
}

void FrameLoaderClient::setMainDocumentError(DocumentLoader* loader, const ResourceError& err) 
{
	CString domain = err.domain().utf8();
	CString url = err.failingURL().utf8();
	CString desc = err.localizedDescription().utf8();

	FLTRACE("FrameLoaderClient::setMainDocumentError: domain:'%s', err:%d, url:'%s', msg:'%s'\n", 
			domain.data(), err.errorCode(), url.data(), desc.data());
	Palm::WebPage* page = m_frame->page();

	if (page && page->client() && isMainFrame()) {
		page->client()->setMainDocumentError(domain.data(), err.errorCode(), url.data(), desc.data());
	}
}


void FrameLoaderClient::startDownload(const ResourceRequest& req) 
{ 
	FLTRACE( "FrameLoaderClient::startDownload: %s (%s)\n", req.url().string().utf8().data(), req.url().protocol().utf8().data() );
}

void FrameLoaderClient::downloadStart( const char* url )
{
	FLTRACE("FrameLoaderClient::downloadStart: '%s'\n", url);
}

void FrameLoaderClient::downloadProgress( const char* url, unsigned long bytesSoFar, unsigned long bytesTotal )
{
	FLTRACE("FrameLoaderClient::downloadProgress: '%s'\n", url);
}

void FrameLoaderClient::downloadError( const char* url, const char* errMsg )
{
	FLTRACE("FrameLoaderClient::downloadProgress: '%s'\n", url);
}

void FrameLoaderClient::downloadFinished( const char* mimeType, const char* url, const char* tmpFilePath )
{
	FLTRACE("FrameLoaderClient::downloadFinished: '%s'\n", url);
}

void FrameLoaderClient::downloadCancel( const char* url )
{
	FLTRACE("FrameLoaderClient::downloadCancel: '%s'\n", url);
}


void FrameLoaderClient::updateGlobalHistory()
{
	if( isMainFrame() )
	{
		// ???
		CString utf8Url = core(m_frame)->document()->url().prettyURL().utf8();
		FLTRACE( "FrameLoaderClient::updateGlobalHistory: '%s'\n", utf8Url.data() );
		Palm::WebPage* page = m_frame->page();
	
		if (page && page->client()) {
			page->client()->updateGlobalHistory(utf8Url.data(), false );
		}
	}
}

void FrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    // ???
}

/* ???
void FrameLoaderClient::updateGlobalHistoryForStandardLoad(const KURL& url)
{
	CString utf8Url = url.prettyURL().utf8();
	FLTRACE( "FrameLoaderClient::updateGlobalHistoryForStandardLoad: '%s'\n", utf8Url.data() );
	Palm::WebPage* page = m_frame->page();

	if (page && page->client()) {
		page->client()->updateGlobalHistory(utf8Url.data(), false );
	}
}

void FrameLoaderClient::updateGlobalHistoryForReload(const KURL& url)
{
	CString utf8Url = url.prettyURL().utf8();
	FLTRACE( "FrameLoaderClient::updateGlobalHistoryForReload: '%s'\n", utf8Url.data() );
	Palm::WebPage* page = m_frame->page();

	if (page && page->client()) {
		page->client()->updateGlobalHistory(utf8Url.data(), true);
	}
}
*/


/**
 * Is the frame that this client is loading for the main frame?
 */
bool FrameLoaderClient::isMainFrame() const
{
	ASSERT(m_frame);
	ASSERT(m_frame->page());
	return m_frame->page()->mainFrame() == m_frame;
}

void FrameLoaderClient::transitionToCommittedForNewPage()
{
    ASSERT(m_frame);

	Palm::WebPage* page = m_frame->page();
 	ASSERT(page);

	Palm::WebView* view = page->view();	

	// Page without a view. we don't create a frameView for it
	// Needed to support faceless apps
	if (!view || !view->client())
		return;

	Palm::WebViewClient* viewClient = view->client();

    bool isMainFrame = m_frame == page->mainFrame();
	WebCore::Frame* coreFrame = core(m_frame);

	if (isMainFrame && coreFrame->view())
        coreFrame->view()->setParentVisible(false);

	coreFrame->setView(0);
    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        if (PalmBrowserSettings()->runningInBrowserServer)
            viewClient->zoomedContents(0,0,0,0,0); //Reset the viewport when creating a new FrameView in BS
        int width, height;
        viewClient->getWindowSize(width, height);
        if (PalmBrowserSettings()->defaultVisibleHeight > 0)
        	height = PalmBrowserSettings()->defaultVisibleHeight;

        frameView = FrameView::create(coreFrame, IntSize(width, height));
		frameView->setContainingWindow(view);
		frameView->setParentVisible(true);
		frameView->setZoomFactor(PalmBrowserSettings()->defaultZoomFactor, ZoomPage);
		frameView->setMaxVisibleHeightWhenScaled(PalmBrowserSettings()->defaultVisibleHeight);
	}
    else
        frameView = FrameView::create(coreFrame);

	frameView->setMarginWidth(0);
	frameView->setMarginHeight(0);
	frameView->setTransparent(view->getTransparent());

    coreFrame->setView(frameView);

    if (isMainFrame)
        frameView->setParentVisible(true);

    if (coreFrame->ownerRenderer())
        coreFrame->ownerRenderer()->setWidget(frameView);

    // Standard approach
	//coreFrame->createView(IntSize(width, height), Color(), view->getTransparent(), IntSize(), 0);
}

std::string decode_string( const String& str )
{
	std::string r;
	
	unsigned i = 0;
	while( i < str.length() )
	{
		if( str[i]=='%' )
		{
			unsigned short hex;
			hex = ( (char)(str[i+1]) - '0' ) << 4 ;
			hex |= ( (char)(str[i+2]) - '0') ;			
			r += (char)hex ;
			i += 2;
		}
		else
			r += (char)str[i];
		i++;
	}
	
	return r;
}

void ParseQueryString( const String& qs, QueryStringItemSet& outItems )
{
	unsigned int i = 0;

	if( qs[0] == '?' )
		i++;
	
	// <token>[=|&] ... 
	std::string token, value;
	
	// read until we hit '&' or '='
	int start_token = i;
	while( i < qs.length() )
	{
		if( qs[i] == '&' )
		{
			value = decode_string( qs.substring( start_token, i-start_token ) );
			outItems[token] = value;
			token = value= "";
			start_token = i+1;
		}
		else if( qs[i] == '=' )
		{
			token = decode_string( qs.substring( start_token, i-start_token ) );
			start_token = i+1;				
		}
		i++;
	}
	outItems[token] = decode_string( qs.substring( start_token, i-start_token ) );
}

void FrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void FrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

/**
 * The "open event" is just a trick we use so that our FrameLoaderClient can tell
 * the difference between a load that is happening as a result of somebody calling
 * our WebPage::load function and a load that is initiated via window.location. WebPage::load
 * will attach this event to the load request and we can then use it to verify the 
 * origin of the load request.
 */
WTF::PassRefPtr<WebCore::Event> FrameLoaderClient::getOpenUrlEvent()
{
	static RefPtr<Event> s_event;

	if (s_event == NULL) {
		s_event = Event::create();
	}

	return s_event;
}

void FrameLoaderClient::dispatchDidPushStateWithinPage()
{
}

void FrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
}

void FrameLoaderClient::dispatchDidPopStateWithinPage()
{
}

void FrameLoaderClient::dispatchDidChangeIcons()
{
}

void FrameLoaderClient::dispatchWillSendSubmitEvent(WebCore::HTMLFormElement*)
{
}		

void FrameLoaderClient::dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClient::dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClient::dispatchDidChangeBackForwardIndex() const
{
}

void FrameLoaderClient::didTransferChildFrameToNewDocument()
{
}




} // namespace WebKit

