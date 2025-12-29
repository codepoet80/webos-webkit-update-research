/* ============================================================
 * Date  : 2008-09-22
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
#include "palmwebglobal.h"
#include "palmwebpage.h"

#include "ChromeClientPalm.h"
#include "ContextMenuClientPalm.h"
#include "DeviceOrientationClientPalm.h"
#include "DeviceMotionClientPalm.h"
#include "DragClientPalm.h"
#include "EditorClientPalm.h"
#include "FocusController.h"
#include "FrameLoaderClientPalm.h"
#include "InspectorClientPalm.h"
#include "FrameLoader.h"
#include "BackForwardList.h"
#include "Frame.h"
#include "FormState.h"
#include "HTMLFormElement.h"
#include "InspectorController.h"
#include "KURL.h"
#include "NavigationAction.h"
#include "PlatformString.h"
#include "Settings.h"
#include "SubstituteData.h"
#include "HTMLImageElement.h"
#include "CachedImage.h"
#include "RenderImage.h"
#include "HTMLAllCollection.h"
#include "RenderLayer.h"
#include "Cache.h"
#include "V8Proxy.h"

#include <set>

#if USE(V8)
	// V8
	#include "v8.h"
#else
	// JSC
	#include "JSValue.h"
	#include "JSObject.h"
	#include "bridge/runtime.h"
	#include "runtime_root.h"
	#include "runtime_object.h"
	#include "c_instance.h"
	#include "JSDOMBinding.h"
	#include "JSDOMWindow.h"
	#include "DOMWindow.h"
	#include "CallFrame.h"
	#include "ScriptController.h"
	#include "ScriptSourceCode.h"
	#include "ScriptValue.h"
#endif


//#include "npapi.h"
//#include "npruntime.h"
//#include "npruntime_impl.h"

#include "palmwebpageprivate.h"
#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "palmwebpageclient.h"
#include "webkitpalmsettings.h"
#include <lunaservice.h>
#include <pbnjson.hpp>
#include <malloc.h>

#if USE(ACCELERATED_COMPOSITING) && PLATFORM(PG) && !defined(WEBOS_DESKTOP)
#include <napp/NWindow.h>
#endif

using namespace WebCore;

WebCore::Page* core(Palm::WebPage* p)
{
	return Palm::WebPagePrivate::core(p);
}

Palm::WebPage* kit(WebCore::Page* p)
{
	return Palm::WebPagePrivate::kit(p);
}

namespace Palm
{

// The group name identifies a namespace of pages. We only use
// one page group, just like it's done in chromium code
// (see WebKit/chromium/src/WebViewImpl.cpp).
const char* pageGroupName = "default";

WebPagePrivate::WebPagePrivate(WebPage* p)
	: parent(p)
	, view(0)
	, mainFrame(0)
	, client(0)
	, m_logOnNextPaint(false)
{
#if USE(V8)
    // Creation of a page implies UI activity.  Defer GC for a bit:
    WebGlobal::avoidGC(WebGlobal::AVOID_OLDGEN);
#endif
	chromeClient = new WebKit::ChromeClient(parent);
	contextMenuClient = new WebKit::ContextMenuClient();
	editorClient = new WebKit::EditorClient(parent);
	dragClient = new WebKit::DragClient();
	inspectorClient = new WebKit::InspectorClient(parent);
    deviceOrientationClient = new WebCore::DeviceOrientationClientPalm(parent);
    deviceMotionClient = new WebCore::DeviceMotionClientPalm(parent);

    Page::PageClients pageClients;
	pageClients.chromeClient = chromeClient;
	pageClients.contextMenuClient = contextMenuClient;
	pageClients.editorClient = editorClient;
	pageClients.dragClient = dragClient;
	pageClients.inspectorClient = inspectorClient;
    pageClients.deviceOrientationClient = deviceOrientationClient;
    pageClients.deviceMotionClient = deviceMotionClient;
	page = new Page( pageClients );
    if (PalmBrowserSettings()->runningInBrowserServer)
        page->setGroupName(pageGroupName);
}

WebPagePrivate::~WebPagePrivate()
{
	delete page;
	delete mainFrame;
}

void WebPagePrivate::init(bool createRenderer)
{
	mainFrame = new WebFrame(parent, createRenderer);
	initBrowserFuncs();
}

void WebPagePrivate::initBrowserFuncs()
{
	// Setup the browser funcs for use by native JS objects
	memset(&browserFuncs, 0, sizeof(NPNetscapeFuncs));
	browserFuncs.size = sizeof (NPNetscapeFuncs);
	browserFuncs.version = NP_VERSION_MINOR;
	browserFuncs.geturl = NPN_GetURL;
	browserFuncs.posturl = (NPN_PostURLProcPtr)NPN_PostURL;
	browserFuncs.requestread = NPN_RequestRead;
	browserFuncs.newstream = NPN_NewStream;
	browserFuncs.write = (NPN_WriteProcPtr)NPN_Write;
	browserFuncs.destroystream = NPN_DestroyStream;
	browserFuncs.status = NPN_Status;
	browserFuncs.uagent = NPN_UserAgent;
	browserFuncs.memalloc = (NPN_MemAllocProcPtr)NPN_MemAlloc;
	browserFuncs.memfree = NPN_MemFree;
	browserFuncs.memflush = (NPN_MemFlushProcPtr)NPN_MemFlush;
	browserFuncs.reloadplugins = NPN_ReloadPlugins;
	browserFuncs.geturlnotify = NPN_GetURLNotify;
	browserFuncs.posturlnotify = (NPN_PostURLNotifyProcPtr)NPN_PostURLNotify;
	browserFuncs.getvalue = NPN_GetValue;
	browserFuncs.setvalue = NPN_SetValue;
	browserFuncs.invalidaterect = NPN_InvalidateRect;
	browserFuncs.invalidateregion = NPN_InvalidateRegion;
	browserFuncs.forceredraw = NPN_ForceRedraw;
	browserFuncs.getJavaEnv = NPN_GetJavaEnv;
	browserFuncs.getJavaPeer = NPN_GetJavaPeer;
	browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
	browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
#if USE(HIDEV8)
	browserFuncs.releasevariantvalue = NPN_ReleaseVariantValue;
	browserFuncs.getstringidentifier = NPN_GetStringIdentifier;
	browserFuncs.getstringidentifiers = NPN_GetStringIdentifiers;
	browserFuncs.getintidentifier = NPN_GetIntIdentifier;
	browserFuncs.identifierisstring = NPN_IdentifierIsString;
	browserFuncs.utf8fromidentifier = NPN_UTF8FromIdentifier;
	browserFuncs.intfromidentifier = NPN_IntFromIdentifier;
	browserFuncs.createobject = NPN_CreateObject;
	browserFuncs.retainobject = NPN_RetainObject;
	browserFuncs.releaseobject = NPN_ReleaseObject;
	browserFuncs.invoke = NPN_Invoke;
	browserFuncs.invokeDefault = NPN_InvokeDefault;
	browserFuncs.evaluate = NPN_Evaluate;
	browserFuncs.getproperty = NPN_GetProperty;
	browserFuncs.setproperty = NPN_SetProperty;
	browserFuncs.removeproperty = NPN_RemoveProperty;
	browserFuncs.hasproperty = NPN_HasMethod;
	browserFuncs.hasmethod = NPN_HasProperty;
	browserFuncs.setexception = NPN_SetException;
	browserFuncs.enumerate = NPN_Enumerate;
#else
	browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
	browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
	browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
	browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
	browserFuncs.identifierisstring = _NPN_IdentifierIsString;
	browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
	browserFuncs.intfromidentifier = _NPN_IntFromIdentifier;
	browserFuncs.createobject = _NPN_CreateObject;
	browserFuncs.retainobject = _NPN_RetainObject;
	browserFuncs.releaseobject = _NPN_ReleaseObject;
	browserFuncs.invoke = _NPN_Invoke;
	browserFuncs.invokeDefault = _NPN_InvokeDefault;
	browserFuncs.evaluate = _NPN_Evaluate;
	browserFuncs.getproperty = _NPN_GetProperty;
	browserFuncs.setproperty = _NPN_SetProperty;
	browserFuncs.removeproperty = _NPN_RemoveProperty;
	browserFuncs.hasproperty = _NPN_HasMethod;
	browserFuncs.hasmethod = _NPN_HasProperty;
	browserFuncs.setexception = _NPN_SetException;
	browserFuncs.enumerate = _NPN_Enumerate;
#endif	
}

Page* WebPagePrivate::core(WebPage* p)
{
	return p->d->page;
}

WebPage* WebPagePrivate::kit(Page* p)
{
	return WebFramePrivate::kit(p->mainFrame())->page();	
}


WebPage::WebPage()
	: d(new WebPagePrivate(this))
{
	d->init(false);

	// no view... do not set view specific settings
    d->page->settings()->setLoadsImagesAutomatically(true);
	d->page->settings()->setPrivateBrowsingEnabled( PalmBrowserSettings()->private_browsing );
	d->page->settings()->setJavaScriptCanOpenWindowsAutomatically( PalmBrowserSettings()->java_script_can_open_windows_automatically );
    d->page->settings()->setJavaScriptEnabled( PalmBrowserSettings()->java_script_enabled );
    d->page->settings()->setPluginsEnabled( PalmBrowserSettings()->plugins_enabled );
	d->page->settings()->setDeveloperExtrasEnabled( PalmBrowserSettings()->enableDeveloperExtras );
	d->page->settings()->setUsesPageCache(true);
	d->page->settings()->setFrameFlatteningEnabled(PalmBrowserSettings()->enableFrameFlattening);
	d->page->settings()->setLocalStorageEnabled(PalmBrowserSettings()->enableLocalStorage);
	d->page->settings()->setLocalStorageDatabasePath(PalmBrowserSettings()->localStoragePath);
	d->page->settings()->setOfflineWebApplicationCacheEnabled(PalmBrowserSettings()->enableAppCache);
}


WebPage::WebPage(WebView* view)
	: d(new WebPagePrivate(this))
{
	d->view = view;
	d->init(true);

    d->page->settings()->setLoadsImagesAutomatically(true);
	d->page->settings()->setShrinksStandaloneImagesToFit(false);
    d->page->settings()->setMinimumFontSize(PalmBrowserSettings()->minFontSize);
    d->page->settings()->setMinimumLogicalFontSize(PalmBrowserSettings()->minLogicalFontSize);
    d->page->settings()->setShouldPrintBackgrounds(true);
	d->page->settings()->setPrivateBrowsingEnabled( PalmBrowserSettings()->private_browsing );
	d->page->settings()->setJavaScriptCanOpenWindowsAutomatically( PalmBrowserSettings()->java_script_can_open_windows_automatically );
    // Enable execCommand("copy/cut/paste") when running in WebAppMgr process.
    d->page->settings()->setJavaScriptCanAccessClipboard(PalmBrowserSettings()->runningInBrowserServer != true);
    d->page->settings()->setJavaScriptEnabled( PalmBrowserSettings()->java_script_enabled );
    d->page->settings()->setDefaultFixedFontSize(PalmBrowserSettings()->defaultFixedFontSize);
    d->page->settings()->setDefaultFontSize(PalmBrowserSettings()->defaultFontSize);
    d->page->settings()->setSerifFontFamily(PalmBrowserSettings()->defaultSerifFontFamilyName);
    d->page->settings()->setSansSerifFontFamily(PalmBrowserSettings()->defaultSansSerifFontFamilyName);
    d->page->settings()->setFixedFontFamily(PalmBrowserSettings()->defaultFixedFontFamilyName);
    d->page->settings()->setStandardFontFamily(PalmBrowserSettings()->defaultStandardFontFamilyName);
    d->page->settings()->setPluginsEnabled( PalmBrowserSettings()->plugins_enabled );
	d->page->settings()->setDeveloperExtrasEnabled( PalmBrowserSettings()->enableDeveloperExtras );
	d->page->settings()->setUsesPageCache(true);
	d->page->settings()->setFrameFlatteningEnabled(PalmBrowserSettings()->enableFrameFlattening);
	d->page->settings()->setDownloadableBinaryFontsEnabled(false);
	d->page->settings()->setShouldPaintCustomScrollbars(false);
	d->page->settings()->setLocalStorageEnabled(PalmBrowserSettings()->enableLocalStorage);
	d->page->settings()->setLocalStorageDatabasePath(PalmBrowserSettings()->localStoragePath);
	d->page->settings()->setOfflineWebApplicationCacheEnabled(PalmBrowserSettings()->enableAppCache);

	d->page->settings()->setWebSecurityEnabled(true);
	d->page->settings()->setAllowUniversalAccessFromFileURLs(true);
	d->page->settings()->setEnhancedViewportEnabled(PalmBrowserSettings()->enableEnhancedViewport);
	d->page->settings()->setShowDebugBorders(PalmBrowserSettings()->showCompositedLayerBorders);
}

WebPage::~WebPage()
{
	Document* doc = mainFrame()->d->frame->document();
	
	FrameLoader* loader = mainFrame()->d->frame->loader();
	if (loader)
		loader->detachFromParent();

	bool isDashboardWindow = doc->url().string().contains( "window=dashboard" );
	delete d;

	// the Page has been destroyed, and the v8context too. Now let's clip
	// all the bindings. This will accelerate freeing of the memory from the document.
	// We are using the doc pointer here simply as a key to a hashtable in the v8 bindings.
	WebCore::V8Proxy::destroyWrappersForDocument( doc );

	// Assume the Document object is now destroyed because destroying the wrappers will
	// likely cause it's reference count to reach zero.
	doc = NULL;

    // We've just free'd a lot of memory.  Give malloc a chance to trim the
    // heap.
    malloc_trim(0);
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    NPixmapPrivate::MallocTrim(0);
#endif
}

void WebPage::setClient(WebPageClient* client)
{
    d->client = client;
    if (d->mainFrame)
        d->mainFrame->setClient(client);
}

WebPageClient* WebPage::client() const
{
    return d->client;
}

WebView* WebPage::view() const
{
    return d->view;
}

WebFrame* WebPage::mainFrame() const
{
	return d->mainFrame;
}

void WebPage::load(const char* _url)
{
    String url = String::fromUTF8(_url);

    // URL cannot be empty. There is a crash otherwise.
    if (url.isEmpty())
        url = String("about:blank");

    KURL kurl(KURL(), url);

    // If path begins with a back slash set default protocol to "file"
    // For all other urls set it to http
    if (kurl.protocol().isEmpty() && kurl.host().isEmpty()) {
        String protocol;
        if (url.startsWith("/"))
            protocol = "file://";
        else
            protocol = "http://";

        url.insert(protocol, 0);
        KURL kurlTmp(KURL(), url);
        kurl = kurlTmp;
    }

    ResourceRequest request(kurl);
    core(d->mainFrame)->loader()->load(request, false);
}

void WebPage::stopLoading()
{
	if (FrameLoader* loader = d->mainFrame->d->frame->loader())
		loader->stopAllLoaders();
}

void WebPage::reload()
{
	if (FrameLoader* loader = d->mainFrame->d->frame->loader())
		loader->reload();    
}

void WebPage::loadString(const char* content, const char* contentMimeType,
						 const char* contentEncoding, const char* baseUrl)
{
	KURL url( ParsedURLString, baseUrl ? String::fromUTF8(baseUrl) : "" );
	RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(content,strlen(content));    
	SubstituteData substituteData(sharedBuffer.release(), contentMimeType ? String(contentMimeType) : "text/html", 
				contentEncoding ? String(contentEncoding) : "UTF-8" , KURL(ParsedURLString,"about:blank"), url);	
	core(d->mainFrame)->loader()->load(ResourceRequest(url), substituteData, false);
}

void WebPage::loadHtmlString(const char* content, const char* baseUrl)
{
	loadString(content, "text/html", "UTF-8", baseUrl);
}

bool WebPage::canGoBackward()
{
	return core(d->mainFrame)->page()->canGoBackOrForward(-1);
}

bool WebPage::canGoForward()
{
	return core(d->mainFrame)->page()->canGoBackOrForward(1);
}

void WebPage::goBackward()
{
    if (core(d->mainFrame)->page()->canGoBackOrForward(-1))
        core(d->mainFrame)->page()->goBackOrForward(-1);
}

void WebPage::goForward()
{
    if (core(d->mainFrame)->page()->canGoBackOrForward(1))
        core(d->mainFrame)->page()->goBackOrForward(1);
}

void WebPage::clearBackForwardList()
{
	BackForwardList* list = core(d->mainFrame)->page()->backForwardList();
	list->setEnabled(false);
	list->setEnabled(true);
}

void WebPage::inspect()
{
	Page* page = d->page;
	
	Frame* mainFrame = page->mainFrame();
	if( !mainFrame )
	{
		g_message( "mainFrame == 0" );
		return;
	}
	Document* doc = mainFrame->document();
	if( !doc )
	{
		g_message( "doc == 0" );
		return;
	}
	Node* node = doc->nodeWithAbsIndex(0);
	if( !node )
	{
		g_message( "node == 0" );
		return;
	}

    page->inspectorController()->inspect( node );
}

void WebPage::setPrivateBrowsingEnabled(bool enabled)
{ 
    d->page->settings()->setPrivateBrowsingEnabled(enabled);
}

void WebPage::setJavaScriptCanOpenWindowsAutomatically(bool enabled)
{
    d->page->settings()->setJavaScriptCanOpenWindowsAutomatically(enabled);
}

void WebPage::setJavaScriptEnabled(bool enabled) 
{
    d->page->settings()->setJavaScriptEnabled(enabled);
}

void WebPage::setPluginsEnabled(bool enabled)
{
    d->page->settings()->setPluginsEnabled(enabled);
}

void WebPage::setEnhancedViewportEnabled(bool enabled)
{
    // Only enable if enabled in the conf
    d->page->settings()->setEnhancedViewportEnabled(enabled && PalmBrowserSettings()->enableEnhancedViewport);
}

void WebPage::setIgnoreMetaRefreshTags(bool ignore)
{
    d->page->settings()->setIgnoreMetaRefreshTags(ignore);
}

void WebPage::inspectRemote(bool enable)
{
	// deprecated
}

void WebPage::closeInspector()
{
	d->page->inspectorController()->close();
}

void WebPage::throttle(unsigned int throttleLevelFrom0To100Percent, unsigned int minThrottleMS)
{
    Document* doc = mainFrame()->d->frame->document();
    doc->throttle(throttleLevelFrom0To100Percent, minThrottleMS);

    d->chromeClient->setCached(throttleLevelFrom0To100Percent == 0);
}

bool WebPage::evaluateScript(const char* script)
{
#if USE(V8)
	ScriptController* s = core(d->mainFrame)->script();
	if( s )
	{
        webOS::Reporter::Event evt_eval(core(d->mainFrame)->document(), "webkit.WebPage.evaluate", script);
		ScriptValue r = s->evaluate( WebCore::ScriptSourceCode( String::fromUTF8(script) ) );
        evt_eval.stop();
		
		logOnNextPaint();

		if( r.hasNoValue() )
			return false;
		
		if( r.isUndefined() )
			return false;
		
		return r.v8Value()->BooleanValue();
	}

/*
	bool succ=false;
	
	core(d->mainFrame)->scriptBridge()->evaluate( 
		String(), // filename
		0,
		String::fromUTF8(script), // code
		0, // Node*
		&succ );
	return succ;
*/	
#else

	ScriptController* s = core(d->mainFrame)->script();
	if( s )
	{
		ScriptValue r = s->evaluate( WebCore::ScriptSourceCode( String::fromUTF8(script) ) );
		JSC::JSValue v = r.jsValue();
		if( v.isBoolean() && v.getBoolean() )
			return true;
	}
	
#endif	
	 
	 return false;    
}

void WebPage::cancelDownload(const char* url)
{
}

std::string WebPage::getContents(bool markedUp)
{
	Page* page = d->page;
	Frame* mainFrame = page->mainFrame();
	String s = mainFrame->document()->toString();	
	return std::string( s.utf8().data() );
}

bool WebPage::createJsObject(void* objClass, const char* name, void** obj, void** browserFuncs)
{
	*browserFuncs = &d->browserFuncs;
	
	if( (*obj) )
		return false;
	
	
#if USE(V8)
	Frame* f = core(d->mainFrame);
	V8Proxy* proxy = V8Proxy::retrieve(f);
	v8::Handle<v8::Context> existingContext = proxy->context();
	if (existingContext.IsEmpty()) {
		g_debug("No existing context and not creating one");
		*obj = 0;
		return false;
	}
#endif
	
	// Create the javascript object
	*obj = d->browserFuncs.createobject(0, (NPClass*) objClass);
	if (!*obj)
		return false;
	
#if USE(V8)
	// set the context for this call.
	v8::HandleScope hs;
	v8::Handle<v8::Context> context = V8Proxy::context(f);
	v8::Context::Scope scope(context);
	
	v8::Handle<v8::Object> o = createV8ObjectForNPObject( (NPObject*)*obj, 0 );
	
	v8::Handle<v8::Object> global = context->Global();	
	global->Set(v8::String::New(name), o);

#else

	NPObject * npobject = (NPObject*)*obj;
	JSDOMWindow *window = toJSDOMWindow(core(d->mainFrame));
	JSC::ExecState* exec = window->globalExec();
	RefPtr<JSC::Bindings::RootObject> rootObj = core(d->mainFrame)->script()->bindingRootObject();

	// bridge/c/c_instance.h
	RefPtr<JSC::Bindings::Instance> inst = JSC::Bindings::CInstance::create( npobject, rootObj);
	
	// bridge/runtime.h
	JSC::JSObject* runtimeObject = inst->createRuntimeObject( exec );		
	
	JSC::PutPropertySlot slot;
	window->put( exec, JSC::Identifier(exec,name), runtimeObject, slot );
	//instance.release(); ???
	
#endif

	return true;
}


static 	std::set<CachedImage*> uniqueImages;
static 	std::set<RenderObject*> uniqueRenderObjects;

static void collectChildrenRenderObjects( RenderObject* o, WebPageStatistics& stats )
{
	RenderObject* c;

	if( o->isImage() )
	{
		RenderImage* ri = (RenderImage*)o;
		CachedImage* img = ri->cachedImage();
		if( img && uniqueImages.find(img) == uniqueImages.end() )
		{
			stats.numImages++;
			IntSize sz = img->imageSize(1.0f);
			stats.totalImagesBytes += ( sz.width() * sz.height() * 4 ) + img->encodedSize();
			uniqueImages.insert( img );
		}
	}
	
	RenderStyle* style = o->style();
	if( style )
	{
		//StyleImage* img = style->backgroundImage();
		//if( img && uniqueImages.find(img) == uniqueImages.end() )
		//{
			//stats.numImages++;
			//IntSize sz = img->imageSize(1.0f);
			//stats.totalImagesBytes += ( sz.width() * sz.height() * 4 ) + img->encodedSize();
			//uniqueImages.insert( img );
		//}
	}
	
	c = o->firstChild();
	while( c )
	{
		uniqueRenderObjects.insert( c );
		//stats.numRenderTreeNodes++;
		collectChildrenRenderObjects( c, stats );
		c = c->nextSibling();
	}			
}

static void walkRenderLayer( RenderLayer* layer, WebPageStatistics& stats )
{
	collectChildrenRenderObjects( layer->renderer(), stats );	
	RenderLayer* l = layer->firstChild();
	while( l )
	{
		stats.numRenderLayers++;
		walkRenderLayer(l,stats);
		l = l->nextSibling();
	}
}

bool WebPage::getStatistics( WebPageStatistics& outStats )
{
	Frame* f = core(d->mainFrame);
	Document* doc = f->document();

	uniqueImages.clear();
	uniqueRenderObjects.clear();
	
	outStats.numImages = 0;
	outStats.totalImagesBytes = 0;
	outStats.numRenderTreeNodes = 0;
	outStats.numRenderLayers = 0;
	
	
	// 1. Walk the render tree
	walkRenderLayer( doc->renderer()->enclosingLayer(), outStats ); // ??? layer() does not exist
	outStats.numRenderTreeNodes = uniqueRenderObjects.size();
	uniqueRenderObjects.clear();
	
	// 2. DOM Nodes
	RefPtr<HTMLAllCollection> allNodes = doc->all();
	outStats.numDOMNodes = allNodes->length();
			
	// 3. Images from <img>
	RefPtr<HTMLCollection> images = doc->images();
	for( unsigned i=0; i<images->length(); i++ ) 
	{
		HTMLImageElement* el = static_cast<HTMLImageElement*>( images->item(i) );
		if( el )
		{
			CachedImage* img = el->cachedImage();
			if( img )
			{
				if( uniqueImages.find( img ) == uniqueImages.end() )
				{		
					//outStats.numImages++;
					//IntSize sz = img->imageSize();
					//outStats.totalImagesBytes += ( sz.width() * sz.height() * 4 ) + img->encodedSize();
					//uniqueImages.insert( img );
				}
			}
			else
			{
				outStats.numImages++;
				outStats.totalImagesBytes += el->naturalWidth() * el->naturalHeight() * 4;
			}
		}
	}
	
	uniqueImages.clear();
	uniqueRenderObjects.clear();

	return true;
}

int WebPage::outstandingDatabaseTasks()
{
	return 0;
}

void WebPage::enableHttpProxy(const char* host, const int port)
{
    d->page->settings()->setHttpProxyHost(host);
    d->page->settings()->setHttpProxyPort(port);
    d->page->settings()->setHttpProxyEnabled(true);
}

void WebPage::disableHttpProxy()
{
    d->page->settings()->setHttpProxyEnabled(false);
}

void WebPage::forceNetworkInterface(const char* interfaceName)
{
	if(interfaceName) {
		d->page->settings()->setNetworkInterface(interfaceName);
		d->page->settings()->setForceNetworkInterfaceEnabled(true);
	} else {
		d->page->settings()->setForceNetworkInterfaceEnabled(false);		
	}
}

void WebPage::notifyTextEntryAutoCorrect ()
{
	static LSHandle* s_lsHandle;

	if (s_lsHandle == NULL) {
		if (LSRegister(NULL, &s_lsHandle, NULL)) {
			if (!LSGmainAttach(s_lsHandle, Palm::WebGlobal::mainLoop(), NULL)) {
				LSUnregister(s_lsHandle, NULL);
				s_lsHandle = NULL;
			}
		}
	}

	LSError error;
	LSErrorInit(&error);
	bool succeeded = LSCall(s_lsHandle, "luna://com.palm.audio/systemsounds/playFeedback",
			"{\"name\": \"text_entry_correction\", \"type\":\"text_entry_correction\"}",
			NULL,NULL, NULL, &error);

	if (!succeeded) {
		g_warning("Cannot play text correction sound: %s", error.message);
		LSErrorFree(&error);
	}
}

void WebPage::logOnNextPaint()
{
	d->m_logOnNextPaint = true;
}

double WebPage::getPageScaleFactor() const
{
	return d->page->settings()->getPageScaleFactor();
}

void WebPage::forceDNSServers(const char* dnsServers)
{
    if(dnsServers) {
        d->page->settings()->setForceDNSServersEnabled(true);
        d->page->settings()->setDNSServers(dnsServers);
    } else {
        d->page->settings()->setForceDNSServersEnabled(false);
    }
}

}
