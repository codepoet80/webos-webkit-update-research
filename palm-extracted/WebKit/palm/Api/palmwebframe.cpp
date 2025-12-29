/* ============================================================
 * Date  : 2008-09-22
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
#include "palmwebframe.h"
#include "palmwebpage.h"
#include "palmwebpageclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpageprivate.h"
#include "webkitpalmsettings.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "FrameLoaderClientPalm.h"
#include "FrameLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include "Element.h"
#include "RenderTreeAsText.h"
#include "markup.h"
#include "V8Proxy.h"
#include "Page.h"
#include "Settings.h"
#include "AXObjectCache.h"
#include "AccessibilityObjectWrapperWebOS.h"
#include "RenderView.h"
#include "PrintContext.h"
#include <sys/stat.h>
#include "IntRect.h"
#include <pbnjson.hpp>
#include "palmwebglobal.h"
#include <webkitstats.h>
#include <SimpleStats/SimpleStats.h>

using namespace WebCore;

WebCore::Frame* core(Palm::WebFrame* f)
{
	return Palm::WebFramePrivate::core(f);
}

Palm::WebFrame* kit(WebCore::Frame* f)
{
	return Palm::WebFramePrivate::kit(f);
}

void WEBKIT_API palm_drt_suspendAnimations(Palm::WebFrame* palmFrame)
{
	WebCore::Frame* frame = core(palmFrame);
    AnimationController* controller = frame->animation();
    if (!controller)
        return;

    controller->suspendAnimations(frame->document());
}

void WEBKIT_API palm_drt_resumeAnimations(Palm::WebFrame* palmFrame)
{
	WebCore::Frame* frame = core(palmFrame);
    AnimationController* controller = frame->animation();
    if (!controller)
        return;

    controller->resumeAnimations(frame->document());
}

// Pause a given CSS animation or transition on the target node at a specific time.
// If the animation or transition is already paused, it will update its pause time.
// This method is only intended to be used for testing the CSS animation and transition system.
bool WEBKIT_API palm_drt_pauseAnimation(Palm::WebFrame* pFrame, const std::string &animationName, double time, const std::string &elementId)
{
    WebCore::Frame* frame = core(pFrame);
    if (!frame)
        return false;

    AnimationController* controller = frame->animation();
    if (!controller)
        return false;

    Document* doc = frame->document();
    ASSERT(doc);

    Node* coreNode = doc->getElementById(String(elementId.c_str()));
    if (!coreNode || !coreNode->renderer())
        return false;

    return controller->pauseAnimationAtTime(coreNode->renderer(), String(animationName.c_str()), time);
}

bool WEBKIT_API palm_drt_pauseTransitionOfProperty(Palm::WebFrame *pframe, const std::string &propertyName, double time, const std::string &elementId)
{
    WebCore::Frame* frame = core(pframe);
    if (!frame)
        return false;

    AnimationController* controller = frame->animation();
    if (!controller)
        return false;

    Document* doc = frame->document();
    ASSERT(doc);

    Node* coreNode = doc->getElementById(String(elementId.c_str()));
    if (!coreNode || !coreNode->renderer())
        return false;

    return controller->pauseTransitionAtTime(coreNode->renderer(), String(propertyName.c_str()), time);
}

int WEBKIT_API palm_drt_numberOfActiveAnimations(Palm::WebFrame* pFrame)
{
    Frame* frame = core(pFrame);
    if (!frame)
        return false;

    AnimationController* controller = frame->animation();
    if (!controller)
        return false;

    return controller->numberOfActiveAnimations();
}

namespace Palm
{

static NPNetscapeFuncs s_browserFuncs;
static bool s_browserFuncsInitialized = false;

WebCore::Frame* WebFramePrivate::core(WebFrame* f)
{
	return f->d->frame;
}

WebFrame* WebFramePrivate::kit(WebCore::Frame* f)
{
	return static_cast<WebKit::FrameLoaderClient*>(f->loader()->client())->frame();
}

WebFrame::WebFrame(WebPage* page, bool createRenderer)
    : d(new WebFramePrivate)
    , m_lpsClient(NULL)
    , m_isPrinting(false)
{
    d->parentFrame = this;
    d->palmPage = page;
    d->client = page->client();

	d->frameLoaderClient = new WebKit::FrameLoaderClient(this);
	RefPtr<WebCore::Frame> newFrame = WebCore::Frame::create( core(d->palmPage), 0, d->frameLoaderClient );
	d->frame = newFrame.get();
	d->frame->setShouldCreateRenderers(createRenderer);	
	
	d->frame->init();
	if( d->frame->view() )
		d->frame->view()->setZoomFactor(PalmBrowserSettings()->defaultZoomFactor, ZoomPage);
	d->frame->page()->settings()->setPageScaleFactor(PalmBrowserSettings()->defaultZoomFactor ); 
}

WebFrame::WebFrame(WebFrame* parentFrame, WebFrameData* frameData)
    : d(new WebFramePrivate),
    m_lpsClient(NULL)
{
    d->parentFrame = parentFrame;
	d->palmPage = parentFrame->page();
    d->client = 0;

	d->frameLoaderClient = new WebKit::FrameLoaderClient(this);
	RefPtr<WebCore::Frame> newFrame = WebCore::Frame::create( core(d->palmPage), frameData->ownerElement, d->frameLoaderClient );
	d->frame = newFrame.get();
	
	if( frameData->ownerElement )
		( newFrame.get() )->ref();
	
	d->frame->init();
	if( d->frame->view() )
		d->frame->view()->setZoomFactor(PalmBrowserSettings()->defaultZoomFactor, ZoomPage);
	d->frame->page()->settings()->setPageScaleFactor(PalmBrowserSettings()->defaultZoomFactor );

    if (d->palmPage->client())
        d->palmPage->client()->frameCreated(this);
}

WebFrame::~WebFrame()
{
    if (d->parentFrame != this) {
        // Send this out only for child frames
        if (d->palmPage->client())
            d->palmPage->client()->frameDestroyed(this);
    }
    
	delete d;
	d = 0;

    unregisterLpsClient();
}

WebPage* WebFrame::page() const
{
	return d->palmPage;    
}

void WebFrame::setClient(WebFrameClient* client)
{
    d->client = client;
}

WebFrameClient* WebFrame::client() const
{
    return d->client;
}

bool WebFrame::evaluateScript(const char* script)
{
    ScriptController* s = webcoreFrame() ? webcoreFrame()->script() : 0;
#if USE(V8)
    if (s) {
        Palm::SimpleStats::Event evt(Palm::WebKitStats::getScriptEvalEventData());

        webOS::Reporter::Event::logStart( webcoreFrame()->document(), "webkit.WebPage.evaluate", script );
        ScriptValue r = s->evaluate( WebCore::ScriptSourceCode( String::fromUTF8(script) ) );
        webOS::Reporter::Event::logStop( webcoreFrame()->document(), "webkit.WebPage.evaluate", "" );

        page()->logOnNextPaint();

        if (r.hasNoValue())
            return false;

        if (r.isUndefined())
            return false;

        return r.v8Value()->BooleanValue();
    }
#else
    if (s) {
        ScriptValue r = s->evaluate( WebCore::ScriptSourceCode( String::fromUTF8(script) ) );
        JSC::JSValue v = r.jsValue();
        if (v.isBoolean() && v.getBoolean())
            return true;
    }
#endif
    return false;
}

std::string WebFrame::toHtml( )
{
    if (!d->frame->document())
        return std::string();
	return std::string(  createMarkup(d->frame->document()).utf8().data() );
}

WebFrame* WebFrame::parentFrame()
{
	return d->parentFrame;
}

std::string WebFrame::toPlainText( )
{
    if (d->frame->view() && d->frame->view()->layoutPending())
        d->frame->view()->layout();

    WebCore::Element *documentElement = d->frame->document()->documentElement();

	String innerText = documentElement->innerText();
    glong len(0);
	gchar* utf8Str = g_utf16_to_utf8(innerText.characters(), innerText.length(), NULL, &len, NULL);
	if (utf8Str) {
		std::string utf8(utf8Str, len);
		g_free(utf8Str);

		return utf8;
	}
	else {
		return std::string();
	}
}

std::string WebFrame::frameName()
{
    return std::string( d->frame->tree()->name().string().utf8().data() );
}

std::string WebFrame::renderTreeDump() 
{
    if (d->frame->view() && d->frame->view()->layoutPending())
        d->frame->view()->layout();

    return std::string( WebCore::externalRepresentation( d->frame ).utf8().data() );

}

std::string WebFrame::layerTreeAsText() const
{
    if (d->frame->view() && d->frame->view()->layoutPending())
        d->frame->view()->layout();

    return std::string( d->frame->layerTreeAsText().utf8().data()  );
}

v8::Handle<v8::Value> GetPointX(v8::Local<v8::String> property, const v8::AccessorInfo &info) 
{
	// grab C++ pointer
	v8::Local<v8::Object> self = info.Holder();
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	
	//int value = static_cast<Point*>(ptr)->x_;
	printf("GetPointX\n");
	return v8::Integer::New(69);
}
  
void SetPointX(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info) 
{
	printf("SetPointX\n");
	
	// grab C++ ptr
	v8::Local<v8::Object> self = info.Holder();
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	
	
	//static_cast<Point*>(ptr)->x_ = value->Int32Value();
}


static v8::Handle<v8::Value> LogCallback(const v8::Arguments& args) {
	
	return v8::Integer::New( 69 );
}

bool WebFrame::pushObjectIntoFrame( const char* name, v8::Handle<v8::Object> obj )
{
	// Debug -->
	if( !name )
	{
		v8::HandleScope hs;
		v8::Handle<v8::Context> context = WebCore::V8Proxy::context(d->frame);
		v8::Context::Scope scope(context);
		
		v8::Handle<v8::Object> global = context->Global();
		
		// make the template (only has to be done once)
		v8::Handle<v8::ObjectTemplate> point_templ = v8::ObjectTemplate::New();
		point_templ->SetInternalFieldCount(1);
		
		// ... a property
		point_templ->SetAccessor(v8::String::New("lala"), GetPointX, SetPointX);
		
		// ... a function
		point_templ->Set( v8::String::New("gogo"), v8::FunctionTemplate::New(LogCallback));
		
		
		// instantiate the template, insert into the context
		v8::Local<v8::Object> obj = point_templ->NewInstance();
		obj->SetInternalField(0, v8::External::New(this)); // stash C++ pointer
		global->Set( v8::String::New("layoutTestController"), obj );
		
		return true;
	}
	// <-- Debug
	
	
	v8::HandleScope hs;
	v8::Handle<v8::Context> context = WebCore::V8Proxy::context(d->frame);
	v8::Context::Scope scope(context);	
	v8::Handle<v8::Object> global = context->Global();
	global->Set( v8::String::New(name), obj );
	// TODO : we may have to specify that this object does not get deleted when 
	// the context is reset?
	return true;
	
	/*
	v8::Handle<v8::ObjectTemplate> temp;

	v8::HandleScope hs;
	v8::Handle<v8::Context> context = WebCore::V8Proxy::context(d->frame);
	v8::Context::Scope scope(context);
	
	v8::Handle<v8::Object> global = context->Global();
	
	// make the template (only has to be done once)
	v8::Handle<v8::ObjectTemplate> point_templ = v8::ObjectTemplate::New();
	point_templ->SetInternalFieldCount(1);
	point_templ->SetAccessor(v8::String::New("lala"), GetPointX, SetPointX);
	
	// instantiate the template, insert into the context
	v8::Local<v8::Object> obj = point_templ->NewInstance();
	obj->SetInternalField(0, v8::External::New(this)); // stash C++ pointer
	global->Set( v8::String::New("layoutTestController"), obj );
	
	printf(" * * * * * *\n");
	return temp;
	*/
}

WebCore::Frame* WebFrame::webcoreFrame() 
{ 
	return d->frame; 
}

WebCore::AccessibilityObject* WebFrame::getFocusedAccessibleElement()
{
#if HAVE(ACCESSIBILITY)
    if (!AXObjectCache::accessibilityEnabled())
        AXObjectCache::enableAccessibility();

    if (!d->frame || !d->frame->document())
        return 0;

    RenderView* root = WebCore::toRenderView(d->frame->document()->renderer());
    if (!root)
        return 0;

    AccessibilityObjectWrapper* wrapper =  d->frame->document()->axObjectCache()->getOrCreate(root)->wrapper();
    if (!wrapper)
        return 0;

	return wrapper->accessibilityObject();
//    return webkit_accessible_get_focused_element(WEBKIT_ACCESSIBLE(wrapper));
#else
	return 0;
#endif
}

#define WEBOS_PRINT_JPG_QUALITY 95
#define WEBOS_PRINT_ZOOM_FACTOR 96.0f;
//#define WEBOS_PRINT_DEBUG

bool WebFrame::isPrinting()
{
    return m_isPrinting;
}

void WebFrame::setPrinting(bool isPrinting)
{
    m_isPrinting = isPrinting;
    d->frame->script()->setPaused(isPrinting);
    if (m_isPrinting) {
        d->frame->document()->suspendActiveDOMObjects();
        //Temporary workaround for floating frame bug while printing
        if(page() && page()->view() && page()->view()->client())
            page()->view()->client()->suspendAppRendering();
    }
    else {
        d->frame->document()->resumeActiveDOMObjects();

        if(page() && page()->view() && page()->view()->client())
            page()->view()->client()->resumeAppRendering();
    }
}

/**
 * Prints this frame using swaths.  For now, this generates one JPEG file per page (PNG is not supported)
 *
 * @param lpsJobId        The Luna Print Service job Id
 * @param printableWidth  The printable width of the page in pixels
 * @param printableHeight The printable height of the page in pixels
 * @param printDpi        The print Dpi
 * @param landscape       The orientation of the page, TRUE=landscape, FALSE=portrait
 * @param reverseOrder    The order to print pages, TRUE=last-to-first, FALSE=first-to-last
 */
void WebFrame::print(int lpsJobId, int printableWidth, int printableHeight, int printDpi, bool landscape, bool reverseOrder)
{
    g_debug("%s %d %d %d %d %s %s", __PRETTY_FUNCTION__, lpsJobId, printableWidth, printableHeight, printDpi, landscape?"landscape":"portrait", reverseOrder?"faceup":"facedown");

    if (!registerLpsClient()) {
        return;
    }

    /* Set up render variables */
    PrintContext* printContext = new PrintContext(d->frame);
    WebFramePrintCtx* webFramePrintCtx = new WebFramePrintCtx(this, lpsJobId, printContext);
    webFramePrintCtx->renderZoomFactor    = printDpi / WEBOS_PRINT_ZOOM_FACTOR;
    webFramePrintCtx->renderPageWidth     = landscape ? printableHeight : printableWidth;
    webFramePrintCtx->renderPageHeight    = landscape ? printableWidth : printableHeight;
    webFramePrintCtx->renderSwathHeight   = MIN(printDpi, webFramePrintCtx->renderPageHeight);
    webFramePrintCtx->reverseOrder        = reverseOrder;


    /* Set up the print context */
    float printContextWidth = (float)webFramePrintCtx->renderPageWidth / webFramePrintCtx->renderZoomFactor;
    float printContextHeight = (float)webFramePrintCtx->renderPageHeight / webFramePrintCtx->renderZoomFactor;
    printContext->begin(printContextWidth);
    this->setPrinting();
    float outPageHeight = 0.0;
    printContext->computePageRects(FloatRect(0.0, 0.0, printContextWidth, printContextHeight),
                                   0,   /* headerHeight */
                                   0,   /* footerHeight */
                                   1.0, /* userScaleFactor */
                                   outPageHeight  /* outPageHeight */);

    webFramePrintCtx->totalPages = printContext->pageCount();
    webFramePrintCtx->currentPage = reverseOrder ? webFramePrintCtx->totalPages : 1;

    if (webFramePrintCtx->totalPages > 0 && webFramePrintCtx->currentPage <= webFramePrintCtx->totalPages) {
        if (lpsJobId <= 0) {
#ifdef WEBKIT_PRINT_TESTING_ENABLED
            // We should never have to call printers/getCurrent since
            // we should be passed a valid job id already associated with a printer id.
            // This is for testing purposes only!
            webFramePrintCtx->jobCloseNeeded = true;
            if (!sendLpsPrintersGetCurrent(webFramePrintCtx)) {
                cleanupPrintContext(webFramePrintCtx);
                return;
            }
#else
            sendLpsJobsSetRenderStatus(lpsJobId, RENDER_STATUS_ERR_INVALID_JOB, "Webkit print: invalid job ID");
            cleanupPrintContext(webFramePrintCtx);
            return;
#endif
        } else {
            if (!sendLpsJobsNewTempFile(webFramePrintCtx)) {
                sendLpsJobsSetRenderStatus(lpsJobId, RENDER_STATUS_ERR_LUNA_SERVICE, "Webkit print: jobs/newTempFile failed");
                cleanupPrintContext(webFramePrintCtx);
            }
        }
    } else {
        sendLpsJobsSetRenderStatus(lpsJobId, RENDER_STATUS_ERR_INVALID_JOB, "Webkit print: invalid page range");
        cleanupPrintContext(webFramePrintCtx);
    }
}

/**
 * Kicks off the rendering of the current page to a JPEG file after allocating a file in the file cache.
 * Sets up the page-specific variables in the WebFramePrintCtx.
 *
 * @param WebFramePrintCtx  The context for the print job used in the calls to luna service
 */
void WebFrame::renderPageBegin (WebFramePrintCtx *webFramePrintCtx) {
    webFramePrintCtx->y = 0;
    webFramePrintCtx->renderDone = false;
    webFramePrintCtx->blankRemaining = false;
    webFramePrintCtx->start_time = palm_time_ms();

    if((webFramePrintCtx->fp = fopen(webFramePrintCtx->pathName.c_str(), "wb")) == NULL) {
        g_warning("[WebFrame::renderPageBegin] File %s could not be opened", webFramePrintCtx->pathName.c_str());
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_ERR_FILE, "Webkit print: file open error");
        cleanupPrintContext(webFramePrintCtx);
        return;
    }

    /* Set up the PG context and surface variables */
    webFramePrintCtx->pgContext = PGContext::create();
    webFramePrintCtx->pgSurface = PGSurface::create(webFramePrintCtx->renderPageWidth,
                                             webFramePrintCtx->renderSwathHeight,
                                             false /* alpha */);
    webFramePrintCtx->pgContext->setSurface(webFramePrintCtx->pgSurface);
    webFramePrintCtx->pgContext->clearRect(0, 0, webFramePrintCtx->renderPageWidth, webFramePrintCtx->renderSwathHeight, PColor32(0xff,0xff,0xff,0xff));

    /* Set up the graphics context */
    webFramePrintCtx->graphicsContext = new GraphicsContext(webFramePrintCtx->pgContext);

    /* Set up the jpeg compressor */
    webFramePrintCtx->jpgInfo = new jpeg_compress_struct;
    webFramePrintCtx->jpgErr = new jpeg_error_mgr;

    bool result = webFramePrintCtx->pgSurface->openJPG(webFramePrintCtx->fp, webFramePrintCtx->renderPageWidth, webFramePrintCtx->renderPageHeight, webFramePrintCtx->jpgInfo, webFramePrintCtx->jpgErr, WEBOS_PRINT_JPG_QUALITY);
    if(!result ||
       !sendLpsJobsResizeTempFile(webFramePrintCtx, webFramePrintCtx->renderSwathHeight * webFramePrintCtx->renderPageWidth * 3)) {
        g_warning("[WebFrame::renderPageBegin] JPEG File %s could not be set up properly", webFramePrintCtx->pathName.c_str());
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_ERR_FILE, "Webkit print: jpeg error");
        cleanupPageContext(webFramePrintCtx);
        cleanupPrintContext(webFramePrintCtx);
     }
}


/**
 * Spools a swath of the page content to a GraphicsContext.
 *
 * @param currentPageRect    The IntRect of the current page as calculated by the PrintContext
 * @param ctx                The GraphicsContext to paint the contents of the page to
 * @param swathRect          The swath of the content to render - this is in printable dimensions
 * @param printContextWidth  The width of the PrintContext, which is the printable width scaled by the 1/renderZoomFactor
 * @param renderZoomFactor   The renderZoomFactor used to scale the PrintContext
 */
void WebFrame::spoolSwath(WebCore::IntRect currentPageRect, WebCore::GraphicsContext& ctx, const WebCore::IntRect& swathRect, float printContextWidth, float renderZoomFactor)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    float scale = printContextWidth / (float)currentPageRect.width();
    float finalScale = scale * renderZoomFactor;
    FloatRect bandRect(swathRect);
    bandRect.scale(1.0/finalScale);
    FloatRect pageFloatRect(currentPageRect);

    int maxY = pageFloatRect.y() + pageFloatRect.height();
    int maxX = pageFloatRect.x() + pageFloatRect.width();

    pageFloatRect.setY(pageFloatRect.y() + bandRect.y());
    pageFloatRect.setHeight(bandRect.height());

    if ((pageFloatRect.y() + pageFloatRect.height()) > maxY)
    {
        pageFloatRect.setHeight(maxY - pageFloatRect.y());
    }

    pageFloatRect.setX(pageFloatRect.x() + bandRect.x());
    pageFloatRect.setWidth(bandRect.width());

    if ((pageFloatRect.x() + pageFloatRect.width()) > maxX)
    {
        pageFloatRect.setWidth(maxX - pageFloatRect.x());
    }
#ifdef WEBOS_PRINT_DEBUG
    printf("[WebFrame::spoolSwath] renderZoomFactor=%f, scale=%f, finalScale=%f \n", renderZoomFactor, scale, finalScale);
    printf("[WebFrame::spoolSwath] current page rect: x=%d, y=%d, w=%d, h=%d \n", currentPageRect.x(), currentPageRect.y(), currentPageRect.width(), currentPageRect.height());
    printf("[WebFrame::spoolSwath] swath rect: x=%d, y=%d, w=%d, h=%d \n", swathRect.x(), swathRect.y(), swathRect.width(), swathRect.height());
    printf("[WebFrame::spoolSwath] rendering pageRect: x=%f, y=%f, w=%f, h=%f \n", pageFloatRect.x(), pageFloatRect.y(), pageFloatRect.width(), pageFloatRect.height());
#endif

    //Hacky solution to fix floating-point precision
    pageFloatRect.inflateX(0.5);
    pageFloatRect.inflateY(0.5);
    IntRect pageIntRect(pageFloatRect);
    ctx.save();
    ctx.scale(FloatSize(finalScale, finalScale));
    ctx.translate(-pageIntRect.x(), -pageIntRect.y());
#ifdef WEBOS_PRINT_DEBUG
    printf("[WebFrame::spoolSwath] renderSwathRect: x=%d, y=%d, w=%d, h=%d \n", pageIntRect.x(), pageIntRect.y(), pageIntRect.width(), pageIntRect.height());
#endif

    ctx.clip(pageIntRect);
    d->frame->view()->paintContents(&ctx, pageIntRect);
    ctx.restore();
}

/**
 * Gets the page height in printable dimensions
 *
 * @param pageRect           The IntRect of the current page as calculated by the PrintContext
 * @param printContextWidth  The width of the PrintContext, which is the printable width scaled by the 1/renderZoomFactor
 * @param renderZoomFactor   The renderZoomFactor used to scale the PrintContext
 */
int WebFrame::getPageHeight(const WebCore::IntRect& pageRect, float printContextWidth, float renderZoomFactor)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    float currPageWidth = (float)(pageRect.width()) / renderZoomFactor;
    return (int)( (float)(pageRect.height()) * (printContextWidth / currPageWidth));
}

/**
 * Gets the actual height of the swath for rendering, based on how much content is available for rendering
 *
 * @param renderPageHeight  The total printable height of the final image
 * @param y                 The y-coordinate of the current location on the image, i.e., where we've rendered to so far
 * @param renderSwathHeight The desired swath height
 */
int WebFrame::getActualSwathHeight(int renderPageHeight, int y, int renderSwathHeight)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    if (y >= renderPageHeight)
        return 0;

    if (renderSwathHeight <= (renderPageHeight - y))
        return renderSwathHeight;

    return renderPageHeight - y;
}

/**
 * Initializes the luna service handle
 *
 */
bool WebFrame::registerLpsClient() {
    g_debug("%s", __PRETTY_FUNCTION__);
    if (m_lpsClient == NULL) {
        LSError lserror;
        LSErrorInit(&lserror);

        bool retVal = LSRegister(NULL, &m_lpsClient, &lserror);
        if (!retVal) {
            g_warning("[WebFrame::registerLpsClient] Failed to get Luna Print Service handle: %s", lserror.message);
            m_lpsClient = NULL;
            LSErrorFree(&lserror);
            return false;
        } else {
            retVal = LSGmainAttach(m_lpsClient, Palm::WebGlobal::mainLoop(), &lserror);
            if (!retVal) {
                g_warning("[WebFrame::registerLpsClient] LSGmainAttach failed: %s", lserror.message);
                m_lpsClient = NULL;
                LSErrorFree(&lserror);
                return false;
            }
        }
    }
    g_debug("[WebFrame::registerLpsClient] successfully registered Luna Print Service handle");
    return true;
}

/**
 * Destroys the luna service handle
 *
 */
void WebFrame::unregisterLpsClient() {
    g_debug("%s", __PRETTY_FUNCTION__);
    LSError lserror;
    LSErrorInit(&lserror);

    if (m_lpsClient && !LSUnregister(m_lpsClient, &lserror)) {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
}

#ifdef WEBKIT_PRINT_TESTING_ENABLED
/**
 * TESTING PURPOSES ONLY!
 * palm://com.palm.printmgr/printers/getCurrent {}
 *
 */
bool WebFrame::sendLpsPrintersGetCurrent(WebFramePrintCtx *ctx) {
    g_debug("%s: palm://com.palm.printmgr/printers/getCurrent", __PRETTY_FUNCTION__);
    LSError lserror;
    LSErrorInit(&lserror);
    if (!LSCallOneReply(m_lpsClient,
                        "palm://com.palm.printmgr/printers/getCurrent",
                        "{}",
                        lpsPrintersGetCurrentCallback,
                        static_cast<void*>(ctx),
                        NULL,
                        &lserror)) {
        g_warning("[WebFrame::sendLpsJobsGetCurrent] Failed requesting printers/getCurrent: %s",
                  lserror.message);
        LSErrorFree(&lserror);
        return false;
    }
    g_debug("[WebFrame::sendLpsJobsGetCurrent] Successfully requested printers/getCurrent - waiting for callback");
    return true;
}

/**
 * TESTING PURPOSES ONLY!
 * palm://com.palm.printmgr/printers/getCurrent callback
 *
 */
bool WebFrame::lpsPrintersGetCurrentCallback(LSHandle *sh, LSMessage *reply, void *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    WebFramePrintCtx* webFramePrintCtx = static_cast<WebFramePrintCtx*>(ctx);

    std::string jsonRaw = LSMessageGetPayload(reply);
    g_debug("[WebFrame::lpsPrintersGetCurrentCallback] JSON: %s", jsonRaw.c_str());
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    std::string printerId;
    int errorCode = 0;
    std::string errorText;

    if (parser.parse(jsonRaw, inputSchema)) {
        pbnjson::JValue parsed = parser.getDom();
        parsed["errorCode"].asNumber(errorCode);
        if (parsed["returnValue"].asBool()) {
            printerId = parsed["printerID"].asString();
        }
        if (errorCode < 0)
            errorText = parsed["errorText"].asString();
    }

    memset(webFramePrintCtx->printerId, 0, sizeof(webFramePrintCtx->printerId));
    if (errorCode < 0 || printerId.empty()) {
        //use the default printer IP:
        snprintf(webFramePrintCtx->printerId, sizeof(webFramePrintCtx->printerId), "%s", PalmBrowserSettings()->defaultPrinterIP);
    }
    else {
        snprintf(webFramePrintCtx->printerId, sizeof(webFramePrintCtx->printerId), "%s", printerId.data());
    }
    g_debug("[WebFrame::lpsPrintersGetCurrentCallback] start rendering");
    if (!webFramePrintCtx->palmWebFrame->sendLpsJobsOpen(webFramePrintCtx)) {
        cleanupPrintContext(webFramePrintCtx);
    }
    return true;
}


/**
 * TESTING PURPOSES ONLY!
 * palm://com.palm.printmgr/jobs/open
 *   {"printerID":"<printer IP address>","description":"some description","appName":"appName"}
 *
 */
bool WebFrame::sendLpsJobsOpen(WebFramePrintCtx *ctx) {
    g_debug("%s: palm://com.palm.printmgr/jobs/open", __PRETTY_FUNCTION__);
    LSError lserror;
    LSErrorInit(&lserror);
    char *jsonStr = new char[200];
    snprintf(jsonStr, 200, "{\"printerID\":\"%s\",\"description\":\"WebKit print\",\"appName\":\"browser\"}", ctx->printerId);
    g_debug("[WebFrame::sendLpsJobsOpen] jobs/open %s", jsonStr);
    if (!LSCallOneReply(m_lpsClient,
                        "palm://com.palm.printmgr/jobs/open",
                        jsonStr,
                        lpsJobsOpenCallback,
                        static_cast<void*>(ctx),
                        NULL,
                        &lserror)) {
        g_warning("[WebFrame::sendLpsJobsOpen] Failed requesting jobs/open: %s",
                  lserror.message);
        LSErrorFree(&lserror);
        delete[] jsonStr;
        return false;
    }
    g_debug("[WebFrame::sendLpsJobsOpen] Successfully requested jobs/open - waiting for callback");
    delete[] jsonStr;
    return true;
}

/**
 * TESTING PURPOSES ONLY!
 * palm://com.palm.printmgr/jobs/open callback
 *
 */
bool WebFrame::lpsJobsOpenCallback(LSHandle *sh, LSMessage *reply, void *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    WebFramePrintCtx* webFramePrintCtx = static_cast<WebFramePrintCtx*>(ctx);

    std::string jsonRaw = LSMessageGetPayload(reply);
    g_debug("[WebFrame::lpsJobsOpenCallback] JSON: %s", jsonRaw.c_str());
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    int jobId = 0;
    int errorCode = 0;
    std::string errorText;

    if (parser.parse(jsonRaw, inputSchema)) {
        pbnjson::JValue parsed = parser.getDom();
        parsed["errorCode"].asNumber(errorCode);
        if (parsed["returnValue"].asBool()) {
            parsed["jobID"].asNumber(jobId);
        }
        if (errorCode < 0)
            errorText = parsed["errorText"].asString();
    }

    if (errorCode < 0) {
        g_warning("[WebFrame::lpsJobsOpenCallback] Error opening new job: errorCode=%d, errorText = %s",
                  errorCode,
                  errorText.c_str());
        cleanupPrintContext(webFramePrintCtx);
        return true;
    }
    g_debug("[WebFrame::lpsJobsOpenCallback] start rendering");
    webFramePrintCtx->lpsJobId = jobId;
    if (!webFramePrintCtx->palmWebFrame->sendLpsJobsNewTempFile(webFramePrintCtx)) {
        cleanupPrintContext(webFramePrintCtx);
    }
    return true;
}

/**
 * palm://com.palm.printmgr/jobs/close
 *  {"jobID":<int>}
 */
bool WebFrame::sendLpsJobsClose(int lpsJobId) {
    g_debug("%s lpsJobId=%d", __PRETTY_FUNCTION__, lpsJobId);
    const char* const schema =
            "{\"type\" : \"object\", \
              \"properties\" : { \
                  \"jobID\" : {\"type\" : \"integer\"} \
               } \
             }";
    pbnjson::JValue callData = pbnjson::Object();
    callData.put("jobID", lpsJobId);
    pbnjson::JGenerator serializer(NULL);
    std::string payload;
    pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(schema);

    if (serializer.toString(callData, callSchema, payload)) {
        LSError lserror;
        LSErrorInit(&lserror);
        g_debug("[WebFrame::sendLpsJobsClose] payload = %s", payload.c_str());

        if (!LSCallOneReply(m_lpsClient,
                            "palm://com.palm.printmgr/jobs/close",
                            payload.c_str(),
                            NULL,
                            NULL,
                            NULL,
                            &lserror)) {
            g_warning("[WebFrame::sendLpsJobsClose] Failed to close job %d: %s",
                      lpsJobId,
                      lserror.message);
            LSErrorFree(&lserror);
            return false;
        }
    }
    else
        return false;
    g_debug("[WebFrame::sendLpsJobsClose] Successfully requested jobs/close");
    return true;
}
#endif

/**
 * palm://com.palm.printmgr/jobs/setRenderStatus
 *   {"jobID":1,"renderResultCode":-1,"renderResultText":"unknown error occurred"}
 */
bool WebFrame::sendLpsJobsSetRenderStatus(int lpsJobId, int code, const char* text) {
    g_debug("%s lpsJobId=%d", __PRETTY_FUNCTION__, lpsJobId);
    const char* const schema =
            "{\"type\" : \"object\", \
              \"properties\" : { \
                  \"jobID\" : {\"type\" : \"integer\"}, \
                  \"renderResultCode\" : {\"type\" : \"integer\"}, \
                  \"renderResultText\" : {\"type\" : \"string\"} \
              } \
             }";
    pbnjson::JValue callData = pbnjson::Object();
    callData.put("jobID", lpsJobId);
    callData.put("renderResultCode", code);
    callData.put("renderResultText", text);

    pbnjson::JGenerator serializer(NULL);
    std::string payload;
    pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(schema);

    if (serializer.toString(callData, callSchema, payload)) {
        LSError lserror;
        LSErrorInit(&lserror);
        g_debug("[WebFrame::sendLpsJobsSetRenderStatus] payload = %s", payload.c_str());

        if (!LSCallOneReply(m_lpsClient,
                            "palm://com.palm.printmgr/jobs/setRenderStatus",
                            payload.c_str(),
                            NULL,
                            NULL,
                            NULL,
                            &lserror)) {
            g_warning("[WebFrame::sendLpsJobsSetRenderStatus] Failed to set render status %d: %s",
                      lpsJobId,
                      lserror.message);
            LSErrorFree(&lserror);
            return false;
        }
    }
    else
        return false;
    g_debug("[WebFrame::sendLpsJobsSetRenderStatus] Successfully requested jobs/setRenderStatus");
    return true;
}

/**
 * palm://com.palm.printmgr/jobs/newTempFile
 *   {"jobID":<int>,"fileExtension":"jpg"}
 */
bool WebFrame::sendLpsJobsNewTempFile(WebFramePrintCtx *ctx) {
    g_debug("%s: palm://com.palm.printmgr/jobs/newTempFile", __PRETTY_FUNCTION__);
    const char* const schema =
            "{\"type\" : \"object\", \
              \"properties\" : { \
                  \"jobID\" : {\"type\" : \"integer\"}, \
                  \"fileExtension\" : {\"type\" : \"string\"} \
               } \
             }";
    pbnjson::JValue callData = pbnjson::Object();
    callData.put("jobID", ctx->lpsJobId);
    callData.put("fileExtension", "jpg");


    pbnjson::JGenerator serializer(NULL);
    std::string payload;
    pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(schema);

    if (serializer.toString(callData, callSchema, payload)) {
        g_debug("[WebFrame::sendLpsJobsNewTempFile] payload = %s", payload.c_str());
        LSError lserror;
        LSErrorInit(&lserror);

        if (!LSCallOneReply(m_lpsClient,
                            "palm://com.palm.printmgr/jobs/newTempFile",
                            payload.c_str(),
                            lpsJobsNewTempFileCallback,
                            static_cast<void*>(ctx),
                            NULL,
                            &lserror)) {
            g_warning("[WebFrame::sendLpsJobsNewTempFile] Failed requesting jobs/newTempFile for jobId %d: %s",
                      ctx->lpsJobId,
                      lserror.message);
            LSErrorFree(&lserror);
            return false;
        }
    }
    else
        return false;
    g_debug("[WebFrame::sendLpsJobsNewTempFile] Successfully requested jobs/newTempFile - waiting for callback");
    return true;
}

/**
 * palm://com.palm.printmgr/jobs/newTempFile callback
 *
 */
bool WebFrame::lpsJobsNewTempFileCallback(LSHandle *sh, LSMessage *reply, void *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    WebFramePrintCtx* webFramePrintCtx = static_cast<WebFramePrintCtx*>(ctx);

	std::string jsonRaw = LSMessageGetPayload(reply);
    g_debug("[WebFrame::lpsJobsNewTempFileCallback] JSON: %s", jsonRaw.c_str());
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    int errorCode = 0;
    std::string errorText;


    if (parser.parse(jsonRaw, inputSchema)) {
		pbnjson::JValue parsed = parser.getDom();
        parsed["errorCode"].asNumber(errorCode);
        if (parsed["returnValue"].asBool()) {
            webFramePrintCtx->pathName = parsed["pathName"].asString();
        }
        if (errorCode < 0)
            errorText = parsed["errorText"].asString();
    }

    if (errorCode < 0 || webFramePrintCtx->pathName.empty()) {
        g_warning("[WebFrame::lpsJobsNewTempFileCallback] Error occurred getting temp file name: errorCode=%d, errorText = %s",
                  errorCode,
                  errorText.c_str());
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, errorCode, errorText.c_str());
        cleanupPrintContext(webFramePrintCtx);
        return true;
    }


    webFramePrintCtx->palmWebFrame->renderPageBegin(webFramePrintCtx);

    return true;
}

/**
 * palm://com.palm.printmgr/jobs/resizeTempFile
 *   {"pathName":"</path/to/file>","newSize":<int>}
 */
bool WebFrame::sendLpsJobsResizeTempFile(WebFramePrintCtx *ctx, int newSize) {
    g_debug("%s: palm://com.palm.printmgr/jobs/resizeTempFile", __PRETTY_FUNCTION__);
    const char* const schema =
            "{\"type\" : \"object\", \
              \"properties\" : { \
                  \"pathName\" : {\"type\" : \"string\"}, \
                  \"newSize\" : {\"type\" : \"integer\"} \
               } \
             }";
    pbnjson::JValue callData = pbnjson::Object();
    callData.put("pathName", ctx->pathName.c_str());
    callData.put("newSize", newSize);


    pbnjson::JGenerator serializer(NULL);
    std::string payload;
    pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(schema);

    if (serializer.toString(callData, callSchema, payload)) {
        g_debug("[WebFrame::sendLpsJobsNewTempFile] payload = %s", payload.c_str());
        LSError lserror;
        LSErrorInit(&lserror);

        if (!LSCallOneReply(m_lpsClient,
                            "palm://com.palm.printmgr/jobs/resizeTempFile",
                            payload.c_str(),
                            lpsJobsResizeTempFileCallback,
                            static_cast<void*>(ctx),
                            NULL,
                            &lserror)) {
            g_warning("[WebFrame::sendLpsJobsResizeTempFile] Failed requesting jobs/resizeTempFile %s %d: %s",
                      ctx->pathName.c_str(),
                      newSize,
                      lserror.message);
            LSErrorFree(&lserror);
            return false;
        }
    }
    else
        return false;
    g_debug("[WebFrame::sendLpsJobsResizeTempFile] Successfully requested jobs/resizeTempFile");
    return true;
}
/**
 * palm://com.palm.printmgr/jobs/resizeTempFile callback
 *
 */
bool WebFrame::lpsJobsResizeTempFileCallback(LSHandle *sh, LSMessage *reply, void *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    WebFramePrintCtx* webFramePrintCtx = static_cast<WebFramePrintCtx*>(ctx);

    std::string jsonRaw = LSMessageGetPayload(reply);
    g_debug("[WebFrame::lpsJobsResizeTempCallback] JSON: %s", jsonRaw.c_str());
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    int errorCode = 0;
    std::string errorText;

    if (parser.parse(jsonRaw, inputSchema)) {
        pbnjson::JValue parsed = parser.getDom();
        parsed["errorCode"].asNumber(errorCode);
        if (errorCode < 0)
            errorText = parsed["errorText"].asString();
    }

    if (errorCode < 0) {
        g_warning("[WebFrame::lpsJobsResizeTempCallback] Error resizing temp file: errorCode=%d, errorText = %s",
                  errorCode,
                  errorText.c_str());
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, errorCode, errorText.c_str());
        cleanupPrintContext(webFramePrintCtx);
        return true;
    }

    if (webFramePrintCtx->renderDone) {
        g_debug("[WebFrame::lpsJobsResizeTempFileCallback] Rendered page %d in %lu ms", webFramePrintCtx->currentPage, palm_time_ms() - webFramePrintCtx->start_time);
        if (!webFramePrintCtx->palmWebFrame->sendLpsJobsAddFile(webFramePrintCtx, webFramePrintCtx->pathName.c_str())) {
            webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_ERR_LUNA_SERVICE, "Webkit print: jobs/addFile error");
            cleanupPrintContext(webFramePrintCtx);
            return true;
        }
    }
    else {
        if (webFramePrintCtx->blankRemaining) {
            //g_debug("[WebFrame::lpsJobsResizeTempFileCallback] Blank out remaining pixels");
            webFramePrintCtx->pgSurface->appendWhiteToJPG(webFramePrintCtx->jpgInfo);
            webFramePrintCtx->renderDone = true;
        }
        else {
            //g_debug("[WebFrame::lpsJobsResizeTempFileCallback] Render swath at row %d", webFramePrintCtx->y);
            IntRect currentPageRect = webFramePrintCtx->printContext->pageRect(webFramePrintCtx->currentPage-1);  // the current page's PrintContext rect
            float printContextWidth = (float)webFramePrintCtx->renderPageWidth / webFramePrintCtx->renderZoomFactor;
            int sourcePageHeight = webFramePrintCtx->palmWebFrame->getPageHeight(currentPageRect, printContextWidth, webFramePrintCtx->renderZoomFactor);

            int actualSwathHeight = webFramePrintCtx->palmWebFrame->getActualSwathHeight(webFramePrintCtx->renderPageHeight, webFramePrintCtx->y, webFramePrintCtx->renderSwathHeight);
            if (actualSwathHeight > 0) {
                webFramePrintCtx->pgContext->clearRect(0, 0, webFramePrintCtx->renderPageWidth, webFramePrintCtx->renderSwathHeight, PColor32(0xff,0xff,0xff,0xff));
                IntRect swathRect(0, 0, webFramePrintCtx->renderPageWidth, 0);
                swathRect.setY(webFramePrintCtx->y);
                swathRect.setHeight(actualSwathHeight);
                webFramePrintCtx->palmWebFrame->spoolSwath(currentPageRect, *(webFramePrintCtx->graphicsContext), swathRect, printContextWidth, webFramePrintCtx->renderZoomFactor);
                webFramePrintCtx->pgSurface->appendToJPG(webFramePrintCtx->jpgInfo, actualSwathHeight);
            }
            webFramePrintCtx->y += webFramePrintCtx->renderSwathHeight;

            if (webFramePrintCtx->y > webFramePrintCtx->renderPageHeight) {
                 webFramePrintCtx->renderDone = true;
             } else {
                 if (webFramePrintCtx->y > sourcePageHeight)
                     webFramePrintCtx->blankRemaining = true;
             }

        }

        if (webFramePrintCtx->renderDone) {
            //We're done rendering!  Clean up and use the final size of the file in the call to resizeTempFile
            cleanupPageContext(webFramePrintCtx);
        }
        struct stat fpStatus;
        int fileSize = 0;
        memset(&fpStatus, 0, sizeof(fpStatus));
        if (stat(webFramePrintCtx->pathName.c_str(), &fpStatus) == 0) {
            fileSize = fpStatus.st_size;
            g_debug("[WebFrame::lpsJobsResizeTempFileCallback] Current file size: %d bytes", fileSize);
        }

        if (!webFramePrintCtx->renderDone) {
            //We still have another swath to render.  Grow the temp file with the uncompressed estimate
            fileSize += (webFramePrintCtx->renderSwathHeight*webFramePrintCtx->renderPageWidth*3);
        }

        if (!webFramePrintCtx->palmWebFrame->sendLpsJobsResizeTempFile(webFramePrintCtx, fpStatus.st_size)) {
            webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_ERR_LUNA_SERVICE, "Webkit print: jobs/resizeTempFile error");
            cleanupPageContext(webFramePrintCtx);
            cleanupPrintContext(webFramePrintCtx);
        }
    }

    return true;
}

/**
 * palm://com.palm.printmgr/jobs/addFile
 *   {"jobID":<int>,"pathName":"/path/to/imageFile", "currentPage":<int>, "totalPages":<int>}
 */
bool WebFrame::sendLpsJobsAddFile(WebFramePrintCtx *ctx, const char* pathName) {
    g_debug("%s pathName = %s", __PRETTY_FUNCTION__, pathName);
    const char* const schema =
            "{\"type\" : \"object\", \
              \"properties\" : { \
                  \"jobID\" : {\"type\" : \"integer\"}, \
                  \"pathName\" : {\"type\" : \"string\"}, \
                  \"currentPage\" : {\"type\" : \"integer\"}, \
                  \"totalPages\" : {\"type\" : \"integer\"} \
               } \
             }";
    pbnjson::JValue callData = pbnjson::Object();
    callData.put("jobID", ctx->lpsJobId);
    callData.put("pathName", pathName);

    if (ctx->reverseOrder)
        callData.put("currentPage", (ctx->totalPages - ctx->currentPage + 1));
    else
        callData.put("currentPage", ctx->currentPage);
    callData.put("totalPages", ctx->totalPages);

    pbnjson::JGenerator serializer(NULL);
    std::string payload;
    pbnjson::JSchema callSchema = pbnjson::JSchemaFragment(schema);

    if (serializer.toString(callData, callSchema, payload)) {
        g_debug("[WebFrame::sendLpsJobsAddFile] payload = %s", payload.c_str());
        LSError lserror;
        LSErrorInit(&lserror);

        if (!LSCallOneReply(m_lpsClient,
                            "palm://com.palm.printmgr/jobs/addFile",
                            payload.c_str(),
                            lpsJobsAddFileCallback,
                            static_cast<void*>(ctx),
                            NULL,
                            &lserror)) {
            g_warning("[WebFrame::sendLpsJobsAddFile] Failed requesting jobs/AddFile, pathName=%s, jobId=%d: %s",
                      pathName,
                      ctx->lpsJobId,
                      lserror.message);
            LSErrorFree(&lserror);
            return false;
        }
    }
    else
        return false;
    g_debug("[WebFrame::sendLpsJobsAddFile] Successfully requested jobs/addFile - waiting for callback");
    return true;
}


/**
 * palm://com.palm.printmgr/jobs/addFile callback
 *
 */
bool WebFrame::lpsJobsAddFileCallback(LSHandle *sh, LSMessage *reply, void *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    WebFramePrintCtx* webFramePrintCtx = static_cast<WebFramePrintCtx*>(ctx);

    std::string jsonRaw = LSMessageGetPayload(reply);
    g_debug("[WebFrame::lpsJobsAddFileCallback] JSON: %s", jsonRaw.c_str());
    static pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    int errorCode = 0;
    std::string errorText;

    if (parser.parse(jsonRaw, inputSchema)) {
        pbnjson::JValue parsed = parser.getDom();
        parsed["errorCode"].asNumber(errorCode);
        if (errorCode < 0)
            errorText = parsed["errorText"].asString();
    }

    if (errorCode < 0) {
        g_warning("[WebFrame::lpsJobsAddFileCallback] Error adding file: errorCode=%d, errorText = %s",
                  errorCode,
                  errorText.c_str());
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, errorCode, errorText.c_str());
        cleanupPrintContext(webFramePrintCtx);
        return true;
    }

    bool done = false;
    if ((webFramePrintCtx->reverseOrder && webFramePrintCtx->currentPage == 1) ||
        (!webFramePrintCtx->reverseOrder && webFramePrintCtx->currentPage == webFramePrintCtx->totalPages))
        done = true;


    if (done) {
        //No more pages to render - close the job
        g_debug("[WebFrame::lpsJobsAddFileCallback] No more pages to render - close the job");
        webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_DONE, "Webkit print: rendering done");
#ifdef WEBKIT_PRINT_TESTING_ENABLED
        if (webFramePrintCtx->jobCloseNeeded &&
            !webFramePrintCtx->palmWebFrame->sendLpsJobsClose(webFramePrintCtx->lpsJobId)) {
            g_warning("[WebFrame::lpsJobsAddFileCallback] Could not close job %d", webFramePrintCtx->lpsJobId);
        }
#endif
        cleanupPrintContext(webFramePrintCtx);
    }
    else {
        if (webFramePrintCtx->reverseOrder)
            webFramePrintCtx->currentPage--;
        else
            webFramePrintCtx->currentPage++;
        g_debug("[WebFrame::lpsJobsAddFileCallback] Render the next page: %d", webFramePrintCtx->currentPage);
        if (!webFramePrintCtx->palmWebFrame->sendLpsJobsNewTempFile(webFramePrintCtx)) {
            webFramePrintCtx->palmWebFrame->sendLpsJobsSetRenderStatus(webFramePrintCtx->lpsJobId, RENDER_STATUS_ERR_LUNA_SERVICE, "Webkit print: jobs/newTempFile failed");
            cleanupPrintContext(webFramePrintCtx);
        }
    }
    return true;
}


/**
 * Clean up the print context
 *
 */
void WebFrame::cleanupPrintContext(WebFramePrintCtx *ctx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    if (ctx) {
        if (ctx->printContext) {
            ctx->printContext->end();
            ctx->palmWebFrame->setPrinting(false);
            delete ctx->printContext;
        }
        delete ctx;
    }
}

/**
 * Clean up the image context related to an individual page
 *
 */
void WebFrame::cleanupPageContext(WebFramePrintCtx *webFramePrintCtx)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    if (webFramePrintCtx) {
        if (webFramePrintCtx->jpgInfo) {
            webFramePrintCtx->pgSurface->closeJPG(webFramePrintCtx->jpgInfo);
            delete webFramePrintCtx->jpgInfo;
            webFramePrintCtx->jpgInfo = NULL;
        }

        if (webFramePrintCtx->jpgErr) {
           delete webFramePrintCtx->jpgErr;
           webFramePrintCtx->jpgErr = NULL;
        }

        if (webFramePrintCtx->fp) {
            fclose(webFramePrintCtx->fp);
            webFramePrintCtx->fp = NULL;
        }
        if (webFramePrintCtx->pgSurface)
            webFramePrintCtx->pgSurface->releaseRef();

        webFramePrintCtx->pgSurface = NULL;
        webFramePrintCtx->pgContext = NULL;

        if (webFramePrintCtx->graphicsContext) {
            delete webFramePrintCtx->graphicsContext;
            webFramePrintCtx->graphicsContext = NULL;
        }
    }
}

bool WebFrame::createJsObject(void* objClass, const char* name, void** obj, void** browserFuncs)
{
    if (!s_browserFuncsInitialized) {
        s_browserFuncsInitialized = true;

        // Setup the browser funcs for use by native JS objects
        memset(&s_browserFuncs, 0, sizeof(NPNetscapeFuncs));
        s_browserFuncs.size = sizeof (NPNetscapeFuncs);
        s_browserFuncs.version = NP_VERSION_MINOR;
        s_browserFuncs.geturl = NPN_GetURL;
        s_browserFuncs.posturl = (NPN_PostURLProcPtr)NPN_PostURL;
        s_browserFuncs.requestread = NPN_RequestRead;
        s_browserFuncs.newstream = NPN_NewStream;
        s_browserFuncs.write = (NPN_WriteProcPtr)NPN_Write;
        s_browserFuncs.destroystream = NPN_DestroyStream;
        s_browserFuncs.status = NPN_Status;
        s_browserFuncs.uagent = NPN_UserAgent;
        s_browserFuncs.memalloc = (NPN_MemAllocProcPtr)NPN_MemAlloc;
        s_browserFuncs.memfree = NPN_MemFree;
        s_browserFuncs.memflush = (NPN_MemFlushProcPtr)NPN_MemFlush;
        s_browserFuncs.reloadplugins = NPN_ReloadPlugins;
        s_browserFuncs.geturlnotify = NPN_GetURLNotify;
        s_browserFuncs.posturlnotify = (NPN_PostURLNotifyProcPtr)NPN_PostURLNotify;
        s_browserFuncs.getvalue = NPN_GetValue;
        s_browserFuncs.setvalue = NPN_SetValue;
        s_browserFuncs.invalidaterect = NPN_InvalidateRect;
        s_browserFuncs.invalidateregion = NPN_InvalidateRegion;
        s_browserFuncs.forceredraw = NPN_ForceRedraw;
        s_browserFuncs.getJavaEnv = NPN_GetJavaEnv;
        s_browserFuncs.getJavaPeer = NPN_GetJavaPeer;
        s_browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
        s_browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
#if USE(HIDEV8)
        s_browserFuncs.releasevariantvalue = NPN_ReleaseVariantValue;
        s_browserFuncs.getstringidentifier = NPN_GetStringIdentifier;
        s_browserFuncs.getstringidentifiers = NPN_GetStringIdentifiers;
        s_browserFuncs.getintidentifier = NPN_GetIntIdentifier;
        s_browserFuncs.identifierisstring = NPN_IdentifierIsString;
        s_browserFuncs.utf8fromidentifier = NPN_UTF8FromIdentifier;
        s_browserFuncs.intfromidentifier = NPN_IntFromIdentifier;
        s_browserFuncs.createobject = NPN_CreateObject;
        s_browserFuncs.retainobject = NPN_RetainObject;
        s_browserFuncs.releaseobject = NPN_ReleaseObject;
        s_browserFuncs.invoke = NPN_Invoke;
        s_browserFuncs.invokeDefault = NPN_InvokeDefault;
        s_browserFuncs.evaluate = NPN_Evaluate;
        s_browserFuncs.getproperty = NPN_GetProperty;
        s_browserFuncs.setproperty = NPN_SetProperty;
        s_browserFuncs.removeproperty = NPN_RemoveProperty;
        s_browserFuncs.hasproperty = NPN_HasMethod;
        s_browserFuncs.hasmethod = NPN_HasProperty;
        s_browserFuncs.setexception = NPN_SetException;
        s_browserFuncs.enumerate = NPN_Enumerate;
#else
        s_browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
        s_browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
        s_browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
        s_browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
        s_browserFuncs.identifierisstring = _NPN_IdentifierIsString;
        s_browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
        s_browserFuncs.intfromidentifier = _NPN_IntFromIdentifier;
        s_browserFuncs.createobject = _NPN_CreateObject;
        s_browserFuncs.retainobject = _NPN_RetainObject;
        s_browserFuncs.releaseobject = _NPN_ReleaseObject;
        s_browserFuncs.invoke = _NPN_Invoke;
        s_browserFuncs.invokeDefault = _NPN_InvokeDefault;
        s_browserFuncs.evaluate = _NPN_Evaluate;
        s_browserFuncs.getproperty = _NPN_GetProperty;
        s_browserFuncs.setproperty = _NPN_SetProperty;
        s_browserFuncs.removeproperty = _NPN_RemoveProperty;
        s_browserFuncs.hasproperty = _NPN_HasMethod;
        s_browserFuncs.hasmethod = _NPN_HasProperty;
        s_browserFuncs.setexception = _NPN_SetException;
        s_browserFuncs.enumerate = _NPN_Enumerate;
#endif
    }

    *browserFuncs = &s_browserFuncs;

    if (*obj)
        return false;

#if USE(V8)
    V8Proxy* proxy = V8Proxy::retrieve(d->frame);
    v8::Handle<v8::Context> existingContext = proxy->context();
    if (existingContext.IsEmpty()) {
        g_debug("No existing context and not creating one");
        *obj = 0;
        return false;
    }
#endif

    // Create the javascript object
    *obj = s_browserFuncs.createobject(0, (NPClass*) objClass);
    if (!(*obj))
        return false;

#if USE(V8)
    // set the context for this call.
    v8::HandleScope hs;
    v8::Handle<v8::Context> context = V8Proxy::context(d->frame);
    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> o = createV8ObjectForNPObject( (NPObject*)*obj, 0 );

    v8::Handle<v8::Object> global = context->Global();
    global->Set(v8::String::New(name), o);

#else

    NPObject * npobject = (NPObject*)*obj;
    JSDOMWindow *window = toJSDOMWindow(core(d->frame));
    JSC::ExecState* exec = window->globalExec();
    RefPtr<JSC::Bindings::RootObject> rootObj = core(d->frame)->script()->bindingRootObject();

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

}
