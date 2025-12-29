#include "palmwebviewclient.h"
#include "palmwebpageclient.h"

namespace Palm
{

void WebPageClient::jsConsoleMessage(MessageLevel level, const char* inMsg, int lineNo, const char* inMsgSource)
{
}

bool WebPageClient::requestGeolocationPermissionForDomain(const char* domain)
{
    return true;
}



void WebViewClient::editorFocused(bool focused, unsigned int editorState)
{
}

void WebViewClient::editorFocused(bool focused, int fieldType, int fieldActions, int fieldFlags)
{
}

void WebViewClient::editorFocused(bool focused, const PalmIME::EditorState & editorState)
{
}

void WebViewClient::autoCapEnabled(bool enabled)
{
}

bool WebViewClient::smartKeySearch(int requestId, const char* query)
{
    return false;
}

void WebViewClient::selectionChanged()
{
}

bool WebViewClient::smartKeyLearn(const char* word)
{
    return false;
}

void WebViewClient::addInteractiveWidgetRect(uintptr_t id, int x, int y, int width, int height, InteractiveRectType type)
{
}

void WebViewClient::removeInteractiveWidgetRect(uintptr_t id, InteractiveRectType type)
{
}

void WebViewClient::needTouchEvents(bool)
{
}

void WebViewClient::needSensorEvents(Palm::SensorType type, bool needEvents)
{
}

void WebViewClient::openSearchUrl(const char* url)
{
}

void WebViewClient::spellingWidgetVisibleRectUpdate(int x, int y, int width, int height)
{
}

void WebViewClient::suspendAppRendering()
{
}

void WebViewClient::resumeAppRendering()
{
}

}
