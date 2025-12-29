
// Copyright 2010 Palm Inc.

#ifndef InspectorClientPalm_h
#define InspectorClientPalm_h

#include <wtf/text/WTFString.h>
#include "InspectorClient.h"

namespace WebCore {
    class Node;
    class Page;
}

namespace Palm {
	class WebPage;
}

namespace WebKit {

	class InspectorClient : public WebCore::InspectorClient {
	public:
		InspectorClient( Palm::WebPage* );
        virtual void inspectorDestroyed();

        virtual void openInspectorFrontend(WebCore::InspectorController*);
        virtual bool sendMessageToFrontend(const String& message);
        
        virtual WebCore::Page* createPage();

        virtual WTF::String localizedStringsURL();
		
		virtual WTF::String hiddenPanels();

		virtual void inspectorWindowObjectCleared();
			
        virtual void showWindow();
        virtual void closeWindow();

        virtual void attachWindow();
        virtual void detachWindow();
		
		virtual void setAttachedWindowHeight(unsigned height);

        virtual void highlight(WebCore::Node*);
        virtual void hideHighlight();
        virtual void inspectedURLChanged(const WTF::String& newURL);
		
        virtual void populateSetting(const WTF::String& key, WTF::String* value);
        virtual void storeSetting(const WTF::String& key, const String& value);
        virtual void removeSetting(const WTF::String& key);		
    };
}

#endif
