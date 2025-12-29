
// Copyright 2010 Palm Inc.

#ifndef ChromeClientPalm_h
#define ChromeClientPalm_h

#include <wtf/text/WTFString.h>
#include "ChromeClient.h"
#include "Chrome.h"
#include "Geolocation.h"
#include "IntRect.h"
#include "Cursor.h"
#if USE(GLES2_RENDERING)
#include "GLES2Context.h"
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "webkitpalmtimer.h"
#endif

namespace WebCore {
	class FileChooser;
	class Node;
	class Frame;
	class GraphicsLayer;
}

namespace Palm {
	class WebPage;
}

namespace WebKit {

    class ChromeClient : public WebCore::ChromeClient {
    public:
		ChromeClient(Palm::WebPage*);
        virtual ~ChromeClient();
        Palm::WebPage* webPage() const { return m_webPage; }

        virtual void chromeDestroyed();

        virtual void setWindowRect(const WebCore::FloatRect&);
        virtual WebCore::FloatRect windowRect();

        virtual WebCore::FloatRect pageRect();

        virtual float scaleFactor();

        virtual void focusedNodeChanged(WebCore::Node*);
        
        // Methods used by HostWindow.
        virtual void invalidateWindow(const WebCore::IntRect&, bool);
        virtual void invalidateContentsAndWindow(const WebCore::IntRect&, bool);
        virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool);
        virtual void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect);
        virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
        virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
        virtual PlatformPageClient platformPageClient() const;
        virtual void scrollbarsModeDidChange() const;
        virtual void setCursor( const WebCore::Cursor& );
        // End methods by HostWindow

#if ENABLE(REQUEST_ANIMATION_FRAME)
        virtual void scheduleAnimation();
#endif
        
        virtual void paintComplete();

        virtual void focus();
        virtual void unfocus();

        virtual bool canTakeFocus(WebCore::FocusDirection);
        virtual void takeFocus(WebCore::FocusDirection);

        virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&);
        virtual void show();

        virtual bool canRunModal();
        virtual void runModal();

        virtual void setToolbarsVisible(bool);
        virtual bool toolbarsVisible();

        virtual void setStatusbarVisible(bool);
        virtual bool statusbarVisible();

        virtual void setScrollbarsVisible(bool);
        virtual bool scrollbarsVisible();

        virtual void setMenubarVisible(bool);
        virtual bool menubarVisible();

        virtual void setResizable(bool);

        virtual void addMessageToConsole(WebCore::MessageSource source, WebCore::MessageType type,
                                         WebCore::MessageLevel level, const String& message,
                                         unsigned int lineNumber, const String& sourceID);

        virtual bool canRunBeforeUnloadConfirmPanel();
        virtual bool runBeforeUnloadConfirmPanel(const String& message, WebCore::Frame* frame);

        virtual void closeWindowSoon();

        virtual void runJavaScriptAlert(WebCore::Frame*, const String&);
        virtual bool runJavaScriptConfirm(WebCore::Frame*, const String&);
        virtual bool runJavaScriptPrompt(WebCore::Frame*, const String& message, const String& defaultValue, String& result);
        virtual void setStatusbarText(const String&);
        virtual bool shouldInterruptJavaScript();
        virtual bool tabsToLinks() const;

        virtual WebCore::IntRect windowResizerRect() const;
        virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const;

        virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags);

        virtual void setToolTip(const String&, WebCore::TextDirection);

        virtual void didReceiveViewportArguments(WebCore::Frame*, const WebCore::ViewportArguments&) const;
        
        virtual void print(WebCore::Frame*);
#if ENABLE(DATABASE)		
		virtual void exceededDatabaseQuota(WebCore::Frame*, const String&);
#endif		

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        // Callback invoked when the application cache fails to save a cache object
        // because storing it would grow the database file past its defined maximum
        // size or past the amount of free space on the device. 
        // The chrome client would need to take some action such as evicting some
        // old caches.
        virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);

        // Callback invoked when the application cache origin quota is reached. This
        // means that the resources attempting to be cached via the manifest are
        // more than allowed on this origin. This callback allows the chrome client
        // to take action, such as prompting the user to ask to increase the quota
        // for this origin.
        virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin* origin);
#endif

#if ENABLE(TOUCH_EVENTS)
        virtual void needTouchEvents(bool val);
#endif
        virtual void needSensorEvents(Palm::SensorType type, bool val);

		virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>);
        // Asynchronous request to load an icon for specified filenames.
        virtual void chooseIconForFiles(const Vector<String>&, WebCore::FileChooser*) { }

        virtual bool selectItemWritingDirectionIsNatural(){ return true; }
		
		virtual void formStateDidChange(const WebCore::Node*) { }

        virtual PassOwnPtr<WebCore::HTMLParserQuirks> createHTMLParserQuirks() { return 0; }

#if USE(ACCELERATED_COMPOSITING)
        // Pass 0 as the GraphicsLayer to detatch the root layer.
        virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*);
        // Sets a flag to specify that the next time content is drawn to the window,
        // the changes appear on the screen in synchrony with updates to GraphicsLayers.
        virtual void setNeedsOneShotDrawingSynchronization();
        // Sets a flag to specify that the view needs to be updated, so we need
        // to do an eager layout before the drawing.
        virtual void scheduleCompositingLayerSync();
        virtual bool allowsAcceleratedCompositing() const;
#endif

#if USE(GLES2_RENDERING)
        // Request a GL ES 2 context to use for compositing this page's content.
        virtual PassOwnPtr<WebCore::GLES2Context> getOnscreenGLES2Context();
        virtual PassOwnPtr<WebCore::GLES2Context> getOffscreenGLES2Context();
#endif

        virtual void scrollRectIntoView(const WebCore::IntRect&, const WebCore::ScrollView*) const {}
        virtual void requestGeolocationPermissionForFrame(WebCore::Frame*, WebCore::Geolocation*);
        virtual void cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*);
        virtual void openSearchUrl(const String& url);

        virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const;
        virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const;

        virtual void setVisible(bool visible);
        virtual void setCached(bool cached);
		
    private:
#if ENABLE(REQUEST_ANIMATION_FRAME)
    static void PrvCbAnimationTimeout(void* pArg);
        virtual void StartAnimationTimer(uint64_t animationDelayMsec);
        virtual void StopAnimationTimer();
        virtual void animate();
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
        WebKitPalmTimer*    m_animationTimer;
        bool                m_animationPending;
        bool                m_animDeferred;
        int                 m_animCount;    ///< The number of times the animation has fired.
#endif
        Palm::WebPage* m_webPage;
        bool           m_paintPending;
        bool           m_visible;   ///< Is the page owning this client visibe?
        bool           m_cached;    ///< Cached pages are alive, but idle (unused).
    };
}

#endif // ChromeClient_h
