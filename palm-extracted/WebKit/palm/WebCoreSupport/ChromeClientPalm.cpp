
#include "config.h"
#include "ChromeClientPalm.h"
#include "FloatRect.h"
#include "FrameLoadRequest.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "CString.h"
#include "FrameView.h"
#include "ScrollView.h"
#include "WindowFeatures.h"
#include "NotImplemented.h"
#include "DatabaseTracker.h"
#include "Frame.h"
#include "PopupMenuPalm.h"
#include "SearchPopupMenuPalm.h"
#include "ViewportArguments.h"
#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayer.h"
#endif

#include "palmwebpage.h"
#include "palmwebpageclient.h"
#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "palmwebview.h"
#include "palmwebviewprivate.h"
#include "palmwebviewclient.h"
#include "palmwebpageprivate.h"
#include "webkitpalmsettings.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "ApplicationCacheStorage.h"
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "webkitpalmtimer.h"
#endif

using namespace WebCore;

namespace WebKit {
	
	
ChromeClient::ChromeClient(Palm::WebPage* page)
: m_webPage(page)
#if ENABLE(REQUEST_ANIMATION_FRAME)
 , m_animationTimer(0)
 , m_animationPending(false)
 , m_animDeferred(false)
 , m_animCount(0)
#endif
 , m_paintPending(false)
 , m_visible(true)
 , m_cached(false)
{
}

ChromeClient::~ChromeClient()
{
    StopAnimationTimer();
}

void ChromeClient::chromeDestroyed()
{
	delete this;
}

FloatRect ChromeClient::windowRect()
{
    Palm::WebView* view = m_webPage->view();
    if (!view)
        return FloatRect();

    Palm::WebViewClient* viewClient = view->client();
    int width, height;
    viewClient->getScreenSize(width, height);

    return FloatRect(0.0f, 0.0f, float(width), float(height));
}

void ChromeClient::setWindowRect(const FloatRect& r)
{
    notImplemented();
}

FloatRect ChromeClient::pageRect()
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return FloatRect();

	Palm::WebViewClient* viewClient = view->client();
	int width, height;
	viewClient->getWindowSize(width, height);
    
    return FloatRect( 0.0f,0.0f, float(width), float(height) );
}

float ChromeClient::scaleFactor()
{
	WebCore::Frame* frame = core(m_webPage)->mainFrame();
	if (frame && frame->view()) {
		return frame->view()->zoomFactor();
	}
    return 1.0f;
}

void ChromeClient::focus()
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

	viewClient->focused();
}

void ChromeClient::unfocus()
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

	viewClient->unfocused();
}

PlatformWidget ChromeClient::platformPageClient() const
{
    return 0; // no platform window on device.
}
    
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClient::reachedMaxAppCacheSize(int64_t spaceNeeded)
{
	// vacuum takes long time to complete, plus it may not have significant
	// enough effect. Just clear the db.
	WebCore::cacheStorage().empty();
}
void ChromeClient::reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin* origin)
{
	
}
#endif

Page* ChromeClient::createWindow(Frame*, const FrameLoadRequest& frameLoadRequest,const WebCore::WindowFeatures& features)
{
    if (features.dialog) {
        printf("New Pages as dialogs are not supported\n");
        notImplemented();
        return 0;
    }

    /* TODO: FrameLoadRequest is not used */

	//printf("createWindow [creating a new sysmgr card view]... \n" );

	Palm::WebPageClient* pageClient = m_webPage->client();
	if (!pageClient)
		return 0;

	int width  = features.widthSet  ? (int) features.width  : -1;
	int height = features.heightSet ? (int) features.height : -1;

	Palm::WebPage* newPage = pageClient->createPage(width, height,
													frameLoadRequest.frameName().isEmpty() ? 0 :
													frameLoadRequest.frameName().utf8().data(),
													features.attributes.isEmpty() ? 0 :
													features.attributes.utf8().data());
    if (newPage)
        return core(newPage);
	
	return 0;
}

void ChromeClient::show()
{
    notImplemented();
}

bool ChromeClient::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClient::runModal()
{
    notImplemented();
}

void ChromeClient::setToolbarsVisible(bool)
{
}

bool ChromeClient::toolbarsVisible()
{
    return false;
}

void ChromeClient::setStatusbarVisible(bool)
{
}

bool ChromeClient::statusbarVisible()
{
    return false;
}

void ChromeClient::setScrollbarsVisible(bool)
{
}

bool ChromeClient::scrollbarsVisible() 
{
    return false;
}

void ChromeClient::setMenubarVisible(bool)
{
}

bool ChromeClient::menubarVisible()
{
    return false;
}

void ChromeClient::setResizable(bool)
{
}

void ChromeClient::closeWindowSoon()
{
	Palm::WebPageClient* pageClient = m_webPage->client();
	if (!pageClient)
		return;

	pageClient->closePageSoon();
}

bool ChromeClient::canTakeFocus(FocusDirection)
{
	return false;
}

void ChromeClient::takeFocus(FocusDirection)
{
	//unfocus();
}

bool ChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClient::runBeforeUnloadConfirmPanel(const WTF::String&, WebCore::Frame*)
{
    return false;
}

static Palm::MessageLevel ConvertMessageLevel(WebCore::MessageLevel level)
{
    return static_cast<Palm::MessageLevel>(static_cast<int>(level));
}

void ChromeClient::addMessageToConsole(WebCore::MessageSource source, WebCore::MessageType type,
								WebCore::MessageLevel level,  const WTF::String& message, 
								unsigned int lineNumber, const WTF::String& sourceID)
{
	Palm::WebPageClient* pageClient = m_webPage->client();
	if (pageClient)
	{
		Palm::MessageLevel lev = ConvertMessageLevel(level);
		pageClient->jsConsoleMessage( lev, message.utf8().data(), lineNumber, sourceID.utf8().data() );
		
        webOS::Reporter::Event::logEvent( core(m_webPage)->mainFrame()->document(), "console.msg", message.utf8().data() );
	}
}

void ChromeClient::runJavaScriptAlert(Frame*, const String& message)
{
	Palm::WebPageClient* pageClient = m_webPage->client();
	if (pageClient)
		pageClient->dialogAlert(message.utf8().data());
}

bool ChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
	Palm::WebPageClient* pageClient = m_webPage->client();
	if (pageClient)
        return pageClient->dialogConfirm(message.utf8().data());
	else
		return false;
}

bool ChromeClient::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
	Palm::WebPageClient* pageClient = m_webPage->client();
	if (pageClient) {
        bool retOk;
		std::string retVal;

		retOk = pageClient->dialogPrompt(message.utf8().data(), defaultValue.utf8().data(), retVal);
		if (retOk && !retVal.empty())
			result = retVal.c_str();

		return retOk;
	}
	else
		return false;
}

void ChromeClient::setStatusbarText(const String& string)
{
	if (string.isEmpty())
		return;
	
	if (!m_webPage || !m_webPage->client())
		return;

	m_webPage->client()->statusMessage(string.utf8().data());
}

bool ChromeClient::shouldInterruptJavaScript()
{
    return true;
}

bool ChromeClient::tabsToLinks() const
{
    return false;
}

IntRect ChromeClient::windowResizerRect() const
{
    return IntRect();
}

void ChromeClient::scroll(const IntSize& delta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    notImplemented();
}


void ChromeClient::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags)
{
    notImplemented();
}

void ChromeClient::setToolTip(const String&,WebCore::TextDirection)
{
    notImplemented();
}

void ChromeClient::didReceiveViewportArguments(WebCore::Frame* f, const WebCore::ViewportArguments& args) const
{
    if (!f)
        return;

    Palm::WebPage* page = kit( f->page() );
    if (!page)
        return;

    Palm::WebView* view = page->view();
    if (!view)
        return;
    
    Palm::WebViewClient* viewClient = view->client();
    if (!viewClient)
    	return;

    if (PalmBrowserSettings()->defaultZoomFactor != 1.0) {
        WebCore::Frame* coreFrame = core(page->mainFrame());
        // Do not allow viewport setting from sub-frames
        if (f != coreFrame)
            return;

        if (coreFrame && coreFrame->view()) {
        	if (args.width && args.width != ViewportArguments::ValueUndefined) {
				float newZoomFactor = f->page()->chrome()->windowRect().width() / args.width;
				float curZoomFactor = coreFrame->view()->pageZoomFactor();
				if (newZoomFactor != curZoomFactor)
					coreFrame->view()->setZoomFactor(newZoomFactor, ZoomPage);
        	}
        	if (args.height && args.height != ViewportArguments::ValueUndefined) {
				if (f->page()->chrome()->windowRect().height() == args.height) {
					coreFrame->view()->setMaxVisibleHeightWhenScaled(0);
					// resize the view to full height of virtual window
					int w, h;
					viewClient->getVirtualWindowSize(w, h);
					view->resize(w, h);
				}
        	}
        }
    }
    else {
   	    viewClient->viewportTagParsed(args.initialScale,
   	        args.minimumScale, args.maximumScale, args.width, args.height,
   	        args.userScalable, args.didUseConstantsForWidth, args.didUseConstantsForHeight);
   	}
}

void ChromeClient::print(Frame*)
{
    Palm::WebView* view = m_webPage->view();
    if (!view)
        return;

    Palm::WebViewClient* viewClient = view->client();
    if (!viewClient)
        return;

    viewClient->showPrintDialog();
}

void ChromeClient::runOpenPanel(Frame*, PassRefPtr<FileChooser> prpFileChooser)
{
    notImplemented();
	/*
    RefPtr<FileChooser> chooser = prpFileChooser;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Upload File"),
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(platformWindow()))),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                    NULL);

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), chooser->allowsMultipleFiles());

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        if (gtk_file_chooser_get_select_multiple(GTK_FILE_CHOOSER(dialog))) {
            GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
            Vector<String> names;
            for (GSList* item = filenames ; item ; item = item->next) {
                if (!item->data)
                    continue;
                names.append(filenameToString(static_cast<char*>(item->data)));
                g_free(item->data);
            }
            g_slist_free(filenames);
            chooser->chooseFiles(names);
        } else {
            gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename)
                chooser->chooseFile(filenameToString(filename));
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
	*/
}

void ChromeClient::setCursor( const WebCore::Cursor& )
{
	
}

void ChromeClient::setVisible(bool visible)
{
    m_visible = visible;

    if (m_visible && !m_cached) {
        if (m_animDeferred)
            StartAnimationTimer(0);
    }
    else if (m_animationTimer) {    // If timer running
        StopAnimationTimer();
        m_animDeferred = true;
    }
}

void ChromeClient::setCached(bool cached)
{
    m_cached = cached;

    if (m_visible && !m_cached) {
        if (m_animDeferred)
            StartAnimationTimer(0);
    }
    else if (m_animationTimer) {    // If timer running
        StopAnimationTimer();
        m_animDeferred = true;
    }
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void ChromeClient::PrvCbAnimationTimeout(void* pArg)
{
    ChromeClient* pClient = reinterpret_cast<ChromeClient*>(pArg);

    if (!pClient->m_visible || pClient->m_cached || pClient->m_paintPending) {
        // Animation timer fired, but window is not focused or paint was 
        // pending. Don't animate and restart timer once focused/painted.
        pClient->m_animDeferred = true;
    }
    else {
        pClient->animate();
    }
}

void ChromeClient::animate()
{
    // clear pending animation flag before calling animate(), because animate()
    // could result in another animation being scheduled, and it would get
    // ignored if this flag was still set to true...
    webOS::Reporter::Event::logEvent(this, "webkit.animation", "Animation frame: %d", m_animCount);
    m_animCount++;
    m_animationPending = false;
    m_webPage->view()->animate();
}

void ChromeClient::StartAnimationTimer(uint64_t animationDelayMsec)
{
    StopAnimationTimer();

    ASSERT(m_animationTimer == 0);
    m_animationTimer = webkit_timer_new(PrvCbAnimationTimeout, this);
    webkit_timer_fire(m_animationTimer, animationDelayMsec);

    m_animDeferred = false;
}

void ChromeClient::StopAnimationTimer()
{
    if (m_animationTimer) {
        webkit_timer_delete(m_animationTimer);
        m_animationTimer = 0;
    }
}

void ChromeClient::scheduleAnimation()
{
     /* An animation has already been scheduled, we don't need to do anything
     * else */
    if (m_animationPending)
        return;

    const uint64_t k_AnimationFps = 60;
    const uint64_t k_AnimationDelayMsec = 1000 / k_AnimationFps;

    StartAnimationTimer(k_AnimationDelayMsec);
    m_animationPending = true;
}
#endif

IntRect ChromeClient::windowToScreen(const IntRect& rect) const
{
	return rect;
}

IntPoint ChromeClient::screenToWindow(const IntPoint& point) const
{
    return point;                   
}


void ChromeClient::contentsSizeChanged(Frame*, const IntSize& sz) const
{
	//printf( "contentsSizeChanged -> %d x %d\n", sz.width(), sz.height() );
}


void ChromeClient::requestGeolocationPermissionForFrame(Frame* frame, WebCore::Geolocation* g)
{
	g->setIsAllowed(true);
}

void ChromeClient::cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*)
{
}

// The database for this frame has exceeded its quota -- this callback is a 
// chance to increase the quota for this origin ONLY.
void ChromeClient::exceededDatabaseQuota(WebCore::Frame* frame, const WTF::String& databaseIdentifier)
{
	SecurityOrigin* origin = frame->document()->securityOrigin();
	
	if( ::getenv( "drt_dumpdatabasecallbacks" ) )
	{
		// Layout Tests
		printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n", 
			origin->protocol().utf8().data(),
			origin->host().utf8().data(),
			origin->port(),
			databaseIdentifier.utf8().data() );
	}
	DatabaseTracker::tracker().setQuota(origin,PalmBrowserSettings()->htmlDbSize);
}

void ChromeClient::scrollbarsModeDidChange() const
{
}

#if ENABLE(TOUCH_EVENTS)
void ChromeClient::needTouchEvents(bool val)
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

	viewClient->needTouchEvents(val);
}
#endif


void ChromeClient::needSensorEvents(Palm::SensorType type, bool val)
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

    viewClient->needSensorEvents(type, val);
}

void ChromeClient::openSearchUrl(const WTF::String& url)
{
	Palm::WebView* view = m_webPage->view();
	if (!view)
		return;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return;

	viewClient->openSearchUrl(url.utf8().data());
}

void ChromeClient::focusedNodeChanged(WebCore::Node*)
{
}

void ChromeClient::invalidateWindow(const WebCore::IntRect& windowRect, bool immediate )
{
	invalidateContentsAndWindow( windowRect, immediate );
}

void ChromeClient::invalidateContentsAndWindow(const WebCore::IntRect& windowRect, bool /* immediate*/ )
{
	Palm::WebView* view = m_webPage->view();
	if( !view )
		return;
	
	Palm::WebViewClient* viewClient = view->client();
	if( !viewClient )
		return;

    m_paintPending = true;
	viewClient->invalContents( windowRect.x(),windowRect.y(),windowRect.width(),windowRect.height() );
}
	
void ChromeClient::invalidateContentsForSlowScroll(const WebCore::IntRect& windowRect, bool immediate)
{
	invalidateContentsAndWindow(windowRect, immediate);
}

PassRefPtr<WebCore::PopupMenu> ChromeClient::createPopupMenu(WebCore::PopupMenuClient* client) const
{
    return adoptRef(new PopupMenuPalm(client));
}

PassRefPtr<WebCore::SearchPopupMenu> ChromeClient::createSearchPopupMenu(WebCore::PopupMenuClient* client) const
{
    return adoptRef(new SearchPopupMenuPalm(client));
}

void ChromeClient::paintComplete()
{
    m_paintPending = false;

    if (m_animDeferred) {
        m_animDeferred = false;
        StartAnimationTimer(0);
    }
}

#if USE(ACCELERATED_COMPOSITING)
void ChromeClient::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer) {
    Palm::WebView* view = m_webPage->view();
    if( !view )
        return;
    view->privateData()->setRootGraphicsLayer(graphicsLayer ? graphicsLayer->platformLayer() : 0);
}

void ChromeClient::setNeedsOneShotDrawingSynchronization() {
#if 0
    From the Qt port - do we need this?
    // we want the layers to synchronize next time we update the screen anyway
    if (platformPageClient())
        platformPageClient()->markForSync(false);
#endif
}

void ChromeClient::scheduleCompositingLayerSync() {
    Palm::WebView* view = m_webPage->view();
    if( !view )
        return;
    view->privateData()->setRootLayerNeedsDisplay();
}

bool ChromeClient::allowsAcceleratedCompositing() const
{
    Palm::WebView* view = m_webPage->view();
    if (view)
        return view->supportsAcceleratedCompositing() && PalmBrowserSettings()->enableAcceleratedCompositing;

   return PalmBrowserSettings()->enableAcceleratedCompositing;
}
#endif

#if USE(GLES2_RENDERING)
PassOwnPtr<GLES2Context> ChromeClient::getOnscreenGLES2Context() {
    Palm::WebView* view = m_webPage->view();
    if( !view )
        return 0;
    return view->privateData()->getOnscreenGLES2Context();
}

PassOwnPtr<GLES2Context> ChromeClient::getOffscreenGLES2Context() {
    Palm::WebView* view = m_webPage->view();
    if( !view )
        return 0;
    return view->privateData()->getOffscreenGLES2Context();
}
#endif

} // namespace WebKit

