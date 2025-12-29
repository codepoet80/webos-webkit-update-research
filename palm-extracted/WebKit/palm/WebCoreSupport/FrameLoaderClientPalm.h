
// Copyright 2008,2009,2010 Palm Inc.

#ifndef FrameLoaderClientPalm_h
#define FrameLoaderClientPalm_h

#include <wtf/text/WTFString.h>

#include "FrameLoaderClient.h"
#include "ResourceResponse.h"
#include "ResourceHandleClient.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "Timer.h"

const int kMaxUrlTitleLength = 2048;

namespace Palm {
class WebFrame;
class SSLValidationInfo;
}

namespace WebCore {
    class PluginView;
	class Event;
}

namespace WebKit {

    class FrameLoaderClient : public WebCore::FrameLoaderClient  {
    public:
        FrameLoaderClient(Palm::WebFrame*);
        virtual ~FrameLoaderClient();
        virtual void frameLoaderDestroyed();

		Palm::WebFrame*  frame() const { return m_frame; }

        virtual bool hasWebView() const;

        virtual void makeRepresentation(WebCore::DocumentLoader*);
        virtual void forceLayout();
        virtual void forceLayoutForNonHTML();

        virtual void setCopiesOnScroll();

        virtual void detachedFromParent1();
        virtual void detachedFromParent2();
        virtual void detachedFromParent3();

        virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);

        virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long  identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
        virtual bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long identifier);
        virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
        virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::AuthenticationChallenge&);
        virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::ResourceResponse&);
        virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived);
        virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long  identifier);
        virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long  identifier, const WebCore::ResourceError&);
        virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);
		virtual void dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const WebCore::ScriptString&);

        virtual void dispatchDidReceiveSSLCertValidationFail(WebCore::DocumentLoader *,Palm::SSLValidationInfo& sslInfo);
        
        virtual void dispatchDidHandleOnloadEvents();
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
        virtual void dispatchDidCancelClientRedirect();
        virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double, double);
        virtual void dispatchDidReplaceStateWithinPage();
        virtual void dispatchDidPopStateWithinPage();
        virtual void dispatchDidChangeIcons();
        virtual void dispatchWillSendSubmitEvent(WebCore::HTMLFormElement*);
        virtual void dispatchDidChangeLocationWithinPage();
        virtual void dispatchWillClose();
        virtual void dispatchDidPushStateWithinPage();
        virtual void dispatchDidReceiveIcon();
        virtual void dispatchDidStartProvisionalLoad();
        virtual void dispatchDidReceiveTitle(const WTF::String&);
        virtual void dispatchDidCommitLoad();
        virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
        virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
        virtual void dispatchDidFinishDocumentLoad();
        virtual void dispatchDidFinishLoad();
        virtual void dispatchDidFirstLayout();
		virtual void dispatchDidFirstVisuallyNonEmptyLayout();
		
		virtual void dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const;
		virtual void dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const;
		virtual void dispatchDidChangeBackForwardIndex() const;
		virtual void didTransferChildFrameToNewDocument();
		virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*);

        virtual WebCore::Frame* dispatchCreatePage();
        virtual void dispatchShow();

        virtual void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction, const WTF::String& MIMEType, const WebCore::ResourceRequest&);
        virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WTF::PassRefPtr<WebCore::FormState>, const WTF::String& frameName);
        virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, WTF::PassRefPtr<WebCore::FormState>);
        virtual void cancelPolicyCheck();

        virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);

        virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, WTF::PassRefPtr<WebCore::FormState>);

        virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
        virtual void revertToProvisionalState(WebCore::DocumentLoader*);
        virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);
        virtual void clearUnarchivingState(WebCore::DocumentLoader*);

        virtual void postProgressStartedNotification();
        virtual void postProgressEstimateChangedNotification();
        virtual void postProgressFinishedNotification();

		virtual void didDisplayInsecureContent();
		virtual void didRunInsecureContent(WebCore::SecurityOrigin*);
		virtual void didCreateScriptContextForFrame();
		virtual void didDestroyScriptContextForFrame();
		virtual void didCreateIsolatedScriptContext();
		
        virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL& url, const WTF::String& name, WebCore::HTMLFrameOwnerElement* ownerElement,
                                                       const WTF::String& referrer, bool allowsScrolling, int marginWidth, int marginHeight);
        virtual PassRefPtr<WebCore::Widget> createPlugin(const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::KURL&, const WTF::Vector<WTF::String>&, const WTF::Vector<WTF::String>&, const WTF::String&, bool);
        virtual void redirectDataToPlugin(WebCore::Widget* pluginWidget);
        virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(const WebCore::IntSize&, WebCore::HTMLAppletElement*, const WebCore::KURL& baseURL, const WTF::Vector<WTF::String>& paramNames, const WTF::Vector<WTF::String>& paramValues);
        virtual WTF::String overrideMediaType() const;
		virtual void documentElementAvailable();
        virtual void didPerformFirstNavigation() const;

        virtual void registerForIconNotification(bool); 
        
        virtual WebCore::ObjectContentType objectContentType(const WebCore::KURL& url, const WTF::String& mimeType);

        virtual void setMainFrameDocumentReady(bool);

        virtual void startDownload(const WebCore::ResourceRequest&);

        virtual void willChangeTitle(WebCore::DocumentLoader*);
        virtual void didChangeTitle(WebCore::DocumentLoader*);

        virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
        virtual void finishedLoading(WebCore::DocumentLoader*);
        virtual void finalSetupForReplace(WebCore::DocumentLoader*);

        virtual void updateGlobalHistory();
		virtual void updateGlobalHistoryRedirectLinks();
        virtual bool shouldGoToHistoryItem(WebCore::HistoryItem*) const;

        virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
        virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);

        virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
        virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
		virtual WebCore::ResourceError pluginWillHandleLoadError(const WebCore::ResourceResponse&);

        virtual bool shouldFallBack(const WebCore::ResourceError&);

        virtual void setDefersLoading(bool);

        virtual bool willUseArchive(WebCore::ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL& originalURL) const;
        virtual bool isArchiveLoadPending(WebCore::ResourceLoader*) const;
        virtual void cancelPendingArchiveLoad(WebCore::ResourceLoader*);
        virtual void clearArchivedResources();

        virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
        virtual bool canShowMIMEType(const WTF::String&) const;
        virtual bool representationExistsForURLScheme(const WTF::String&) const;
        virtual WTF::String generatedMIMETypeForURLScheme(const WTF::String&) const;

        virtual void frameLoadCompleted();
        virtual void saveViewStateToItem(WebCore::HistoryItem*);
        virtual void restoreViewState();
        virtual void provisionalLoadStarted();
        virtual void didFinishLoad();
        virtual void prepareForDataSourceReplacement();

        virtual WTF::PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
        virtual void setTitle(const WTF::String& title, const WebCore::KURL&);

        virtual WTF::String userAgent(const WebCore::KURL&);

        virtual void savePlatformDataToCachedFrame(WebCore::CachedFrame*);
        virtual void transitionToCommittedFromCachedFrame(WebCore::CachedFrame*);
        virtual void transitionToCommittedForNewPage();
		
        virtual bool canCachePage() const;
        virtual void download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
		long GetProgress() const { return m_ProgressIndicator; }
		
		// downloads
		virtual void downloadStart( const char* url );
		virtual void downloadProgress( const char* url, unsigned long bytesSoFar, unsigned long bytesTotal );
		virtual void downloadError( const char* url, const char* errMsg );
		virtual void downloadFinished( const char* mimeType, const char* url, const char* tmpFilePath );
		virtual void downloadCancel( const char* url ); 
		
		// UserAgent customization
		void initializeUserAgentTokens();

		static WTF::PassRefPtr<WebCore::Event> getOpenUrlEvent();
		
    private:
		bool isMainFrame() const;
		static bool mimeIsInvalid(const WTF::String& mimeString);

		Palm::WebFrame* m_frame;
        WebCore::ResourceResponse m_response;
        bool m_firstData;
		long m_ProgressIndicator;

        // Points to the plugin view that data should be redirected to.
        WebCore::PluginView* m_pluginView;
        bool m_hasSentResponseToPlugin;
		
		// Keep PmIpc less chatty.
		CString		m_lastTitleSent;
		CString		m_lastUrlSent;
		long		m_lastPercentSent;
		
		bool m_userAgentInitialized;
		Vector<WTF::String> m_UATokens;
    };

}

#endif
