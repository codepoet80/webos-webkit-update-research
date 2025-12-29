#include "webkitstats.h"
#include <SimpleStats/SimpleStats.h>


namespace Palm
{

static SimpleStats::EventData s_PaintEventData("paint");
static SimpleStats::EventData s_KeyboardEventData("keyboard");
static SimpleStats::EventData s_MouseDownEventData("mouseDown");
static SimpleStats::EventData s_MouseMoveEventData("mouseMove");
static SimpleStats::EventData s_MouseUpEventData("mouseUp");
static SimpleStats::EventData s_GestureEventData("gesture");
static SimpleStats::EventData s_ScriptEvalEventData("scriptEval");
static SimpleStats::EventData s_AcDoCompositeEventData("ac-doComposite");
static SimpleStats::EventData s_AcUpdateContentsEventData("ac-updateContents");
static SimpleStats::EventData s_AcTextureUpdateEventData("ac-textureUpdate");
static SimpleStats::EventData s_AcPixmapCreateEventData("ac-createPixmap");
static SimpleStats::EventData s_AcDrawLayersEventData("ac-drawLayers");
static SimpleStats::EventData s_ImageDecodeEventData("imageDecode");
static SimpleStats::EventData s_JsGcEventData("js-gc");
static SimpleStats::EventData s_JsInvokeCallbackEventData("js-invokeCallback");
static SimpleStats::EventData s_JsInvokeListenerEventData("js-invokeListener");
static SimpleStats::EventData s_JsInvokeTimerEventData("js-invokeTimer");
static SimpleStats::EventData s_JsCompileEventData("js-compile");
static SimpleStats::EventData s_JsRunEventData("js-run");
static SimpleStats::EventData s_StyleRecalcEventData("styleRecalc");
static SimpleStats::EventData s_StyleForElementEventData("styleForElement");
static SimpleStats::EventData s_DomEventDispatchEventData("domEventDispatch");
static SimpleStats::EventData s_HtmlParseEventData("parseHtml");
static SimpleStats::EventData s_ServiceResponseEventData("serviceResponse");

SimpleStats::EventData& WebKitStats::getPaintEventData()
{
    return s_PaintEventData;
}

SimpleStats::EventData& WebKitStats::getKeyboardEventData()
{
    return s_KeyboardEventData;
}

SimpleStats::EventData& WebKitStats::getMouseDownEventData()
{
    return s_MouseDownEventData;
}

SimpleStats::EventData& WebKitStats::getMouseMoveEventData()
{
    return s_MouseMoveEventData;
}

SimpleStats::EventData& WebKitStats::getMouseUpEventData()
{
    return s_MouseUpEventData;
}

SimpleStats::EventData& WebKitStats::getGestureEventData()
{
    return s_GestureEventData;
}

SimpleStats::EventData& WebKitStats::getScriptEvalEventData()
{
    return s_ScriptEvalEventData;
}

SimpleStats::EventData& WebKitStats::getAcDoCompositeEventData()
{
    return s_AcDoCompositeEventData;
}

SimpleStats::EventData& WebKitStats::getAcUpdateContentsEventData()
{
    return s_AcUpdateContentsEventData;
}

SimpleStats::EventData& WebKitStats::getAcTextureUpdateEventData()
{
    return s_AcTextureUpdateEventData;
}

SimpleStats::EventData& WebKitStats::getAcPixmapCreateEventData()
{
    return s_AcPixmapCreateEventData;
}

SimpleStats::EventData& WebKitStats::getAcDrawLayersEventData()
{
    return s_AcDrawLayersEventData;
}

SimpleStats::EventData& WebKitStats::getImageDecodeEventData()
{
    return s_ImageDecodeEventData;
}

SimpleStats::EventData& WebKitStats::getJsGcEventData()
{
    return s_JsGcEventData;
}

SimpleStats::EventData& WebKitStats::getJsInvokeCallbackEventData()
{
    return s_JsInvokeCallbackEventData;
}

SimpleStats::EventData& WebKitStats::getJsInvokeListenerEventData()
{
    return s_JsInvokeListenerEventData;
}

SimpleStats::EventData& WebKitStats::getJsInvokeTimerEventData()
{
    return s_JsInvokeTimerEventData;
}

SimpleStats::EventData& WebKitStats::getJsCompileEventData()
{
    return s_JsCompileEventData;
}

SimpleStats::EventData& WebKitStats::getJsRunEventData()
{
    return s_JsRunEventData;
}

SimpleStats::EventData& WebKitStats::getStyleRecalcEventData()
{
    return s_StyleRecalcEventData;
}

SimpleStats::EventData& WebKitStats::getStyleForElementEventData()
{
    return s_StyleForElementEventData;
}

SimpleStats::EventData& WebKitStats::getDomEventDispatchEventData()
{
    return s_DomEventDispatchEventData;
}

SimpleStats::EventData& WebKitStats::getHtmlParseEventData()
{
    return s_HtmlParseEventData;
}

SimpleStats::EventData& WebKitStats::getServiceResponseEventData()
{
    return s_ServiceResponseEventData;
}

bool WebKitStats::getJSON(std::string& json)
{
    json = "[";
    json += s_PaintEventData.toJSON();
    json += "," + s_KeyboardEventData.toJSON();
    json += "," + s_MouseDownEventData.toJSON();
    json += "," + s_MouseMoveEventData.toJSON();
    json += "," + s_MouseUpEventData.toJSON();
    json += "," + s_GestureEventData.toJSON();
    json += "," + s_ScriptEvalEventData.toJSON();
    json += "," + s_AcDoCompositeEventData.toJSON();
    json += "," + s_AcUpdateContentsEventData.toJSON();
    json += "," + s_AcTextureUpdateEventData.toJSON();
    json += "," + s_AcPixmapCreateEventData.toJSON();
    json += "," + s_AcDrawLayersEventData.toJSON();
    json += "," + s_ImageDecodeEventData.toJSON();
    json += "," + s_JsGcEventData.toJSON();
    json += "," + s_JsInvokeCallbackEventData.toJSON();
    json += "," + s_JsInvokeListenerEventData.toJSON();
    json += "," + s_JsInvokeTimerEventData.toJSON();
    json += "," + s_JsCompileEventData.toJSON();
    json += "," + s_JsRunEventData.toJSON();
    json += "," + s_StyleRecalcEventData.toJSON();
    json += "," + s_StyleForElementEventData.toJSON();
    json += "," + s_DomEventDispatchEventData.toJSON();
    json += "," + s_HtmlParseEventData.toJSON();
    json += "," + s_ServiceResponseEventData.toJSON();
    json += "]";
    return true;
}

}
