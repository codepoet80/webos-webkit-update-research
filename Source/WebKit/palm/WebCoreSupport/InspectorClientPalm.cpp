

#include "config.h"
#include "InspectorClientPalm.h"

#include "NotImplemented.h"
#include "PlatformString.h"

using namespace WebCore;

namespace WebKit {
	
InspectorClient::InspectorClient( Palm::WebPage* pg )
{
}

void InspectorClient::inspectorDestroyed()
{
	delete this;
}

void InspectorClient::openInspectorFrontend(InspectorController* controller)
{
	/*
    // This g_object_get will ref the inspector. We're not doing an
    // unref if this method succeeds because the inspector object must
    // be alive even after the inspected WebView is destroyed - the
    // close-window and destroy signals still need to be
    // emitted.
    WebKitWebInspector* webInspector = 0;
    g_object_get(m_inspectedWebView, "web-inspector", &webInspector, NULL);
    ASSERT(webInspector);

    WebKitWebView* inspectorWebView = 0;
    g_signal_emit_by_name(webInspector, "inspect-web-view", m_inspectedWebView, &inspectorWebView);

    if (!inspectorWebView) {
        g_object_unref(webInspector);
        return;
    }

    webkit_web_inspector_set_web_view(webInspector, inspectorWebView);

    GOwnPtr<gchar> inspectorPath(g_build_filename(inspectorFilesPath(), "inspector.html", NULL));
    GOwnPtr<gchar> inspectorURI(g_filename_to_uri(inspectorPath.get(), 0, 0));
    webkit_web_view_load_uri(inspectorWebView, inspectorURI.get());

    gtk_widget_show(GTK_WIDGET(inspectorWebView));

    m_frontendPage = core(inspectorWebView);
    m_frontendClient = new InspectorFrontendClient(m_inspectedWebView, inspectorWebView, webInspector, m_frontendPage, this);
    m_frontendPage->inspectorController()->setInspectorFrontendClient(m_frontendClient);
    */
}

bool InspectorClient::sendMessageToFrontend(const String& message)
{
	/*
    if (!m_frontendPage)
        return false;

    Frame* frame = m_frontendPage->mainFrame();
    if (!frame)
        return false;

    ScriptController* scriptController = frame->script();
    if (!scriptController)
        return false;

    String dispatchToFrontend("WebInspector.dispatchMessageFromBackend(");
    dispatchToFrontend += message;
    dispatchToFrontend += ");";
    scriptController->executeScript(dispatchToFrontend);
    return true;
    */
    return false;
}

Page* InspectorClient::createPage()
{
    notImplemented();
    return 0;
}

String InspectorClient::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorClient::showWindow()
{
    notImplemented();
}

void InspectorClient::closeWindow()
{
    notImplemented();
}

void InspectorClient::attachWindow()
{
    notImplemented();
}

void InspectorClient::detachWindow()
{
    notImplemented();
}

void InspectorClient::highlight(Node* node)
{
    notImplemented();
}

void InspectorClient::hideHighlight()
{
    notImplemented();
}

void InspectorClient::inspectedURLChanged(const String&)
{
    notImplemented();
}

String InspectorClient::localizedStringsURL()
{
    notImplemented();
    return String();
}

void  InspectorClient::inspectorWindowObjectCleared()
{
}

void InspectorClient::populateSetting(const String& key, String* value )
{
    notImplemented();
}

void InspectorClient::storeSetting(const String& key, const String& value )
{
    notImplemented();
}

void InspectorClient::removeSetting(const String& key)
{
    notImplemented();
}

void InspectorClient::setAttachedWindowHeight( unsigned height )
{
}

}

