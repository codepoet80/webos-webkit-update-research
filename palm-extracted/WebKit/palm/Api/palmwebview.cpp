/* ============================================================
 * Date  : 2008-09-22
 * Copyright 2008-2010 Palm, Inc. All rights reserved.
 * ============================================================ */
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "palmwebglobal.h"
#include "palmwebview.h"
#include "palmwebviewprivate.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"

#include <stdio.h>
#include <errno.h>
#include <fstream>
#include <sstream>

#include "ClientRect.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "DragController.h"
#include "DragData.h"
#include "Editor.h"
#include "EditorClientPalm.h"
#include "Event.h"
#include "EventNames.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "PopupMenuPalm.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "HTMLFrameOwnerElement.h"
#include "KeyboardCodes.h"
#include "MouseEventWithHitTestResults.h"
#include "NotImplemented.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "PlatformTouchPoint.h"
#include "Settings.h"
#include "SystemTime.h"
#include "RenderBlock.h"
#include "RenderImage.h"
#include "RenderObject.h"
#include "RenderBox.h"
#include "RenderWidget.h"
#include "RenderText.h"
#include "RenderPart.h"
#include "RenderThemePalm.h"
#include "Scrollbar.h"
#include "ScrollView.h"
#include "NodeList.h"
#include "NodeRenderStyle.h"
#include "webosDeviceKeydefs.h"
#include "HTMLNames.h"
#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"
#endif
#include "MediaPlayer.h"
#include "PGFallbackFonts.h"
#include "HTMLNames.h"
#include <wtf/CurrentTime.h>
#include "KURL.h"
#include "FileSystem.h"
#include "ChromeClientPalm.h"
#include "RenderLayer.h"

#include "palmwebpage.h"
#include "palmwebpageprivate.h"
#include "palmwebpageclient.h"
#include "palmwebviewclient.h"
#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "webkitpalmsettings.h"
#include "webkitstats.h"
#include <SimpleStats/SimpleStats.h>
#include "PluginView.h"
#include "RenderView.h"
#include "SharedBuffer.h"

#include <Timer.h>
#include <HTMLLinkElement.h>
#include <HTMLImageElement.h>
#include <CString.h>

#include "DocumentFragment.h"
#include "markup.h"
#include "ReplaceSelectionCommand.h"

#include "SpellingWidgetController.h"
#include "LayerRendererPalm.h"

#include "palmfpshandler.h"

#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayer.h"
#include "ContentLayerPalm.h"
#include "RenderLayerCompositor.h"
#include "LayerPalm.h"
#endif

#if PLATFORM(PG)
#include "PGContext.h"
#include "PGSurface.h"
#endif

#include <sstream>
#include <cmath>

#ifdef INSTRUMENTATION
#include "instrumentation.h"
#endif

#include <ime/palmimedefines.h>

using namespace WebKit;
using namespace WebCore;

namespace Nova {
	namespace Utils {
		bool isBreakChar(UChar ch);
		bool isTerminalPunctuation(UChar ch);
	}
}

namespace Palm
{

static bool TrySendKeyPressForClipboardOperation( unsigned short keyCode );

struct SmartZoomState
{
	SmartZoomState() : sz_wasAutoPositioned(false) { }
	~SmartZoomState() {}
	
	struct LayerCandidate
	{
		LayerCandidate(RenderObject* r, float sc=0.0f, float centerScore=0.0f) : ro(r), scale(sc), centerScore(centerScore) { } 
		LayerCandidate() : ro(0) {} 
		~LayerCandidate() { }
		RenderObject*   ro;
		float			scale;			// what would the scale be for this candidate?
		float			centerScore;	// this is 0..1.0 which rates how close to the center of the box the user clicked.

		
		LayerCandidate( const LayerCandidate& rhs )
		{
			*this=rhs;
		}
		LayerCandidate& operator=( const LayerCandidate& rhs )
		{
			ro=rhs.ro;
			scale=rhs.scale;
			centerScore=rhs.centerScore;
			return *this;
		}
	};

									// indicates the current zoom/scroll offsets
									// were determined by the autozoom algorithm.
	bool							sz_wasAutoPositioned;
	
};

/**
 * Maintain information about the current drag event that may be in progress.
 */
class WebView::SpellingDragInfo {
public:

	SpellingDragInfo() :
		m_prevPtTime(0)
		, m_currPtTime(0)
		, m_prevPt(0,0)
		, m_currPt(0,0)
		, m_downOnWidget(false)
		, m_mouseDown(false) {
	}

	void clear() {
		m_prevPtTime = m_currPtTime = 0;
		m_prevPt = m_currPt = WebCore::IntPoint(0,0);
		m_mouseDown = false;
		m_downOnWidget = false;
	}

	void setMouseDownInfo(bool down, bool downOnWidget = false) {
		if (down) {
			clear();
		}
		m_mouseDown = down;
		m_downOnWidget = downOnWidget;
	}

	void flickStart(int x) {
		m_flickStartX = x;
		m_flickStartTime = palm_monotonic_time_ms();
	}

	void flickEnd(int x) {
		m_flickEndX = x;
		m_flickEndTime = palm_monotonic_time_ms();
	}

	void flickClear() {
		m_flickStartX = 0;
		m_flickStartTime = 0;
		m_flickEndX = 0;
		m_flickEndTime = 0;
	}

	double getFlickSpeed() {
		static short min_flick_delta_x = 10;

		double dx = static_cast<double>(m_flickStartX - m_flickEndX);
		double dt = static_cast<double>(m_flickEndTime - m_flickStartTime) / 1000;
		if (std::abs(dx) > min_flick_delta_x && dt > 0) {
			return dx/dt;
		}
		return 0;
	}

	bool isMouseDown() const {
		return m_mouseDown;
	}

	bool isDownOnWidget() const {
		return m_downOnWidget;
	}

	bool didDrag() const {
		return m_prevPtTime != 0 && m_currPtTime != 0;
	}

	int moveX() {
		return m_currPt.x() - m_prevPt.x();
	}

	void setCurrPt(const WebCore::IntPoint& pt) {
		m_prevPt = m_currPt;
		m_prevPtTime = m_currPtTime;
		m_currPt = pt;
		m_currPtTime = palm_monotonic_time_ms();
	}

private:

	WebCore::IntPoint m_prevPt; ///< Previous mouse position
	WebCore::IntPoint m_currPt; ///< Current mouse position
	unsigned long m_prevPtTime; ///< Time when the previous point was recorded
	unsigned long m_currPtTime; ///< Time when the current point was recorded
	bool m_downOnWidget;        ///< Did the mouse go down on the spelling widget?
	bool m_mouseDown;           ///< Is the mouse currently down?
	int m_flickStartX;			///< x coord at mouse down
	int m_flickEndX;			///< x coord at mouse up
	int m_flickStartTime;		///< time at mouse down
	int m_flickEndTime;			///< time at mouse up
};


#undef COPY_DEBUG

#ifdef COPY_DEBUG 
#define COPY_TRACE(...) \
do { \
    fprintf(stdout, "palmwebview: %s: ", __FUNCTION__ ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)
#else
#define COPY_TRACE(...) (void)0
#endif


#ifndef NDEBUG
#define __SMARTZOOM_DEBUG
#endif

#ifdef __SMARTZOOM_DEBUG
	#define SZLOG		printf
#else
    inline void SZLOG(...) {}
#endif // __SMARTZOOM_DEBUG


class RenderObjectWalker
{
public:
	RenderObjectWalker( RenderLayer* rl, RenderObject* ro, int mx, int my )
        : mouse_x(mx), mouse_y(my), rootLayer(rl)
	{ 
		processLayer(ro->enclosingLayer()); 
	}

	std::vector<RenderObject*>		matching;
    std::vector<RenderLayer*>       matchingLayers;
	int								mouse_x, mouse_y;
    RenderLayer*                    rootLayer;
private:
	void processLayer( RenderLayer* layer )
	{
		RenderLayer* l;
		
		if( !layer )
			return;
		
		collectChildren( layer->renderer() );
		
		l = layer->firstChild();
		while( l )
		{
            IntRect rect = l->boundingBox(rootLayer);
            if (rect.contains(mouse_x, mouse_y))
                matchingLayers.push_back(l);
			processLayer( l );
			l = l->nextSibling();
		}
	}
	
	void collectChildren( RenderObject* o )
	{
		RenderObject* c;
		
		if( o->absoluteOutlineBounds().contains(mouse_x,mouse_y) )
			matching.push_back( o );
		
		// process layers
		//processLayer( o->layer() );
	
		c = o->firstChild();
		while( c )
		{
			collectChildren( c );
			c = c->nextSibling();
		}		
	}

	RenderObjectWalker();
};


// There are two basic algorithms: locate by node (click through) or locate by renderbox.
bool smartzoom_cluster_renderbox( WebView* page, int inMouseX_scaled, int inMouseY_scaled, std::vector<SmartZoomState::LayerCandidate>& outCluster, int& outWinningIndex );
bool smartzoom_cluster_node( WebView* page, int inMouseX_scaled, int inMouseY_scaled, std::vector<SmartZoomState::LayerCandidate>& outCluster, int& outWinningIndex );

// Which algorithm will we use? -- we'll use Node first since it more accurately captures
// what the user was "clicking" at. However, if this does not work, we'll use renderbox. We have
// to do this because the render tree does not have a 1:1 mapping with the DOM tree.
bool smartzoom_cluster( WebView* view, int in_mouse_x_abs_unscaled, int in_mouse_y_abs_unscaled, std::vector<SmartZoomState::LayerCandidate>& widthsSet, int& outWinningIndex )
{
	//bool result = smartzoom_cluster_renderbox( page, in_mouse_x_abs_unscaled, in_mouse_y_abs_unscaled, widthsSet, outWinningIndex );
	//printf( "result from renderbox ...\n" );
	
	std::vector<SmartZoomState::LayerCandidate> box_winners, node_winners;
	int box_winning_index = -1;
	int node_winning_index = -1;
	
	smartzoom_cluster_renderbox( view, in_mouse_x_abs_unscaled, in_mouse_y_abs_unscaled, box_winners, box_winning_index );
	smartzoom_cluster_node( view, in_mouse_x_abs_unscaled, in_mouse_y_abs_unscaled, node_winners, node_winning_index );
	
	
	// we have none or only one winner:
	if( -1 == box_winning_index && -1 == node_winning_index )
		return false;
	
	if( -1 == box_winning_index )
	{
		// the node one won.
		widthsSet.push_back( node_winners[node_winning_index] );
		outWinningIndex = 0;
		return true;
	}
	if( -1 == node_winning_index )
	{
		// the box one won.
		widthsSet.push_back( box_winners[box_winning_index] );
		outWinningIndex = 0;
		return true;
	}
	
	// we have winners from each category. Choose the one with the better center score. -- ?
	if( box_winners[box_winning_index].centerScore > node_winners[node_winning_index].centerScore )
		widthsSet.push_back( box_winners[box_winning_index] );
	else
		widthsSet.push_back( node_winners[node_winning_index] );
	outWinningIndex = 0;
	
	return true;
}

// Sort these based on the box width.
bool SortByWidth( const SmartZoomState::LayerCandidate& left, const SmartZoomState::LayerCandidate& right )
{
	return left.ro->absoluteOutlineBounds().width() < right.ro->absoluteOutlineBounds().width(); 
}

// Sort these candidates by the "proposed" scale factor.
bool SortByScale( const SmartZoomState::LayerCandidate& left, const SmartZoomState::LayerCandidate& right )
{
	return left.scale < right.scale; 
}

// The renderbox algorithms walks the whole render tree and 
bool smartzoom_cluster_renderbox( WebView* view, int in_mouse_x_abs_unscaled, int in_mouse_y_abs_unscaled, std::vector<SmartZoomState::LayerCandidate>& widthsSet, int& outWinningIndex )
{
	Frame* frame = core(view->page())->focusController()->focusedOrMainFrame();
	int win_width, win_height;
	view->client()->getWindowSize(win_width, win_height);		
	int contents_unscaled_width = frame->view()->contentsWidth();
	
	RenderObject* ro = 0;
	float scale = (float) frame->view()->getScale();
	
	SZLOG( " ((( RENDERBOX )))  \n" );
	
	// WebCore APIs expect the mouse clicks to be 1:1 and relative to 
	// current scroll offset in ScrollViewPalm.cpp. 
	
	int sx, sy;
	view->getContentPosition(sx, sy);
	sx = long( sx / scale) ;
	sy = long( sy / scale );
	int in_mouse_x_rel_unscaled = in_mouse_x_abs_unscaled - sx;
	int in_mouse_y_rel_unscaled = in_mouse_y_abs_unscaled - sy;
	
	
	int y_center_delta = (win_height/2) - in_mouse_y_rel_unscaled;
	
	
	SZLOG( "y_center_delta=%d  contents_unscaled_width=%d\n", y_center_delta, contents_unscaled_width );
	SZLOG( "webkit_page_smartzoom() mouse (1:1): %d, %d\n", in_mouse_x_rel_unscaled, in_mouse_y_rel_unscaled );
	

	//
	// Algorithm
	//
	// We want to look at the DOM tree at the mouse coordinate passed in. 
	// we're looking for the WIDTH of the current Node, and we'll adjust the scale/etc. to 
	// snap to this node width.

	// We need some fuzzy logic to see if the node candidate we're going to snap to is "too small" 
	
	
	
	
	// In this first step, we want to collect all the RenderBoxes that
	// are pierced by this coordinate. We'll use this as an opportunity to
	// "throw out" any boxes that obviously won't make the cut --
	// e.g., text
	
	// TODO : if we encounter an edit box, we should make this a special
	// case.
	
	// look at all renderobjects in the tree.
	RenderObjectWalker* walker = new RenderObjectWalker(
        frame->document()->renderer()->enclosingLayer(),
        frame->document()->renderer(), 
		in_mouse_x_abs_unscaled, in_mouse_y_abs_unscaled );
	SZLOG("render_objects.count = %d\n", walker->matching.size() );
	
	std::vector<SmartZoomState::LayerCandidate> candidates;
	
	for( std::vector<RenderObject*>::const_iterator it=walker->matching.begin(); it != walker->matching.end(); it++ )
	{
		ro = *it;
		IntRect r = ro->absoluteOutlineBounds();
		
		// should we add this to our "search" list ?
		// Rules to "throw out" render objects:
		bool add_it = true;
		
		// RULE : if box is a text node.
		// 	These can be as small as a single character, so there are edge cases
		//	where they are not useful.
		if( ro->node() && ro->node()->nodeType() == Node::TEXT_NODE ) add_it = false;
		
		// RULE : width must be > 0
		if( r.width() <= 0 ) add_it = false;
		
		// RULE : if abs box with is less than 1/2 the screen width, do not include it in the solution.
		//if( r.width() < ( win_width / 2 ) ) add_it = false;
		
		// RULE : if box is not "portrait"
		//if( r.height() < r.width() ) add_it = false;
		
		// RULE : if box height is not at least 2x width
		//if( ( r.height() / r.width() ) <= 1 ) add_it = false;
		
		// RULE : if the box width is essentially the width of the entire page, we don't care.
		float box_width_score = fabs( r.width() - contents_unscaled_width )/float(contents_unscaled_width);
		if( box_width_score < 0.1f ) add_it = false; 
		//SZLOG( "boxwidth=%ld width score: %f\n", r.width(), box_width_score );
		
		// Compute a score based on how close the click was to the center of the box. This will
		// be used later when evaluating what box to use.
		float centerScore = 1.0f - ( float(abs(in_mouse_x_abs_unscaled-(r.x()+(r.width()/2)) ) )  
						/
						float( r.width()/2 ) );

		//if( centerScore < 0.1f )
		//	add_it = false;

		//SZLOG( "box[%d %d] w=%d in_mouse_x_abs_unscaled=%d centerScore=%f add=%d\n", 
		//	r.x(), r.right(), r.width(), in_mouse_x_abs_unscaled,
		//	centerScore, add_it );
		
		for( size_t i=0; i<candidates.size(); i++ ) 
		{
			if( candidates[i].ro == ro )
			{
				add_it=false;
				break;
			}
		}
				
		if( add_it )
		{
			candidates.push_back( SmartZoomState::LayerCandidate( ro, 
						float( ((float)win_width ) / r.width() ),
						centerScore
							) );
		}
	}
	
	delete walker;
	
	// sort by width before differentiating.
	std::sort(candidates.begin(), candidates.end(),SortByWidth );
	
	if( !candidates.size() )
	{
		SZLOG( "No candidates passed the rule filter\n" );
		return false;
	}
	
#ifdef __SMARTZOOM_DEBUG
	SZLOG("\n Layers, zorder\n" );
	
	for( size_t i=0; i<candidates.size(); i++ )
	{
		RenderLayer* layer = candidates[i].ro->enclosingLayer();
		
		SZLOG(" ro=%08x  layer=%08x   isStackingContext=%d\n", 
				(unsigned int)candidates[i].ro, (unsigned int)layer, layer ? layer->isStackingContext() : -1 );
		/*
		if( layer )
		{
			layer->updateZOrderLists();
			Vector<RenderLayer*>* neg = layer->negZOrderList();
			SZLOG(" --- NEG:\n" );
			if( neg ) {
				for( int j=0; j<neg->size(); j++ ) {
					SZLOG( "      %08x\n", (*neg)[j] );
				}
			}
			Vector<RenderLayer*>* pos = layer->posZOrderList();		
			SZLOG(" --- POS:\n" );
			if( pos ) {
				for( int j=0; j<pos->size(); j++ ) {
					SZLOG( "      %08x\n", (*pos)[j] );
				}
			}
		} */
	}
	
	SZLOG("\n");
#endif	
	
	// Figure out the breaks between the "groups." Inevidably, the
	// rectangle Widths are clustered in groups. We'll compute the 
	// derivative of the width/layer-z-index line and look to see 
	// where the group boundaries lie.
	const float kDerivativeThreshold = 0.2f;
	
	for( size_t i=1; i< candidates.size(); i++ )
	{	
		long v = abs( candidates[i-1].ro->absoluteOutlineBounds().width() - candidates[i].ro->absoluteOutlineBounds().width() );
		float deriv = (float) v / ( float )candidates[i].ro->absoluteOutlineBounds().width() ;
		IntRect r = candidates[i].ro->absoluteOutlineBounds();
		if( deriv >= kDerivativeThreshold )
		{
			if( i >= 1 )
				widthsSet.push_back( candidates[i-1] );
			widthsSet.push_back( candidates[i] );
			SZLOG( "* [% 2d] %d,%d  %d x %d    score=%f center=%f\n", i,  r.x(), r.y(), r.width(), r.height(), deriv, candidates[i].centerScore );
		}
		else
		{
			SZLOG( "  [% 2d] %d,%d  %d x %d    score=%f center=%f\n", i,  r.x(), r.y(), r.width(), r.height(), deriv, candidates[i].centerScore );
		}
	}
	SZLOG("\n");
	
	if( !widthsSet.size() )
	{
		// No candidates were selected, but we should choose ONE just because there's one available.
		widthsSet.push_back( candidates[0] );
	}
	
	// Sort the widthSet based on SCALE -- we are going to search these based on 
	// the scale, so this will make the scale easier.
	std::sort(widthsSet.begin(), widthsSet.end(), SortByScale);
	
#ifdef __SMARTZOOM_DEBUG	
	// print the resulting width "winners" to the screen, sorted 
	SZLOG( " Current scale is %f\n", scale );
	for( size_t i=0; i<widthsSet.size(); i++ )
	{
		const SmartZoomState::LayerCandidate& c = widthsSet[i] ;
		ro = candidates[i].ro;
		int type=-1;
		if( ro->node() ) type = (int)ro->node()->nodeType();
		IntRect r = c.ro->absoluteOutlineBounds();
		SZLOG( "SORTED by scale [% 2d] RenderObject %08x type=%d : %d, %d  %d x %d  WIDTH=%d scale=%f center=%f \n", 
				i, (unsigned int)ro, type, r.x(), r.y(), r.width(), r.height(), r.width(), c.scale, c.centerScore );
	}
	SZLOG("\n");
#endif

	// We want to select the box that has the SMALLEST POS-SCALE DELTA
	// from our current scale.  If there are none, then we want to
	// find the SMALLEST NEG-SCALE DELTA from our current.

	// phase 1 finding smallest positive scale delta.....
	outWinningIndex = -1;
	
	IntRect clicked_rect;
	RenderObject* clicked_ro=0;
	
	if( widthsSet.size() > 1 )
	{
		// look at the layer z-order, and pick the "top" one.
		
		PlatformMouseEvent mouseEvent( IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), 
				IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), WebCore::LeftButton,
				WebCore::MouseEventPressed, 1, false, false, false, false, 
				WTF::currentTime() );
		
		MouseEventWithHitTestResults results =
			frame->document()->prepareMouseEvent(HitTestRequest(
					HitTestRequest::ReadOnly |
					HitTestRequest::Active |
					HitTestRequest::MouseMove |
					HitTestRequest::MouseUp),
				IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), mouseEvent);
		Node* n = results.hitTestResult().innerNonSharedNode();
		if( n )
		{
			clicked_ro = n->renderer();
			if( clicked_ro )
			{
				clicked_rect =  clicked_ro->absoluteOutlineBounds();
				SZLOG( "*-*-*-* CLICKED node: node=%08x layer=%08x enclosingLayer=%08x width=%d\n", 
							(unsigned int)n,
							(unsigned int)clicked_ro->enclosingLayer(), 
							(unsigned int)clicked_ro->enclosingLayer(), clicked_rect.width() );
				
				// add a special layer candidate for this.
				/*
				SmartZoomState::LayerCandidate winner;
				winner.ro = ro;
				winner.scale = float( ((float)win_width ) / r.width() );
				widthsSet.push_back(winner);
				outWinningIndex = widthsSet.size()-1;
				return true; */
			}
		}
		/*
		for( size_t i=0; i<widthsSet.size(); i++ )
		{
			IntRect r = widthsSet[i].ro->absoluteOutlineBounds();
			RenderLayer* layer = widthsSet[i].ro->enclosingLayer();
			if( ! layer )
			{
				SZLOG("[%d] layer=0\n",i );
				continue;
			}
			
			SZLOG( "[%d] enclosingLayer=%08x ", i, layer );
			
			layer->updateZOrderLists();
			
			if( layer->hasAutoZIndex() )
				SZLOG( "z-index= %d ", layer->zIndex() );
			
			if( layer->isStackingContext() )
				SZLOG( "stackingcontext= %08x ", layer->stackingContext() );
			
			SZLOG( " width=%d ", r.width() );
			
			SZLOG( " node=%08x ", widthsSet[i].ro->element() );
				
			SZLOG("\n");
		}
		*/
	}
	else
		outWinningIndex = 0;
	
	
	// -----------------------------------------------------------------------------------------------
	outWinningIndex = -1;
	float winningCenterScore = 0.0f;
	for( size_t i=0; i<widthsSet.size(); i++ )
	{
		//float proposed_new_scale = float( ((float)win_width ) / widthsSet[i].ro->absoluteOutlineBounds().width() );
		if( widthsSet[i].scale > scale ) 
		{
		//	float ndelta = scale - widthsSet[i].scale;
		//	printf( "ndelta = %f\n", ndelta );
		//	if( ndelta < delta )
			if( winningCenterScore < widthsSet[i].centerScore )
			{
				SZLOG( "index=%d winner %f  scale=%f\n", i, widthsSet[i].scale , scale );
				//delta = ndelta;
				outWinningIndex = i;
				winningCenterScore = widthsSet[i].centerScore;
				//break;
			}
		}
	}
	
	SZLOG("end phase 1 (winner=%d)\n",outWinningIndex);
	
	// phase 2 finding smallest negative delta....
	if( outWinningIndex == -1 )
	{
		// find the smallest next-largest delta.
		for( int i=widthsSet.size()-1; i>=0; i-- )
		{
			//float proposed_new_scale = float( ((float)win_width ) / widthsSet[i].ro->absoluteOutlineBounds().width() );
			if( widthsSet[i].scale > scale ) 
			{
				//float ndelta = widthsSet[i].scale - scale;
				//printf( "ndelta = %f\n", ndelta );
				//if( ndelta < delta )
				{
					//delta = ndelta;
					outWinningIndex = i;
				}
				break;
			}
		}		
	}
	// -----------------------------------------------------------------------------------------------
	
	
	SZLOG("end phase 2 (winner=%d)\n",outWinningIndex);
	
	if( outWinningIndex == -1 )
	{
		// just take the last one.
		outWinningIndex = widthsSet.size()-1;
	}
	
	SZLOG( "choosing item %d\n", outWinningIndex );

	return true;
}

// This algorithm generates a list of candidate boxes by using the 
// DOM event model event passing routine to collect the DOM nodes that
// would handle a mouse click event.
bool smartzoom_cluster_node( WebView* view, int in_mouse_x_abs_unscaled, int in_mouse_y_abs_unscaled, std::vector<SmartZoomState::LayerCandidate>& widthsSet, int& outWinningIndex )
{
	Frame* frame = core(view->page())->focusController()->focusedOrMainFrame();
	int win_width, win_height;
	view->client()->getWindowSize(win_width, win_height);		
	int contents_unscaled_width = frame->view()->contentsWidth();
	
	//RenderObject* ro = 0;
	float scale = (float) frame->view()->getScale();
	
	
	SZLOG( " ((( NODEs )))  \n" );
	
	// WebCore APIs expect the mouse clicks to be 1:1 and relative to 
	// current scroll offset in ScrollViewPalm.cpp. 
	
	int sx, sy;
	view->getContentPosition(sx, sy);
	sx = long( sx / scale) ;
	sy = long( sy / scale );
	int in_mouse_x_rel_unscaled = in_mouse_x_abs_unscaled - sx;
	int in_mouse_y_rel_unscaled = in_mouse_y_abs_unscaled - sy;
	
	
	int y_center_delta = (win_height/2) - in_mouse_y_rel_unscaled;
	
	
	SZLOG( "y_center_delta=%d  contents_unscaled_width=%d\n", y_center_delta, contents_unscaled_width );
	SZLOG( "webkit_page_smartzoom() mouse (1:1): %d, %d\n", in_mouse_x_rel_unscaled, in_mouse_y_rel_unscaled );
	


	// We want to select the box that has the SMALLEST POS-SCALE DELTA
	// from our current scale.  If there are none, then we want to
	// find the SMALLEST NEG-SCALE DELTA from our current.

	// phase 1 finding smallest positive scale delta.....
	outWinningIndex = -1;
	
	IntRect clicked_rect;
	//RenderObject* clicked_ro=0;
	
	
	// We're going to do event "bubbling." Starting with the node that gets the click
	// we're going to walk up the DOM chain until we hit the main page. That will
	// give us our collection of candidates.
	{
		PlatformMouseEvent mouseEvent( IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), 
				IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), WebCore::LeftButton,
				WebCore::MouseEventPressed, 1, false, false, false, false, 
				WTF::currentTime() );
		
		MouseEventWithHitTestResults mev =
			frame->document()->prepareMouseEvent(HitTestRequest(
					HitTestRequest::ReadOnly |
					HitTestRequest::Active |
					HitTestRequest::MouseMove),
				IntPoint(in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled), mouseEvent);
		
		if (!mev.targetNode()) {
		   SZLOG( "No node at clicked position.\n" );
		   return false;
		}

	   	Node* mousePressNode = mev.targetNode();
		
		Node* node = mousePressNode;
		RenderObject* renderer = node ? node->renderer() : 0;
	   
		// start with parent of this node.
		renderer = renderer->parent();
		int last_width = -1;
	   
		while (renderer) {
			node = renderer->node();
			
			if( node )
			{
				bool add_it = true;
				IntRect r =  renderer->absoluteOutlineBounds();
				
				if( last_width != renderer->absoluteOutlineBounds().width() )
					add_it = false;
				
				float box_width_score = fabs( r.width() - contents_unscaled_width )/float(contents_unscaled_width);
				if( box_width_score < 0.1f ) 
					add_it = false; 

				if( add_it )
				{
					SmartZoomState::LayerCandidate candidate;
					candidate.ro = node->renderer();
					IntRect r =  candidate.ro->absoluteOutlineBounds();
					candidate.scale = float( ((float)win_width ) / r.width() );
					widthsSet.push_back( candidate );
				}
				
				last_width = r.width();
			}
			
			renderer = renderer->parent();
		}
		
		for( size_t i=0; i<widthsSet.size(); i++ )
		{
			RenderObject* ro = widthsSet[i].ro;
			if( ro )
			{
				IntRect r =  ro->absoluteOutlineBounds();
				SZLOG( " **** CANDIDATE[%d] renderobject=%08x layer=%08x enclosingLayer=%08x width=%d\n", 
							i,
							(unsigned int)ro, 
							(unsigned int)ro->enclosingLayer(), 
							(unsigned int)ro->enclosingLayer(), r.width() );
			}
		}
		
		if( !widthsSet.size() )
			return false;
		
		outWinningIndex = 0;

		return true;
	}
	
	return false;
}

/**
 * @brief Walks down the tree, collecting render rectangles of leaves. 
 *        Clears vector of rects if <span> or non-text leaves are found,
 *        or tree is deeper than kMaxLineBoxTraversalDepth.
 *
 * @arg node parent node
 * @arg textRects a vector of text line rectangles
 * @arg depth 1-based depth counter
 * 
 */
static const int kMaxLineBoxTraversalDepth = 5;

bool addNestedLineBoxRects(Node* node, Vector<IntRect>& textRects, int depth)
{
    if (depth > kMaxLineBoxTraversalDepth) {
        return false;
    }
    
    if (!node || !node->renderer()) {
        return false;
    }

    // assume that spans may have variable height, style, alignment,
    // so clear out individual line rects because we want the parent node
    // to be highlighted entirely, and not just its text lines
    if (node->hasTagName(HTMLNames::spanTag)) {
        textRects.clear();
        return false;
    }
    
    // node is text leaf whose renderer is RenderText and will give us all the text rects
    if (node->renderer()->isText())  {
		RenderText* rt = static_cast<RenderText*>(node->renderer());
        rt->absoluteRectsForRange(textRects);
        return true;
    } 
    
    if (node->hasChildNodes()) {
        
        for (unsigned int n = 0; n < node->childNodeCount(); n++) 
        {
            Node* childNode = node->childNode(n);
            bool canContinue = addNestedLineBoxRects(childNode, textRects, depth + 1);
            
            // stop recursing if some descendant invalidated search
            if (!canContinue) {
                return false;
            }
        }
        
    } else {
        // non-text node is a leaf, such as <img>
        textRects.clear();
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------------------------------------


WebView::WebView(WebViewClient* client)
	: m_page(0)
	, m_client(client)
	, m_szState(new SmartZoomState())
	, m_interrogateClicks(false)
	, m_scrollbarsEnabled(false)
	, m_isTransparent(false)
	, m_lastMouseDownContentX(0)
	, m_lastMouseDownContentY(0)
	, m_usingDifferentMousePos(false)
	, m_showClickedLinkThisView(true)
	, m_isActive(true)
	, m_fallbackFonts(0)
	, m_selectingForCopy(false)
	, m_supportsAcceleratedCompositing(true)
	, m_nodeUnderMouseUp(0)
	, m_mouseMode(MouseModeSelect)
	, m_scrollingNode(NULL)
	, m_viewport()
	, m_dragInfo(new SpellingDragInfo())
    , m_data(new WebViewPrivate(this))
    , m_targetFrame(0)
    , m_usesSimulatedMouseClicks(false)
{
    m_page = new WebPage(this);

	// Make sure the main frame has focus
	core(m_page)->focusController()->setFocusedFrame(core(m_page->mainFrame()));
	core(m_page)->focusController()->setFocused(true);
	core(m_page)->focusController()->setActive(true);
	
    m_enableClickSearchRectangleExpansion = PalmBrowserSettings()->enableClickSearchRectangleExpansion;
	
    // fps counter for paints
    if(PalmBrowserSettings()->outputFpsEnable)
        m_fpsHandler = new PalmFpsHandler(PalmBrowserSettings()->outputFpsFileName);
    else
        m_fpsHandler = 0;

}

WebView::~WebView()
{
    delete m_page;
	delete m_szState;
	delete m_dragInfo;
	delete m_data;
    delete m_fpsHandler;
}

WebViewClient* WebView::client() const
{
    return m_client;
}

WebPage* WebView::page() const
{
    return m_page;
}

void WebView::resize(int width, int height)
{
	Frame* frame = core(m_page->mainFrame());
	int maxHeigh = frame->view()->getMaxVisibleHeightWhenScaled();
	if (maxHeigh)
		height = std::min(height, maxHeigh);

	frame->view()->resize(width, height);
	frame->view()->forceLayout();
	frame->view()->adjustViewSize();
}

void WebView::sendResizeEvent()
{
	Frame* frame = core(m_page->mainFrame());
	frame->eventHandler()->sendResizeEvent();
}

void WebView::getContentRect(int& contentX, int& contentY, int& width, int& height)
{
	Frame* frame = core(m_page->mainFrame());

    double scale = frame->view()->getScale();

    // As far as I understand scrollX()/scrollY() would always return 0 before anyway
    contentX   = 0; //(int) (scale * frame->view()->scrollX() + 0.5);
    contentY   = 0; //(int) (scale * frame->view()->scrollY() + 0.5);
	width      = (int) (scale * frame->view()->contentsWidth()  + 0.5);
	height     = (int) (scale * frame->view()->contentsHeight() + 0.5);
}

void WebView::getContentSize(int& width, int& height)
{
	Frame* frame = core(m_page->mainFrame());
	
    double scale = frame->view()->getScale();
    
	width      = (int) (scale * frame->view()->contentsWidth()  + 0.5);
	height     = (int) (scale * frame->view()->contentsHeight() + 0.5);    
}

void WebView::getContentPosition(int& contentX, int& contentY)
{
	Frame* frame = core(m_page->mainFrame());
	
   double scale = frame->view()->getScale();
    
    contentX   = (int) (scale * frame->view()->scrollX() + 0.5);
    contentY   = (int) (scale * frame->view()->scrollY() + 0.5);    
}

void WebView::setContentPosition(int x, int y, bool updateOffset)
{
	m_szState->sz_wasAutoPositioned = false;

	Frame* frame = core(m_page->mainFrame());

    double scale = frame->view()->getScale();

    x = (int) (x / scale + 0.5);
    y = (int) (y / scale + 0.5);

	frame->view()->setScrollPosition( IntPoint(x, y) , updateOffset);
    frame->view()->scrollPositionChangedViaPlatformWidget();
}

int WebView::getFontScale() const
{
	Frame* frame = core(m_page->mainFrame());
	return frame->view()->zoomFactor();    
}

void WebView::setFontScale(int zoom)
{
	if (zoom <= 0)
		return;

	Frame* frame = core(m_page->mainFrame());
	
	// disable auto-fixed-zoom if it's on
	frame->view()->setScale(frame->view()->getScale( ));
	
	frame->view()->setZoomFactor( (float)zoom, ZoomTextOnly);
}

void WebView::setMinFontSize(int minFontSizePt)
{
	Settings* settings = core(m_page)->settings();
	settings->setMinimumFontSize( minFontSizePt );
	settings->setMinimumLogicalFontSize( minFontSizePt );
	Frame* frame = core(m_page->mainFrame());
	frame->view()->forceLayout();
}


void WebView::setScale(double scale)
{
	m_szState->sz_wasAutoPositioned = false;

	Frame* frame = core(m_page->mainFrame());
	frame->view()->setScale(scale);    
}

void WebView::setScaleAndPosition(double scale, int x, int y)
{
	m_szState->sz_wasAutoPositioned = false;

	Frame* frame = core(m_page->mainFrame());

    x = (int) (x / scale + 0.5);
    y = (int) (y / scale + 0.5);
	
	frame->view()->setScaleAndScroll( scale, x, y );
}

double WebView::getScale() const
{
	Frame* frame = core(m_page->mainFrame());
	return frame->view()->getScale();
}

void WebView::fitWidth()
{
	m_szState->sz_wasAutoPositioned = false;

	Frame* frame = core(m_page->mainFrame());
    frame->view()->fitWidth();
}

void WebView::setMouseMode(MouseMode mode)
{
	m_mouseMode = mode;
}

bool WebView::selectionIsRange()
{
    Frame* frame = core(m_page->mainFrame());

    return (frame && frame->selection()->isRange());
}

void WebView::getTextCaretPos( int& left, int& top, int& right, int& bottom )
{
	Frame* frame = core(m_page->mainFrame());
	
	if( frame && isEditing() && !frame->selection()->isRange() )
	{
		IntRect r = frame->selection()->absoluteCaretBounds();
		left = r.x();		
		top = r.y();		
		right = r.right();		
		bottom = r.bottom();
	}
	else	
	{
		left = top = right = bottom = -1;
	}
}

void WebView::layoutIfNeeded()
{
	Frame* frame = core(m_page->mainFrame());

	if (frame->view())
        frame->view()->layoutIfNeededRecursive();
}

void WebView::setFallbackFonts( PGFallbackFonts* fonts )
{
	PGFallbackFonts::setThreadSpecificInstance( fonts );
}

/**
 * Paint the requested portion of the web page.
 *
 * @param context  The drawing context to use when painting this page.
 * @param contentX The left coord of the portion of the page to draw.
 * @param contentY The top coord of the portion of the page to draw.
 * @param width    The width of the portion of the page to draw.
 * @param height   The height of the portion of the page to draw.
 * @param recomposite If true then page is only recomposited.
 * @param mayScale If true then painting draws the page scaled.
 *
 * @return true if the context param was used to composite the page (else false).
 */
bool WebView::paint(NativeContext* context, int contentX, int contentY,
					int width, int height, bool recomposite, bool mayScale)
{
    SimpleStats::Event evt(WebKitStats::getPaintEventData());

#if 0   // Disabling this for now because it is causing junk to be drawn on the screen
    if (m_page->mainFrame()->isPrinting()) {
        g_debug("[WebView::paint] bypassing paint because we are printing!!!");
        return false;
    }
#endif
	Frame* frame = core(m_page->mainFrame());

    m_data->m_paintCount++;
    webOS::Reporter::Event event(frame->document(), "webkit.view.paint", "%d,%d (%dx%d), count:%d, recomposite:%c",
            contentX, contentY, width, height, m_data->m_paintCount, recomposite?'Y':'N');

    ScrollView* sv = static_cast<ScrollView*>(frame->view());

    SpellingWidgetController* spellingController = getSpellWidgetController();
    ClipboardController* clipboardController = getClipboardController();

    if (sv) {
		// update controller regardless of whether it is being drawn.
        if (spellingController)
            spellingController->setPageMetrics(IntPoint(sv->actualScrollOffset()), context->getScale(), sv->width(), sv->height());

        if (clipboardController)
            clipboardController->setPageMetrics(IntPoint(sv->actualScrollOffset()), context->getScale(), sv->width(), sv->height());
	}
	
    bool compositedWithGl(false);
    IntRect contentRect(contentX, contentY, width, height);

#if USE(ACCELERATED_COMPOSITING)
    if (m_data->isAcceleratedCompositingActive()) {
        m_data->doComposite(contentRect, !recomposite, mayScale);
        compositedWithGl = true;
    }
    //printf( "%s: webview %p compositedWithGl %d x %d y %d w %d h %d\n", __FUNCTION__, this, compositedWithGl, contentX, contentY, width, height);
#endif

    if (!compositedWithGl) {
        m_data->paintWithContext(context, contentRect, mayScale);

        GraphicsContext ctxt(context);
        if (spellingController)
            spellingController->paint(ctxt, contentX, contentY, context->getScale());

        if (clipboardController)
            clipboardController->paint(ctxt, contentX, contentY, context->getScale());
    }
	paintRemainingWindowSurface(context);

#if USE(ACCELERATED_COMPOSITING)
    // This will swap buffers when compositing.
    if (compositedWithGl && m_data->m_layerRenderer)
        m_data->m_layerRenderer->present();
#endif

    if (page()->d->m_logOnNextPaint && page()->view() == this) {
        uint32_t ms = (uint32_t) palm_monotonic_time_ms();

        g_debug("WEBKIT PERF: PAINT appid: %s time: %u\n",
            (page()->mainFrame()->client()) ? page()->mainFrame()->client()->getIdentifier() : "(nil)", ms);

        page()->d->m_logOnNextPaint = false;
    }

    // It counts the number of times this method is invoked. 
    // The counter can be accessed through the window.webkitPaintCount property.
    frame->domWindow()->webkitPainted();

    page()->d->chromeClient->paintComplete();

    if(m_fpsHandler)
        m_fpsHandler->framePainted(palm_monotonic_time_ms());

    return !compositedWithGl;
}

// When 320x480 apps are scaled 1.5x and running on 480x800 device we paint the
// remaining bottom stripe black: 480*1.5 = 720 which leaves 80px extra
// TODO:
// test with BS when it's hooked up
// Optimize: we should need to draw this only once, but it doesn't work
// like expected. Some page content still gets drawn at the bottom even after setting
// the clip rect to match the view size.
void WebView::paintRemainingWindowSurface(PGContextIface* context)
{
	Frame* frame = core(m_page->mainFrame());
	if (frame && frame->view()) {
		int maxHeight = frame->view()->getMaxVisibleHeightWhenScaled();
		if (!maxHeight)
			return;

		int viewHeight = frame->view()->height();
		if (maxHeight && maxHeight == viewHeight) {
			PGContext* contextPg = static_cast<PGContext*>(context);
			PGSurface* surfacePg = contextPg->getSurface();
			int winWidth = 0, winHeight = 0;
			if (surfacePg) {
				winHeight = surfacePg->height();
				winWidth = surfacePg->width();
			}
			else {
				m_client->getScreenSize(winWidth, winHeight);
			}

			if (maxHeight < winHeight) {
				context->push();
				contextPg->clearClip();
				contextPg->setGlobalClipRect(0, 0, winWidth, winHeight);
				GraphicsContext ctxt((PGContext*)context);
				ctxt.fillRect(FloatRect(0, viewHeight, winWidth, winHeight - viewHeight), Color::black, DeviceColorSpace);
				//printf("WebView::paintRemainingWindowSurface fillBlackRect (%d,%d  %d,%d)\n", 0, viewHeight, winWidth, winHeight - viewHeight);
				context->pop();
			}
		}
	}
}

void WebView::focus(bool enable)
{
	FocusController* fc = core(m_page)->focusController();
	if (!fc)
		return;

	if (enable) {

		Frame* frame = core(m_page->mainFrame());
		if (frame) {
			fc->setFocusedFrame(frame);
			fc->setFocused(true);
			fc->setActive(true);
		}
	}
	else {
		fc->setFocusedFrame(0);
		fc->setFocused(false);
		fc->setActive(false);
	}		
}

void WebView::animate()
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    Frame* frame = core(m_page->mainFrame());
    if (frame) {
        FrameView* fv = frame->view();
        if (fv)
            fv->serviceScriptedAnimations(convertSecondsToDOMTimeStamp(currentTime()));
    }
#endif
}

bool WebView::isEditing()
{
	return m_page->d->editorClient->isEditing();    
}

#if ENABLE(VIDEO)
static void pageFocusVideoOnFrame(Frame* frame, bool focus)
{
    RefPtr<NodeList> list = frame->document()->getElementsByTagName("video");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        if (list->item(i)->hasTagName(HTMLNames::videoTag)) {
            HTMLVideoElement* video = static_cast<HTMLVideoElement*>(list->item(i));
            MediaPlayer* player = video->player();
            if (player) player->pageFocus(focus);
        }
    }
}

static void pageFocusVideo(Frame* frame, bool focus)
{
    pageFocusVideoOnFrame(frame, focus);
    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
      pageFocusVideo(child, focus);
}
#endif

bool WebView::isViewActive()
{
	return m_isActive;
}

void WebView::setViewIsActive(bool isActive)
{
    Frame* frame = core(m_page)->mainFrame();

    m_isActive = isActive;
    frame->selection()->setCaretVisible(isActive);
    if (isActive && m_page->mainFrame()->client())
        WebGlobal::setActiveApplicationId(m_page->mainFrame()->client()->getIdentifier());

    // cancel/commit any pending IME composition
    if (!isActive)
        commitComposingText();

    if (isActive) {
        ITERATE_PLUGIN(PageGainFocus, frame, true);
#if ENABLE(VIDEO)
        pageFocusVideo(frame, isActive);
#endif
    }
    else {
        ITERATE_PLUGIN(PageLoseFocus, frame, true);
#if ENABLE(VIDEO)
        pageFocusVideo(frame, isActive);
#endif
    }
}

void WebView::setViewIsVisible(bool isVisible)
{
    page()->d->chromeClient->setVisible(isVisible);
}

bool WebView::keyEvent(unsigned short keyCode, unsigned short modifier, bool keyDown)
{
//    setViewIsActive();

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	
#if USE(V8)
    // Avoid oldgen GCs because we're doing interactive work right now:
    WebGlobal::avoidGC(WebGlobal::AVOID_OLDGEN);
#endif

	// translate the cursor keys: this is important for the Web Inspector.
	if( 0x0008 & modifier ) {

		switch( keyCode ) {
		case 0x0034:
			keyCode=VK_LEFT;
			break;
		case 0x0035:
			keyCode=VK_RIGHT;
			break;
		case 0x000b:
			keyCode=VK_UP;
			break;
		case 0x000c:
			keyCode=VK_DOWN;
			break;
		}
	}

    if (isEditing()) {
    	bool cursorMoveKey = (keyCode == VK_DOWN
    			|| keyCode == VK_UP
    			|| keyCode == VK_LEFT
    			|| keyCode == VK_RIGHT);

    	// TODO: replace magic constant with correct constant
    	// currently, sysmgr sends Event::Key_Shift, which is 0x80
    	bool selectingForCopyToggleKey = (keyCode == 0x80);

        WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );

    	// update sticky state only if we are about to enter a key that alters the text field
    	if (!cursorMoveKey && !selectingForCopyToggleKey && keyDown) {  // once per key, picking keyDown
    		editor->setStickyStateByCurrentSelection(false); // change only from Normal
    	}
    	editor->translateKeyWithStickyState(keyCode, modifier, keyDown);
    }

    return frame->eventHandler()->keyEvent(PlatformKeyboardEvent(keyCode, modifier, !keyDown));
}

bool WebView::clickWouldChangeNodeFocus( int x, int y )
{
	Frame* frame = core(m_page->mainFrame());
	double scale = frame->view()->getScale();
	long contentX = x;
	long contentY = y;

	if( scale != 1.0 ) {
		contentX = (long)(contentX / scale + 0.5);
		contentY = (long)(contentY / scale + 0.5);
	}
	
	// what is the current node?
	Node* focusedNode = frame->document()->focusedNode();
	
	// what would the new click be?
	PlatformMouseEvent mouseEvent( IntPoint(contentX,contentY), IntPoint(contentX,contentY), WebCore::LeftButton,
			WebCore::MouseEventPressed, 0, false, false, false, false, 
			WTF::currentTime() );
	MouseEventWithHitTestResults results =
		frame->document()->prepareMouseEvent(HitTestRequest(
			HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::MouseMove ), // mouseup
			IntPoint(contentX+frame->view()->scrollPosition().x(),
						contentY+frame->view()->scrollPosition().y()), mouseEvent);

	Node* target = results.targetNode();	

	if (!target) {
        return false;
	}
	
	if (target == focusedNode) {
	    return false;
	}
	
	Node* shadowParentNode = results.targetNode()->shadowParentNode();
	
	if( !shadowParentNode )
	{
		if( Node* p = results.targetNode()->parent() )
			shadowParentNode = p->shadowParentNode();
	}


	/*
	printf(" cur=%p/%s (parent is %p/%s)   new-would-be=%p/%s isShadowNode=%d isEditbleBlock=%d (parent %p/%s) (shadowParentNode= %p,%s) \n", 
		focusedNode, focusedNode ? focusedNode->nodeName().utf8().data() : "--",
		focusedNode->parent(), focusedNode->parent()->nodeName().utf8().data(),
			target, target->nodeName().utf8().data(), target->isShadowNode(), target->isEditableBlock(),
			target->parent(), ( target->parent() ? target->parent()->nodeName().utf8().data() : "" ),
			shadowParentNode, ""); */
	
	if( shadowParentNode == focusedNode )
		return false;
		
	if (focusedNode->isContentEditable() && target->isContentEditable()
	        && (focusedNode->rootEditableElement() == target->rootEditableElement())) {
	    // nodes are part of the same editable field
	    return false;
	}
	
	return true;
}

/**
 * Allows user to create a selection in document by enabling designMode, 
 * setting editableLinkBehavior to EditableLinkNeverLive, and then setting the 
 * state of WebPage's EditorClient to "selectingForCopy"
 * 
 */
bool WebView::setSelectionMode(bool on) 
{
	
    if (! PalmBrowserSettings()->runningInBrowserServer) {
        COPY_TRACE("Not running in BrowserServer\n");
        return false;
    }
    
    Frame* mainFrame = core(m_page->mainFrame());
    if (!mainFrame) {
        COPY_TRACE("no main frame\n");
        return false;
    }

    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );
    if (!editor) {
        COPY_TRACE("%s: no editor!\n", __FUNCTION__);
        return false;
    }

    Settings* settings = mainFrame->document()->settings();

    editor->selectingForCopy(on);
    
    if (on) {
        
        // only make document editable if we're not already in an editable text field
        // otherwise, we lose focus on text field when we setDesignMode(off)
        if (!isEditing()) {
            COPY_TRACE("Document is now editable\n");
            
            if (settings) {
                // disable clickable links
                settings->setEditableLinkBehavior(WebCore::EditableLinkNeverLive);
            }
            
            // document is editable
            mainFrame->document()->setDesignMode(WebCore::Document::on);

            // set state to block incoming keys
            m_selectingForCopy = true; 
            
            editor->setSelectionGranularityForDesignMode(true);
            
            // we're selecting from a web page, so no trackballing
            editor->setTrackballEnabled(false);

        } 
        
    } else if (m_selectingForCopy) {

        // leaving selection mode
        mainFrame->document()->setDesignMode(WebCore::Document::off);
        COPY_TRACE("Document is now NOT editable\n");
        
        VisibleSelection sel = mainFrame->selection()->selection();
        if (!sel.isNone() && sel.firstRange().get() != NULL) {
            m_page->d->editorClient->shouldEndEditing(sel.firstRange().get());
        }
        
        m_selectingForCopy = false;
        
        editor->setSelectionGranularityForDesignMode(false);
        
        editor->setTrackballEnabled(true);
        
        if (settings) {
            // enable clickable links
            settings->setEditableLinkBehavior(WebCore::EditableLinkDefaultBehavior);
        }
    }
    
    return true;
}

void WebView::setSupportsSingleAndLockStickyStates(bool enable)
{
    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );
    if (editor) {
        editor->enableSingleAndLockStickyStates(enable);
    }
}

/**
 * @brief if node at given x, y is interactive, creates a vector of node 
 *        renderer's rects. 
 * 
 */
void WebView::getInteractiveNodeRects(int mouseX, int mouseY, std::vector<Palm::WebRect>& nodeRects)
{
    // preferences can disable us
    if (! PalmBrowserSettings()->runningInBrowserServer
        || ! PalmBrowserSettings()->showClickedLink) {
        return;
    }
    
    Node* node = getInteractiveNodeInRectangle(mouseX, mouseY);

    if (!node) {
        //SZLOG("%s: Node is not interactive\n", __FUNCTION__);
        return;
    }

    RenderObject* renderer = node->renderer();
    if (!renderer) {
        return;
    }

    // vector used by RenderText to store text rects
    Vector<IntRect> lineBoxRects;

    // Anchor element doesn't just have a text leaf. May have text formatting tags
    // or non-text elements.

    if (node->isLink())
    {
        // add rects of descendant text nodes. 
        addNestedLineBoxRects(node, lineBoxRects, 1);


        if (lineBoxRects.size() == 0) {
            // use bounding box if no leaf text nodes are found or other
            // elements are mixed with text nodes
            lineBoxRects.append(node->renderer()->absoluteBoundingBoxRect());
        }
    } else {
        // A non-anchor element needs to be highlighted (input, select, plugin).
        // If it is editable, walk up to the top-most editable parent
        Node* editableArea = node;
        for (Node* n = node; n && n->isContentEditable(); n = n->parentNode()) {
            if (n->isElementNode())
                editableArea = n;
            if (n->hasTagName(HTMLNames::bodyTag))
                break;
        }
        lineBoxRects.append(editableArea->renderer()->absoluteBoundingBoxRect());
    }

    // copy Vector<IntRect> to vector<Palm::WebRect> used by client
    for (size_t i = 0; i < lineBoxRects.size(); ++i) 
    {
        IntRect iRect = lineBoxRects[i];
    
        Palm::WebRect wRect;
        wRect.left = iRect.x();
        wRect.top = iRect.y();
        wRect.bottom = iRect.bottom();
        wRect.right = iRect.right();
        
        nodeRects.push_back(wRect);
    }
}

static Frame* frameUnderMousePoint(int mouseX, int mouseY, Frame* mainFrame);
static bool hasMouseListener(Element* element)
{
    return element && (element->hasEventListeners(eventNames().clickEvent)
                       || element->hasEventListeners(eventNames().mousedownEvent)
                       || element->hasEventListeners(eventNames().mouseupEvent));
}

static bool isClickableElement(Element* element, RefPtr<NodeList> list)
{
    if (!element)
        return false;

    bool isClickable = hasMouseListener(element);
    if (!isClickable && list) {
        Element* parent = element->parentElement();
        unsigned count = list->length();
        for (unsigned i = 0; i < count && parent; i++) {
            if (list->item(i) != parent)
                continue;

            isClickable = hasMouseListener(parent);
            if (isClickable)
                break;

            parent = parent->parentElement();
        }
    }

    ExceptionCode ec = 0;
    return isClickable
        || element->webkitMatchesSelector("a,*:link,*:visited,*[role=button],button,input,select,label", ec)
        || computedStyle(element)->getPropertyValue(cssPropertyID("cursor")) == "pointer";
}

static bool isValidFrameOwner(Element* element)
{
    return element && element->isFrameOwnerElement() && static_cast<HTMLFrameOwnerElement*>(element)->contentFrame();
}

static Element* nodeToElement(Node* node)
{
    if (node && node->isElementNode())
        return static_cast<Element*>(node);
    return 0;
}

WebView::TouchAdjuster::TouchAdjuster(unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding)
    : m_topPadding(topPadding)
    , m_rightPadding(rightPadding)
    , m_bottomPadding(bottomPadding)
    , m_leftPadding(leftPadding)
{
}

IntPoint WebView::TouchAdjuster::findCandidatePointForTouch(const IntPoint& touchPoint, Document* document) const
{
    if (!document)
        return IntPoint();

    int x = touchPoint.x();
    int y = touchPoint.y();

    RefPtr<NodeList> intersectedNodes = document->nodesFromRect(x, y, m_topPadding, m_rightPadding, m_bottomPadding, m_leftPadding, false);
    if (!intersectedNodes)
        return IntPoint();

    Element* closestClickableElement = 0;
    IntRect largestIntersectionRect;
    Frame* frame = document->frame();
    if (!frame)
        return IntPoint();

    FrameView* view = frame->view();
    if (!view)
        return IntPoint();

    // Touch rect in contents coordinates.
    IntRect touchRect(HitTestResult::rectForPoint(view->windowToContents(IntPoint(x, y)), m_topPadding, m_rightPadding, m_bottomPadding, m_leftPadding));

    // Iterate over the list of nodes hit looking for the one whose bounding area
    // has largest intersection with the touch area (point + padding).
    for (unsigned i = 0; i < intersectedNodes->length(); i++) {
        Node* currentNode = intersectedNodes->item(i);

        Element* currentElement = nodeToElement(currentNode);
        if (!currentElement || (!isClickableElement(currentElement, 0) && !isValidFrameOwner(currentElement)))
            continue;

        IntRect currentElementBoundingRect = currentElement->getRect();
        currentElementBoundingRect.intersect(touchRect);

        if (currentElementBoundingRect.isEmpty())
            continue;

        int currentIntersectionRectArea = currentElementBoundingRect.width() * currentElementBoundingRect.height();
        int largestIntersectionRectArea = largestIntersectionRect.width() * largestIntersectionRect.height();
        if (currentIntersectionRectArea > largestIntersectionRectArea) {
            closestClickableElement = currentElement;
            largestIntersectionRect = currentElementBoundingRect;
        }
    }

    if (largestIntersectionRect.isEmpty())
        return IntPoint();

    // Handle the case when user taps a inner frame. It is done in three steps:
    // 1) Transform the original touch point to the inner document coordinates;
    // 2) Call nodesFromRect for the inner document in case;
    // 3) Re-add the inner frame offset (location) before passing the new clicking
    //    position to WebCore.
    if (closestClickableElement->isFrameOwnerElement()) {
        // Adjust client coordinates' origin to be top left of inner frame viewport.
        PassRefPtr<ClientRect> rect = closestClickableElement->getBoundingClientRect();
        IntPoint newTouchPoint = touchPoint;
        IntSize offset =  IntSize(rect->left(), rect->top());
        newTouchPoint -= offset;

        HTMLFrameOwnerElement* owner = static_cast<HTMLFrameOwnerElement*>(closestClickableElement);
        Document* childDocument = owner->contentDocument();
        return findCandidatePointForTouch(newTouchPoint, childDocument);
    }
    return view->contentsToWindow(largestIntersectionRect).center();
}
void WebView::adjustPointForClicking(WebCore::IntPoint& clickPoint)
{
    if (!m_enableClickSearchRectangleExpansion)
        return;

    unsigned topPadding = PalmBrowserSettings()->clickSearchRectangleTopPadding;
    unsigned rightPadding = PalmBrowserSettings()->clickSearchRectangleRightPadding;
    unsigned bottomPadding = PalmBrowserSettings()->clickSearchRectangleBottomPadding;
    unsigned leftPadding = PalmBrowserSettings()->clickSearchRectangleLeftPadding;

    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding)
        return;

    Document* startingDocument = core(m_page->mainFrame())->document();
    if (!startingDocument)
        return;

    TouchAdjuster touchAdjuster(topPadding, rightPadding, bottomPadding, leftPadding);
    IntPoint adjustedPoint = touchAdjuster.findCandidatePointForTouch(clickPoint, startingDocument);
    if (adjustedPoint == IntPoint::zero())
        return;

    clickPoint = adjustedPoint;
    return;
}

/**
 * @return Node ptr if an interactive node is found within click rectangle, NULL if none.
 *
 */
WebCore::Node* WebView::getInteractiveNodeInRectangle(int mouseX, int mouseY)
{
    IntPoint clickPoint(mouseX, mouseY);

    adjustPointForClicking(clickPoint);

    return getInteractiveNodeAtPoint(clickPoint.x(), clickPoint.y());
}


static Frame* frameUnderMousePoint(int mouseX, int mouseY, Frame* mainFrame)
{
	PlatformMouseEvent mouseEvent(IntPoint(mouseX, mouseY), IntPoint(mouseX, mouseY),
								  WebCore::LeftButton,
								  WebCore::MouseEventPressed, 1, false, false, false, false,
								  WTF::currentTime() );	

	MouseEventWithHitTestResults mev = mainFrame->document()->prepareMouseEvent(HitTestRequest(HitTestRequest::Active),
																				IntPoint(mouseX, mouseY),
																				mouseEvent);
	Node* node = mev.targetNode();
	if (!node)
		return mainFrame;

	RenderObject* renderer = node->renderer();
	if (!renderer || !renderer->isWidget())
		return mainFrame;

	Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
	if (!widget || !widget->isFrameView())
		return mainFrame;

	Frame* frame = static_cast<FrameView*>(widget)->frame();
	return frame == NULL ? mainFrame : frame;
}


WebCore::Node* WebView::getInteractiveNodeAtPoint(int mouseX, int mouseY)
{
    Frame* frame = frameUnderMousePoint(mouseX, mouseY, core(m_page->mainFrame()));
	if (!frame)
		return NULL;

    PlatformMouseEvent mouseEvent( IntPoint(mouseX,mouseY), 
            IntPoint(mouseX, mouseY), 
            WebCore::LeftButton,
            WebCore::MouseEventReleased, 
            0, 
            false, false, false, false,
            WTF::currentTime());
      
    MouseEventWithHitTestResults results =
        frame->document()->prepareMouseEvent(
                HitTestRequest(
                        HitTestRequest::ReadOnly | 
                        HitTestRequest::Active |
                        HitTestRequest::MouseMove), 
                        IntPoint(mouseX+frame->view()->scrollOffset().width(),
                                mouseY+frame->view()->scrollOffset().height()), 
                                mouseEvent);

    WebCore::Node* node = results.targetNode();

    if (!node) {
        return NULL;
    }

    // We consider the point 'interactive' if it or any of its parent nodes are editable.
    if (node->rootEditableElement() != NULL) {
        return node;
    }
    
    // If we clicked an element node, input, object, select are interactive
    if (node->nodeType() == Node::ELEMENT_NODE) 
    {
        Element *e = static_cast<Element*>(node);       
        String tagName = e->tagName();

        if (equalIgnoringCase(tagName, "input") ||
            equalIgnoringCase(tagName, "select"))
            return node;
    }

    
        
    // links are interactive
    Element* link = results.hitTestResult().URLElement();
    if (link) {
        // returning anchor node, since our true hitTarget could be a child
        // of the anchor node, which doesn't always give us all highlight rects
        return static_cast<Node*>(link);
    }
    
    return NULL;
}

// Collect all rects in a page containing flash content
void WebView::getFlashRects(std::vector<Palm::WebRect>& nodeRects)
{
    Frame* frame = core(m_page->mainFrame());
    if (!frame)
        return;
    Document* doc = frame->document();
    if (!doc)
        return;
    RenderObject *renderer = doc->renderer();
    if (!renderer)
        return;
    Node* n = renderer->node();

    while (n) {
        // look for object tag with child param tag having attributes name = movie, value = *.swf
        if (n->hasTagName(HTMLNames::paramTag)) {
            if (n->hasAttributes()) {
                NamedNodeMap* attr = n->attributes();
                RefPtr<Node> name = attr->getNamedItem(HTMLNames::nameAttr);
                RefPtr<Node> val = attr->getNamedItem(HTMLNames::valueAttr);

                if (name && val) {
                    if (equalIgnoringCase(name->nodeValue(), "movie") && val->nodeValue().endsWith(String(".swf", false))) {
                        Node* parent = n->parentNode();
                        if (parent && parent->hasTagName(HTMLNames::objectTag)) {
                            IntRect r = parent->getRect();
                            if (!r.isEmpty()) {
                                //g_debug("*** Found FLASH OBJECT: rect.x: %d, rect.y: %d, rect.width: %d, rect.height: %d\n",
                                //        r.x(), r.y(), r.width(), r.height());
                                Palm::WebRect wRect;
                                wRect.left = r.x();
                                wRect.top = r.y();
                                wRect.bottom = r.bottom();
                                wRect.right = r.right();
                                nodeRects.push_back(wRect);
                            }
                        }
                    }
                }
            }
        } else if (n->hasTagName(HTMLNames::embedTag)) {
            // look for embed tag with src attribute = *.swf
            if (n->hasAttributes()) {
                RefPtr<Node> srcAttr = n->attributes()->getNamedItem(HTMLNames::srcAttr);
                if (srcAttr) {
                    String val = srcAttr->nodeValue();
                    bool isSwf = val.endsWith(String(".swf"), false);
                    //g_debug("  FOUND SRC val: %s, isSwf: %d\n", val.ascii().data(), isSwf);
                    if (isSwf) {
                        IntRect r = n->getRect();
                        if (!r.isEmpty()) {
                            //g_debug("*** Found FLASH EMBED: rect.x: %d, rect.y: %d, rect.width: %d, rect.height: %d\n",
                            //        r.x(), r.y(), r.width(), r.height());
                            Palm::WebRect wRect;
                            wRect.left = r.x();
                            wRect.top = r.y();
                            wRect.bottom = r.bottom();
                            wRect.right = r.right();
                            nodeRects.push_back(wRect);
                        }
                    }
                }
            }
        }
        n = n->traverseNextNode();
    }
}

/**
 * Given a starting point (docX, docY) look for an "interractive" point near it.
 * If a point is found (even if it is the original input point) then (newDocX, newDocY)
 * will contain that interractive point. If no point is found then then (newDocX, newDocY)
 * will be set to the original input values.
 *
 * @return true if an interractive point was found, false if not.
 */
bool WebView::findInteractivePt(int docX, int docY, int& newDocX, int& newDocY)
{
    IntPoint clickPoint(docX, docY);

    adjustPointForClicking(clickPoint);

    newDocX = clickPoint.x();
    newDocY = clickPoint.y();

    if (getInteractiveNodeAtPoint(newDocX, newDocY))
        return true;

    return false;
}

// Came from Chromium
void WebView::selectWordAroundPosition(WebCore::Frame* frame, WebCore::VisiblePosition pos)
{
	WebCore::VisibleSelection selection;
	UChar32 charAfter = pos.characterAfter();
	UChar32 charBefore = pos.characterBefore();

	if (charBefore && charAfter &&
		(!Nova::Utils::isBreakChar(charBefore) && !Nova::Utils::isTerminalPunctuation(charBefore)) &&
		(Nova::Utils::isBreakChar(charAfter) || Nova::Utils::isTerminalPunctuation(charAfter))) {
		// Select the word to the left with bias - since expandUsingGranularity will always select
		// the word to the right if on boundary, even if that word is a break char.
		selection = pos.previous();
	} else {
		selection = pos;
	}

	selection.expandUsingGranularity(WebCore::WordGranularity);

	if (frame->shouldChangeSelection(selection)) {
		WebCore::TextGranularity granularity = selection.isRange() ? WebCore::WordGranularity : WebCore::CharacterGranularity;
		frame->selection()->setSelection(selection, false, true, false, WebCore::SelectionController::AlignCursorOnScrollIfNeeded, granularity);
	}
}

// Came from Chromium
bool WebView::selectWordAroundCaret(WebCore::Frame* frame)
{
	WebCore::SelectionController* controller = frame->selection();
    if (controller->isNone() || controller->isRange())
        return false;
    selectWordAroundPosition(frame, controller->selection().visibleStart());
    return true;
}

const IntRect WebView::wordBoundsAroundCaret(WebCore::Frame* frame) {
    IntRect r;
    if (!frame || !frame->document()->focusedNode()) {
        return r;
    }

    VisibleSelection prevSelection = frame->selection()->selection();

    if (!prevSelection.isCaret())
       return r;

    if (selectWordAroundCaret(frame)) {
        r = frame->contentRenderer()->selectionBounds();
        frame->selection()->setSelection(prevSelection);
    }

    return r;
}

/**
 * Replace the current selection with the specified word.
 */
void WebView::replaceSelection(WebCore::Frame* frame, const WTF::String* newWord)
{
	ASSERT(frame->selectedText().length());
	if (frame->editor()->shouldInsertText(*newWord, frame->selection()->toNormalizedRange().get(), EditorInsertActionPasted)) {
		Document* document = frame->document();
		RefPtr<ReplaceSelectionCommand> command = ReplaceSelectionCommand::create(document, createFragmentFromMarkup(document, *newWord, ""), true, false, true);
		applyCommand(command);
		frame->revealSelection(ScrollAlignment::alignToEdgeIfNeeded);
	}
}

/**
 * Select the word that contains the caret (if there is one) and replace it with the specified word.
 */
bool WebView::replaceWordAroundCaret(const WTF::String* newWord)
{
	bool replaced = false;

	WebCore::Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame && !frame->document()->focusedNode()) {
        return false;
    }
	
	SelectionController* sel = frame->selection();
	if (selectWordAroundCaret(frame)) {

		String replacedString = frame->selectedText();

		String markerDescription;
		IntRect selBounds = frame->contentRenderer()->selectionBounds();
		IntPoint selCenter(selBounds.x() + selBounds.width()/2, selBounds.y() + selBounds.height()/2);
		DocumentMarker* existingMarker = frame->document()->markers()->markerContainingPoint(selCenter, DocumentMarker::Replacement);
		if (existingMarker)
			markerDescription = existingMarker->description;

		if (replacedString.length() > 0) { // If able to select a word
			replaceSelection(frame, newWord);

			// If user replaced auto-correction with original misspelled word
			// make sure we don't auto-correct it again. Placing an empty marker will do the trick.
			if (*newWord == markerDescription) {
				// need to create selection around replacement to correctly create marker
				selectWordAroundCaret(frame);
				frame->document()->markers()->addMarker(frame->selection()->selection().toNormalizedRange().get(), DocumentMarker::Replacement, String());
			}

			// Set cursor position at the end of the replaced word.
			sel->setSelection(VisibleSelection(sel->end()));
			replaced = true;
		}
	}

	return replaced;
}

/**
 * Process the tap (called on mouse down) to see if it was on a mispelled word. If so then
 * display the spelling widget.
 *
 * @return true if the spelling widget was shown, false if not.
 */
bool WebView::processTapForSpellingWidget(const IntPoint& viewPt, bool isSimulatedMouseEvt)
{
    // If app uses simulated mouse events wait for the simulated event
    if (m_usesSimulatedMouseClicks && !isSimulatedMouseEvt)
        return false;

	SpellingWidgetController* spellingController = getSpellWidgetController();
	if (!spellingController) {
		return false;
	}
	
	bool displayedWidget(false);

	WebCore::Frame* focusedFrame = core(m_page)->focusController()->focusedOrMainFrame();
    WebKit::EditorClient* editorClient = static_cast<WebKit::EditorClient*>(focusedFrame->editor()->client());
	IntPoint cp(focusedFrame->view()->windowToContents(viewPt));
	HitTestResult result = focusedFrame->eventHandler()->hitTestResultAtPoint(cp, /*allowShadowContent*/ false);
	if (result.isContentEditable() &&
        (editorClient && editorClient->canAutoCorrectSpellingErrors())) {
		Node* insn = result.innerNonSharedNode();
		if (insn) {
			WebCore::Frame* nodeFrame = insn->document()->frame();
			if (nodeFrame) {
				SelectionController* sel = focusedFrame->selection();
				VisibleSelection prevSel = sel->selection();
				if (prevSel.isCaret()) {
					if (selectWordAroundCaret(focusedFrame)) {
						bool replacementIsOriginalWord = false;
						String lookupWord = nodeFrame->selectedText();
						String tappedWord = lookupWord;
						RenderView* rv = nodeFrame->contentRenderer();
						IntRect selBounds = rv->selectionBounds();
						IntPoint selCenter(selBounds.x() + selBounds.width()/2, selBounds.y() + selBounds.height()/2);

						// tapped word may be a programmatic replacement:
						// if word under tap was programmatically replaced, offer
						// original string as suggestion, in addition to other guesses
						// for original string (original string is provided in results)

						Vector<String> guesses;
						DocumentMarker* replacementMarker = focusedFrame->document()->markers()->markerContainingPoint(selCenter, DocumentMarker::Replacement);
						if (replacementMarker && !replacementMarker->description.isEmpty()) {
							replacementIsOriginalWord = true;
							guesses.append(replacementMarker->description);
						}
						else {
							editorClient->getGuessesForWord(lookupWord, guesses);
						}

						sel->setSelection(prevSel, false);
						const size_t numGuesses = guesses.size();
						if (numGuesses) {

							size_t i;
							spellingController->clearWords();

							if (replacementIsOriginalWord) {
								spellingController->widget()->appendWord(guesses[0], SpellingWidget::WT_Original);
							}
							else {
								for (i = 1; i < numGuesses; i++) {
									if (!equalIgnoringCase(tappedWord, guesses[i])) {
										spellingController->widget()->appendWord(guesses[i], SpellingWidget::WT_Guess);
									}
								}
								spellingController->widget()->appendWord(guesses[0], SpellingWidget::WT_AddToDictionary);
							}

							spellingController->setWordRect(selBounds);
							spellingController->scrollTo(0);
							spellingController->show();
							displayedWidget = true;
						}
					}
				}
			}
		}
	}

	return displayedWidget;
}

bool IsTap(const IntPoint& downPt, const IntPoint& upPt)
{
	const int maxDelta = 4;
	return abs(downPt.x() - upPt.x()) < maxDelta && abs(downPt.x() - upPt.x()) < maxDelta;
}

/**
 * Handle a mouse event.
 *
 * @param x The mouse X position in view coordinates (unscaled).
 * @param y The mouse Y position in view coordinates (unscaled).
 * @param clickCount The click count.
 * @param shiftKey Is the shift key down?
 * @param ctrlKey Is the control key down?
 * @param altKey Is the alt key down?
 * @param metaKey Is the meta key down?
 */
bool WebView::mouseEvent(Palm::MouseEventType type, int x, int y, int clickCount, bool shiftKey, bool ctrlKey, bool altKey, bool metaKey, bool isSimulated)
{
	Frame* mainFrame = core(m_page->mainFrame());
	int documentX = x;
	int documentY = y;

#if USE(V8)
    // Avoid oldgen GCs because we're doing interactive work right now:
    WebGlobal::avoidGC(WebGlobal::AVOID_OLDGEN);
#endif

	double scale = mainFrame->view()->getScale();
	if( scale != 1.0 ) {
		documentX = (int)(documentX / scale + 0.5);
		documentY = (int)(documentY / scale + 0.5);
	}

    Frame* frame;
	if (m_mouseMode == MouseModeScroll)
		frame = frameUnderMousePoint(documentX, documentY, mainFrame);
	else
		frame = mainFrame;
	if (!frame)
		return false;
	
	bool swallowEvent = false;

	if (type == MouseDown) {

		IntPoint expansionDelta(0, 0);
        if (m_mouseMode != MouseModeScroll && m_enableClickSearchRectangleExpansion) {
            findInteractivePt(documentX, documentY, m_lastMouseDownContentX, m_lastMouseDownContentY);

			expansionDelta.setX(m_lastMouseDownContentX - documentX);
			expansionDelta.setY(m_lastMouseDownContentY - documentY);
		}
		else {
			m_lastMouseDownContentX = documentX;
			m_lastMouseDownContentY = documentY;
		}

		m_usingDifferentMousePos = expansionDelta.x() != 0 || expansionDelta.y() != 0;

		if (m_usingDifferentMousePos) {
			documentX = m_lastMouseDownContentX;
			documentY = m_lastMouseDownContentY;
		}
	}

    if (type == MouseUp && m_usingDifferentMousePos && m_enableClickSearchRectangleExpansion) {
		// We may have moved the mousedown and the hyperlink following happens on a mouseup
		// so this ensures that the mouseup happens at the same exact place as the mousedown.
		documentX = m_lastMouseDownContentX;
		documentY = m_lastMouseDownContentY;
	}

    Palm::WordCompletionController* wordCompController = getWordCompletionController();
	IntPoint documentPt(documentX, documentY);
	FrameView* view = frame->view();
	IntPoint viewPt(documentPt);
	IntSize scrollPos(view->scrollOffset());
	documentPt.move( scrollPos.width(), scrollPos.height() );

	switch (type) {

	case MouseDown:
		{
            // handle clipboard/selection
		    Palm::ClipboardController* clipboardController = getClipboardController();
		    if (clipboardController) {
		        PlatformMouseEvent mouseEvent(viewPt, documentPt, WebCore::LeftButton, WebCore::MouseEventPressed,
		                clickCount, shiftKey, ctrlKey, altKey, metaKey, WTF::currentTime());
                swallowEvent = clipboardController->handleMouseDownEvent(mouseEvent, isSimulated);
		    }

            // handle word completion
            if (!swallowEvent && wordCompController)
                swallowEvent = wordCompController->handleMouseDownEvent(viewPt);

            // handle text assist
            if (!swallowEvent) {
                SpellingWidgetController* spellingController = getSpellWidgetController();
                if (spellingController && spellingController->isVisible()) {
                    const WebCore::SpellingWidget::Word* word = spellingController->pointToWord(documentPt);
                    if (word)
                        swallowEvent = true;
                }
                m_dragInfo->setMouseDownInfo(true, swallowEvent);
                m_dragInfo->setCurrPt(documentPt);
                m_dragInfo->flickStart(documentPt.x());
            }

            // let the document handle the event
            if (!swallowEvent) {
                m_scrollingNode = NULL;
                frame->eventHandler()->setScrollingNode(m_mouseMode == MouseModeScroll);

                PlatformMouseEvent mouseEvent( viewPt, documentPt, WebCore::LeftButton,
                        WebCore::MouseEventPressed, clickCount, shiftKey, ctrlKey, altKey, metaKey,
                        WTF::currentTime() );
                swallowEvent = frame->eventHandler()->handleMousePressEvent(mouseEvent);
                if (m_mouseMode == MouseModeScroll) {
                    MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(HitTestRequest(HitTestRequest::Active | HitTestRequest::ReadOnly),
                                                                                        documentPt, mouseEvent);
                    m_scrollingNode = mev.targetNode();
                }

                m_targetFrame = kit(frameUnderMousePoint(documentX, documentY, frame));
            }
            else {
                swallowEvent = true;
                m_targetFrame = 0;
            }
		}
		break;
		
	case MouseMove:
		{
            // handle clipboard/selection
		    Palm::ClipboardController* clipboardController = getClipboardController();
            if (clipboardController) {
                PlatformMouseEvent mouseEvent(viewPt, documentPt, WebCore::LeftButton, WebCore::MouseEventMoved,
                        0, shiftKey, ctrlKey, altKey, metaKey, WTF::currentTime());
                swallowEvent = clipboardController->handleMouseMoveEvent(mouseEvent);
            }

            // handle word completion
            if (!swallowEvent && wordCompController)
                swallowEvent = wordCompController->handleMouseMoveEvent();

            // handle text assist
            if (!swallowEvent) {
                SpellingWidgetController* spellingController = getSpellWidgetController();
                if (m_dragInfo->isMouseDown() && m_dragInfo->isDownOnWidget()) {
                    m_dragInfo->setCurrPt(documentPt);
                    if (spellingController) {
                        spellingController->scrollBy(-m_dragInfo->moveX());
                        spellingController->invalidate();
                    }
                    swallowEvent = true;
                }
                else if (spellingController && spellingController->isVisible())
                    spellingController->hide();
            }

            // let the document handle the event
            if (!swallowEvent) {
                IntSize delta = viewPt - frame->eventHandler()->currentMousePosition();

                PlatformMouseEvent mouseEvent( viewPt, documentPt, WebCore::LeftButton,
                        WebCore::MouseEventMoved, 0, shiftKey, ctrlKey, altKey, metaKey,
                        WTF::currentTime() );
                HitTestResult hoveredNode = HitTestResult(IntPoint());

                swallowEvent = frame->eventHandler()->handleMouseMoveEvent(mouseEvent, &hoveredNode);

                if (m_mouseMode == MouseModeScroll && frame->eventHandler()->mousePressed() && m_scrollingNode) {
                    RenderBox* enclosingBox = m_scrollingNode->renderer()->enclosingBox();
                    if (enclosingBox) {
                        enclosingBox->scroll(delta.width() < 0 ? ScrollRight: ScrollLeft, ScrollByPixel, abs(delta.width()));
                        enclosingBox->scroll(delta.height() < 0 ? ScrollDown: ScrollUp, ScrollByPixel, abs(delta.height()));
                    }
                }
            }
		}
		break;

	case MouseUp:
		{
            // handle clipboard/selection
		    Palm::ClipboardController* clipboardController = getClipboardController();
            if (clipboardController) {
                PlatformMouseEvent mouseEvent(viewPt, documentPt, WebCore::LeftButton, WebCore::MouseEventReleased,
                        0, shiftKey, ctrlKey, altKey, metaKey, WTF::currentTime() );
                swallowEvent = clipboardController->handleMouseUpEvent(mouseEvent, isSimulated);
            }

            // handle word completion
            if (!swallowEvent && wordCompController)
                swallowEvent = wordCompController->handleMouseUpEvent();


            if (!swallowEvent) {
                SpellingWidgetController* spellingController = getSpellWidgetController();

                if (m_dragInfo->isMouseDown() && m_dragInfo->isDownOnWidget()) {
                    m_dragInfo->flickEnd(documentPt.x());
                    spellingController->flick(m_dragInfo->getFlickSpeed());
                }

                bool clipboardVisible = false;
                if (clipboardController)
                    clipboardVisible = clipboardController->clipboardIsVisible();

                bool isTap = IsTap(IntPoint(m_lastMouseDownContentX, m_lastMouseDownContentY), IntPoint(documentX, documentY));
                if (spellingController && spellingController->isVisible()) {
                    if (!m_dragInfo->didDrag()) {
                        const WebCore::SpellingWidget::Word* word = spellingController->pointToWord(documentPt);
                        if (!word) {
                            g_debug("Didn't click on a guess");
                            spellingController->hide();

                            // but still may have clicked on another (or even the same) word so check for spelling widget.
                            if (isTap && !clipboardVisible)
                                processTapForSpellingWidget(documentPt, isSimulated);
                        }
                        else {
                            g_debug("Clicked on spelling guess '%s'", word->getString().utf8().data());

                            if (spellingController->isFlicking())
                                spellingController->stopFlick();
                            else {
                                // if user cliced on '+' add word to the dictionary
                                if (word->type == WebCore::SpellingWidget::WT_AddToDictionary) {
                                    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>(core(m_page)->editorClient());
                                    if (editor)
                                        editor->learnWord(word->getString());
                                }
                                replaceWordAroundCaret( &(word->getString()));
                                spellingController->hide();
                            }
                        }
                    }
                }
                else {
                    if (isTap && !clipboardVisible)
                        processTapForSpellingWidget(documentPt, isSimulated);
                }

                if (!m_dragInfo->isDownOnWidget()) {
                    m_scrollingNode = NULL;
                    PlatformMouseEvent mouseEvent( viewPt, documentPt, WebCore::LeftButton,
                            WebCore::MouseEventReleased, 0, shiftKey, ctrlKey, altKey, metaKey,
                            WTF::currentTime() );

                    swallowEvent = frame->eventHandler()->handleMouseReleaseEvent(mouseEvent);

                    MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(HitTestRequest(HitTestRequest::Active | HitTestRequest::ReadOnly),
                                                                                            documentPt, mouseEvent);

                    m_nodeUnderMouseUp = mev.targetNode();

                    frame->eventHandler()->setScrollingNode(false);
                }

                m_dragInfo->setMouseDownInfo(false);
                m_dragInfo->flickClear();
            }
		}
		break;
	}
	
	return swallowEvent;
}

bool WebView::orientationChangeEvent(int position, float pitch, float roll, uint32_t time)
{
    Frame* frame = core(m_page->mainFrame());
    
    bool handled = frame->eventHandler()->dispatchOrientationChangeEvent(
            position, pitch, roll,
            time);
    
    return handled;
}

bool WebView::shakeEvent(int shakeState, float shakeMagnitude, uint32_t time)
{
	AtomicString shakeEventName;
    
    switch (shakeState)
    {
    case ShakeStart:
        shakeEventName = eventNames().shakestartEvent;
        break;

    case Shaking:
        shakeEventName = eventNames().shakingEvent;
        break;

    case ShakeEnd:
        shakeEventName = eventNames().shakeendEvent;
        break;
        
    default:
        return false;
    }
 
    Frame* frame = core(m_page->mainFrame());
    
    bool handled = frame->eventHandler()->dispatchShakeEvent(shakeEventName, shakeMagnitude, time);
    
    return handled;
}

bool WebView::compassEvent(float magHeading, float trueHeading, float accuracy, uint32_t time)
{
    Frame* frame = core(m_page->mainFrame());
    
    bool handled = frame->eventHandler()->dispatchCompassEvent(
            magHeading, trueHeading, accuracy, time);
    
    return handled;
}

bool WebView::accelerationEvent(float accelX, float accelY, float accelZ, uint32_t time)
{
    Frame* frame = core(m_page->mainFrame());
    
    bool handled = frame->eventHandler()->dispatchAccelerationEvent(
            accelX, accelY, accelZ,
            time);
    
    return handled;
}


bool WebView::gestureEvent(GestureEventType type, int fx, int fy, float rotation, float scale,
							int centerX, int centerY, bool shiftKey, bool ctrlKey, bool altKey, bool metaKey)
{
//    setViewIsActive();

	Frame* frame = core(m_page->mainFrame());
	bool swallowEvent = false;

#if USE(V8)
    // Avoid oldgen GCs because we're doing interactive work right now:
    WebGlobal::avoidGC(WebGlobal::AVOID_OLDGEN);
#endif

    if (!PalmBrowserSettings()->runningInBrowserServer
        && targetFrame() && core(targetFrame())
        && core(targetFrame())->document()) {
        frame = core(targetFrame());
    }

    SimpleStats::Event statsEvt(WebKitStats::getGestureEventData());

	switch( type )
	{
	case GestureStart:
		{
            webOS::Reporter::Event event(frame->document(), "webkit.gesture.start");
			hideSpellingWidget();
			swallowEvent = frame->eventHandler()->handleGestureEvent(type, fx, fy, rotation, scale,
								centerX, centerY, shiftKey, ctrlKey, altKey, metaKey);
		}
		break;
	case GestureChange:
		{
            webOS::Reporter::Event event(frame->document(), "webkit.gesture.change");
			swallowEvent = frame->eventHandler()->handleGestureEvent(type, fx, fy, rotation, scale,
								centerX, centerY, shiftKey, ctrlKey, altKey, metaKey);
		}
		break;
	case GestureEnd:
		{
            webOS::Reporter::Event event(frame->document(), "webkit.gesture.end");
			swallowEvent = frame->eventHandler()->handleGestureEvent(type, fx, fy, rotation, scale,
							centerX, centerY, shiftKey, ctrlKey, altKey, metaKey);
		}
		break;
	case GestureSingleTap:
		{
			PlatformMouseEvent mouseEvent( IntPoint(centerX,centerY), IntPoint(centerX,centerY),
										   WebCore::LeftButton,
										   WebCore::MouseEventReleased, 0, shiftKey, ctrlKey, altKey, metaKey, 
										   WTF::currentTime() );

            webOS::Reporter::Event event(frame->document(), "webkit.gesture.singleTap");
			MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(HitTestRequest(HitTestRequest::ReadOnly | HitTestRequest::Active),
																					IntPoint(centerX, centerY),
																					mouseEvent);
			if (!m_nodeUnderMouseUp || m_nodeUnderMouseUp == mev.targetNode()) {

				swallowEvent = frame->eventHandler()->handleGestureEvent(type, fx, fy, rotation, scale,
									centerX, centerY, shiftKey, ctrlKey, altKey, metaKey);
			}
			
			m_nodeUnderMouseUp = 0;
		}
		break;
	}
	return swallowEvent;

}


bool WebView::touchEvent(Palm::TouchEventType type, const TouchPointPalm* touches, unsigned touchesLen,
						bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
#if ENABLE(TOUCH_EVENTS)
//    setViewIsActive();

	Frame* frame = core(m_page->mainFrame());

	// touch events will be stopped at sysmgr/brserver level
	// if there are no registered listeners

	WebCore::TouchEventType evtType;
	switch (type) {
		case Palm::TouchStart:
			evtType = WebCore::TouchStart;
			break;
		case Palm::TouchMove:
			evtType = WebCore::TouchMove;
			break;
		case Palm::TouchEnd:
			evtType = WebCore::TouchEnd;
			break;
		case Palm::TouchCancel:
			evtType = WebCore::TouchCancel;
			break;
		default:
			return false;
	}

	PlatformTouchEvent evt(evtType, ctrlKey, altKey, shiftKey, metaKey);

	// add touches to the event
	unsigned index = 0;
	PlatformTouchPoint::State touchState;

	while (index < touchesLen)
	{
		switch (touches[index].state) {
			case TouchPointPalm::TouchReleased:
				touchState = PlatformTouchPoint::TouchReleased;
				break;
			case TouchPointPalm::TouchPressed:
				touchState = PlatformTouchPoint::TouchPressed;
				break;
			case TouchPointPalm::TouchMoved:
				touchState = PlatformTouchPoint::TouchMoved;
				break;
			case TouchPointPalm::TouchStationary:
				touchState = PlatformTouchPoint::TouchStationary;
				break;
			case TouchPointPalm::TouchCancelled:
				touchState = PlatformTouchPoint::TouchCancelled;
				break;
			default:
				return false;
		}

		IntPoint intPt(touches[index].x, touches[index].y);
		evt.addTouchPoint(touches[index].id, intPt, touchState);
		index++;
	}

	return frame->eventHandler()->handleTouchEvent(evt);
#else
	notImplemented();
	return false;
#endif
}

bool WebView::mouseHoldEvent(int x, int y)
{
    Palm::ClipboardController* clipboardController = getClipboardController();
    if (clipboardController)
        return clipboardController->handleMouseHoldEvent(x, y);

    return false;
}


// This is the automatic smart zoom function. It returns a new scale, scroll offset
// based on what it thinks is the "best candidate" for a snap-to/scaled zoom.
bool WebView::smartZoomAuto(int mouseX, int mouseY)
{
	Frame* frame = core(m_page->mainFrame());
	int win_width, win_height;
	m_client->getWindowSize(win_width, win_height);		
	
	long outScrollX, outScrollY;
	double scale = frame->view()->getScale();
	int in_mouse_x_abs_unscaled = (long)(mouseX / scale);
	int in_mouse_y_abs_unscaled = (long)(mouseY / scale);
	
	int winner ;
	std::vector<SmartZoomState::LayerCandidate> cluster;
	
	
	SZLOG( "\n ** ** ** SMARTZOOM ** ** ** \n" );
	
	// If we are already auto-positined, we need to fit-width this page.  We want
	// to adjust the scroll-y offset to reflect where the user clicked, (within reason)
	if( m_szState->sz_wasAutoPositioned )
	{
		// TODO : Calculate what the fitPageWidth is.
		
		// for now, we'll just call fitWidth and return false so the
		// caller will ignore the parameters.
		
		SZLOG( "RESTORE from smart-zoom position. \n" );
		
		m_szState->sz_wasAutoPositioned = false;
	
		int sx, sy;
		getContentPosition(sx, sy);
		
		frame->view()->fitWidth();
		
		float new_scale = frame->view()->getScale();
		SZLOG( "  in_mouse_y_abs_unscaled=%d  win_height=%d old_scale, new_scale=%f\n",in_mouse_y_abs_unscaled, win_height, new_scale );
		
		// Adjust for window height*new_scale factor -- we want the "tap" to be as
		// close to the center of the window as possible.
		sy = in_mouse_y_abs_unscaled - (unsigned int)( float( win_height / 2 ) / new_scale ); 
		
		frame->view()->setScrollPosition( IntPoint(0, sy) );	// These are in 1:1 coords.
		
		SZLOG( "################################# Smartzoom: restoring fit width\n" );
		
		return true;
	}
	
	
	if( !smartzoom_cluster(this, in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled, cluster, winner ) )
	{
		// If there were no winners, just zoom in 2x.
		double outScale = frame->view()->getScale() * 2.0;
		frame->view()->setScale( outScale );
		outScrollX = long( (in_mouse_x_abs_unscaled * outScale)  /*- ( *outScale * (win_height/8) ) */ );
		outScrollY = long( (in_mouse_y_abs_unscaled * outScale)  /*- ( *outScale * (win_height/8) ) */ );
		outScrollX -= win_width/2;
		outScrollY -= win_height/2;
		//frame->view()->setContentsPos( long( outScrollX  ), long( outScrollY  ) );	// 1:1 coords.
		/*
		// No need to relayout or send resize when zooming
		frame->forceLayout();
		frame->view()->adjustViewSize();
		frame->sendResizeEvent();
	*/
		m_szState->sz_wasAutoPositioned = true;
		return true;
	}
		
	double outScale = float(   ((float)win_width ) / cluster[winner].ro->absoluteOutlineBounds().width() );
	
	// add some slop
	outScale *= 0.8f;
	
	// now compute the new scale (already done above), and scroll offset based on this "winner"
	const IntRect nodeRect = cluster[winner].ro->absoluteOutlineBounds();

	// Set the return values. The scroll offsets are in SCALED page-absolute 	
	outScale = double(   ((double)win_width ) / nodeRect.width() );
	outScrollX = long( nodeRect.x() * (outScale) );
	outScrollY = long( (in_mouse_y_abs_unscaled * outScale)  /*- ( *outScale * (win_height/8) ) */ );
	
	// we want to center the mouse click in the 
	outScrollY -= win_height/2;
	
	const float kSnapMarginPercent = 0.8f;
	
	// Snap box to top : if the resulting scroll offset of the box is "close" to the top of the box, just 
	// scroll to the top of the box.
	float a = fabs( ( outScrollY -  ((outScale)*float(nodeRect.y()))  )  / ((outScale)*float(nodeRect.y())) );
	SZLOG( " SNAP TO TOP %f\n", a );
	if( a > kSnapMarginPercent )
	{
		//outScrollY = long( nodeRect.y() * (outScale) );
		SZLOG( " (should) --> Snapping to TOP\n" );
	}
	
	
	// Snap box to bottom : if the resulting scroll offset of the box is "close" to the bottom of the box, just 
	// scroll to the top of the box.
	// TODO

	SZLOG( "POSITION DEBUGGING: in_mouse_unscaled=%d  in_mouse_scaled=%d  box.top(1:1)=%d  winheight=%d new_scale=%.2f  ==> sy=%d (scaled)   sy=%d (1:1)\n",
			in_mouse_y_abs_unscaled, 
			int((float)in_mouse_y_abs_unscaled*scale), 
			nodeRect.y(), 
			win_height, outScale,
			(int)outScrollY,
			int( outScrollY / outScale) );

	// Set the scroll/scale here so we can preserve the value of wasAutoPositioned.
	//frame->view()->setScaleAndScroll(outScale, (int) (outScrollX / outScale), (int)(outScrollY / outScale) );	// 1:1 coords.
	
	/*
	// No need to relayout or send resize when zooming
	frame->forceLayout();
    frame->view()->adjustViewSize();
    frame->sendResizeEvent();
	*/
	m_szState->sz_wasAutoPositioned = true;
	
	return true;
}


struct AutoZoomCandidateObject {

	AutoZoomCandidateObject(RenderBox* b, const IntRect& r)
		: box(b), rect(r) {
	}
	
	//RenderObject* obj;
	RenderBox* box;
	IntRect rect;
};

typedef std::list<AutoZoomCandidateObject> AutoZoomCandidateObjectList;

static void collectRenderBoxesForAutoZoom(RenderBox* box,
											 AutoZoomCandidateObjectList& objList,
											 int contentX, int contentY)
{
	if (!box)
		return;

	IntRect rect = box->absoluteOutlineBounds();
	if (rect.contains(contentX, contentY)) {

		// Based on experimentation we need to look at:
		// RenderBlock, RenderTable, RenderTableCell, RenderImage, RenderWidget
		if (box->isRenderBlock() ||
			box->isTableCell() ||
			box->isTable() ||
			box->isWidget() ||
			(box->isImage() && !box->isListMarker())) {

			AutoZoomCandidateObject candidate(box, rect);
			objList.push_back(candidate);
			//printf("sz: %s  %d:%d, %d:%d\n", obj->renderName(), rect.x(), rect.right(), rect.y(), rect.bottom());
		}
	}

	RenderObject* o = box->firstChild();
	while (o) {
		if( o->isBox() )
			collectRenderBoxesForAutoZoom( static_cast<RenderBox*>(o), objList, contentX, contentY);
		o = o->nextSibling();
	}
}

static void collectRenderLayersForAutoZoom(RenderLayer* layer, RenderLayer* rootLayer,
											AutoZoomCandidateObjectList& objList,
											int contentX, int contentY)											
{
	if (!layer)
		return;

	IntRect rect = layer->boundingBox(rootLayer);
	if (!rect.contains(contentX, contentY))
		return;
	
	if( layer->renderer()->isBox() ) {
		
		collectRenderBoxesForAutoZoom( static_cast<RenderBox*>( layer->renderer() ), objList,
									contentX, contentY);
	}
	
	RenderLayer* l = layer->firstChild();
	while (l) {
	    collectRenderLayersForAutoZoom(l, rootLayer, objList,
										contentX, contentY);
		l = l->nextSibling();
	}
}

bool WebView::smartZoomCalculate(int mouseX, int mouseY, WebRect& rect, int* lockAreaHandle)
{
	Frame* mainFrame = core(m_page->mainFrame());
	if (!mainFrame)
		return false;
	
    Frame* frame = frameUnderMousePoint(mouseX, mouseY, mainFrame);
	if (!frame)
		frame = mainFrame;
	
	IntPoint pt(mouseX, mouseY);
	pt = frame->view()->windowToContents(pt);  // x, y need to be wrt the window

	AutoZoomCandidateObjectList candidateList;
	collectRenderLayersForAutoZoom(frame->document()->renderer()->enclosingLayer(),
									frame->document()->renderer()->enclosingLayer(),
									candidateList, pt.x(), pt.y());

	if (candidateList.empty())
		return false;

	int deviceWidth, deviceHeight;
	m_client->getScreenSize(deviceWidth, deviceHeight);

	int contentWidth, contentHeight;
	contentWidth = mainFrame->view()->contentsWidth();
	contentHeight = mainFrame->view()->contentsHeight();

	// Walk the list in reverse order. because the way we collected the items, children
	// tend to be lower than parents

	// First we identify the smallest object width-wise in the list. 
	int minWidth = contentWidth;
	AutoZoomCandidateObjectList::const_reverse_iterator startAt = candidateList.rend();
	for (AutoZoomCandidateObjectList::const_reverse_iterator it = candidateList.rbegin();
		 it != candidateList.rend(); ++it) {

		const AutoZoomCandidateObject& o = (*it);
		const IntRect& r = o.rect;
		
		// Too wide: Threshold is 90% of content width
		if ((r.width() * 100) / contentWidth > 90)
			continue;
			
		// Too small: Threshold is 10% of device width
		if ((r.width() * 100) / deviceWidth < 10)
			continue;
		
		if (r.width() < minWidth) {
			minWidth = r.width();
			startAt = it;
		}
	}

	// Failed?
	if (startAt == candidateList.rend()) {

		rect.left = 0;
		rect.top = 0;
		rect.right = contentWidth;
		rect.bottom = contentHeight;	
		*lockAreaHandle = 0;
		return true;
	}

	// check plugin first
	for (AutoZoomCandidateObjectList::const_reverse_iterator it = startAt;
		 it != candidateList.rend(); ++it) {

		const AutoZoomCandidateObject& o = (*it);
		if (!o.box->isWidget()) continue;
		Node* element = o.box->node();
		if (element == 0) continue;
		if (!element->hasTagName(HTMLNames::appletTag) && !element->hasTagName(HTMLNames::embedTag) && !element->hasTagName(HTMLNames::objectTag))
			continue;
		IntRect r = o.rect;
	    IntPoint p = frame->view()->contentsToWindow(r.location());
	    r.setLocation(p);
		rect.left   = r.x();
		rect.top    = r.y();
		rect.right  = r.right();
		rect.bottom = r.bottom();
		*lockAreaHandle = (int)( static_cast<RenderWidget*>(o.box)->widget());
		return true;		
	}
	*lockAreaHandle = 0;

	// Now we climb up from the previously identified object looking for enclosing closely
	// packed rectangles

	bool firstItem = true;
	IntRect candidateRect;
	for (AutoZoomCandidateObjectList::const_reverse_iterator it = startAt;
		 it != candidateList.rend(); ++it) {

		const AutoZoomCandidateObject& o = (*it);
		const IntRect& r = o.rect;

		//printf("checking: %s  %d:%d, %d:%d\n", o.obj->renderName(), r.x(), r.right(), r.y(), r.bottom());

		if (firstItem) {
			firstItem = false;
			candidateRect = r;
			continue;
		}
		else {
			if (r.contains(candidateRect)) {
				if ((r.width() * 100) / candidateRect.width() <= 110) {
					candidateRect = r;
					continue;
				}
			}
		}

	    IntPoint p = frame->view()->contentsToWindow(candidateRect.location());
	    candidateRect.setLocation(p);
		rect.left   = candidateRect.x();
		rect.top    = candidateRect.y();
		rect.right  = candidateRect.right();
		rect.bottom = candidateRect.bottom();

		return true;		
	}

	rect.left = 0;
	rect.top = 0;
	rect.right = contentWidth;
	rect.bottom = contentHeight;
	
	return true;
}

std::vector<WebRect> WebView::smartZoomCalculate(int mouseX, int mouseY)
{
	Frame* frame = core(m_page->mainFrame());

	int win_width, win_height;
	m_client->getWindowSize(win_width, win_height);		
	
	//long outScrollX, outScrollY;
	double scale = frame->view()->getScale();
	int in_mouse_x_abs_unscaled = (long)(mouseX / scale);
	int in_mouse_y_abs_unscaled = (long)(mouseY / scale);

	std::vector<WebRect> result;
	
	int winner ;
	std::vector<SmartZoomState::LayerCandidate> cluster;
	if( !smartzoom_cluster( this, in_mouse_x_abs_unscaled,in_mouse_y_abs_unscaled, cluster, winner ) )
		return result;
		
	if( cluster.size() )
	{
		result.resize(cluster.size());
		
		for( size_t i=0; i<cluster.size(); i++ )
		{
			const IntRect& r = cluster[i].ro->absoluteOutlineBounds();
			result[i].left = long( r.x() );
			result[i].top = long( r.y()  );
			result[i].right = long( r.right() );
			result[i].bottom = long( r.bottom() );
		}
	}

	return result;
}

void WebView::smartZoomReset()
{
    m_szState->sz_wasAutoPositioned = false;
}
void WebView::setViewportMeta(bool canScale, double scaleValue, int pageWidth, int pageHeight)
{
	Frame* frame = core(m_page->mainFrame());
	FrameView* fv = frame->view();
	
	fv->setUserCanScale( canScale ? true : false );
	fv->setInitialScale( scaleValue );
	
	if( pageWidth > 0 )
		fv->setFixVirtualWidth( pageWidth );

	if( pageHeight > 0 )
		fv->setFixVirtualHeight( pageHeight );

	frame->view()->forceLayout();
}

void WebView::setInterrogateClicks(bool enable)
{
	m_interrogateClicks = enable;
}

bool WebView::getInterrogateClicks() const
{
	return m_interrogateClicks;    
}

// From Page.cpp
static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

int WebView::findString(const char* str, bool forward, bool caseSensitive, bool shouldWrap, bool startWithSelection)
{
	Page* page = core(m_page);

	//bool startWithSelection = ( inStartWithSelection ) ? true : false;
	FindDirection direction = ( forward ) ? FindDirectionForward : FindDirectionBackward;
	
	
	if( !page->mainFrame() )
		return 0;		
	
	// Count the total number of "hits"
	unsigned hits = 0;
    Frame* frame = page->focusController()->focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
		
		hits += frame->countAllMatchesForText( String(str), caseSensitive );
        frame = incrementFrame(frame, direction, shouldWrap);
    } while (frame && frame != startFrame);
	
	if( !hits )
		return 0;
	

	bool found =false;
    frame = page->focusController()->focusedOrMainFrame();
    startFrame = frame;
    do {		
		if( frame->findString( String(str), direction == FindDirectionForward, caseSensitive, false /*wrapFlag*/, true /*startWithSelection*/ )) {
            if (frame != startFrame)
                startFrame->selection()->clear();
			page->focusController()->setFocusedFrame(frame);
			found=true;
			break;
		}
        frame = incrementFrame(frame, direction, shouldWrap);
    } while (frame && frame != startFrame);

	ASSERT(frame);
	
	if( !found )
	{
		if( shouldWrap && !startFrame->selection()->isNone()) {
			found = startFrame->findString( String(str), direction == FindDirectionForward, caseSensitive, true, true);
			page->focusController()->setFocusedFrame(frame);
		}
	}
	
	if( found && frame )
	{
		const double kFindResultFontSizeScaled = 19.0;
		//double scale = frame->view()->getScale();
		//const Selection& sel = frame->selection()->selection();
		Position p = frame->selection()->start();
		Node* n = p.node();
		if( n )
		{
			RenderStyle* rs = n->renderStyle();			
            IntRect r = enclosingIntRect(frame->selectionBounds(false));
			
			// This is the size we want the target text to be after we 
			// apply the new scale factor.
            int new_offset_x = r.x();
            int new_offset_y = r.y();
			
			//printf( "new_offset_x= %d\n",new_offset_x ); 
			//printf( "frameview = %d\n", frame->view()->x() );
			new_offset_x += frame->view()->x();
			new_offset_y += frame->view()->y();
			
			double new_scale = kFindResultFontSizeScaled / rs->fontSize() ;
			if( m_client)
			{
				int win_width, win_height;
				m_client->getWindowSize(win_width, win_height);
				
				new_offset_x -= ( win_width/4 );
				new_offset_y -= ( win_height/4 );
				
				// if the selection box is less than the width of the window,
				// let's center it in the window.
				if( r.width() < win_width )
                    new_offset_x += int( new_scale * double( r.width()/2 ) );
				
			}
			new_offset_x= max(0,(int)new_offset_x);
			new_offset_y= max(0,(int)new_offset_y);
			
			// TODO : frame->view()->setScaleAndScroll( new_scale, new_offset_x, new_offset_y );
			
			page->mainFrame()->view()->setScale( new_scale );
            page->mainFrame()->view()->setScrollPosition( IntPoint( new_offset_x, new_offset_y ) );

			/*
			// No need to relayout or send resize when zooming
			page->mainFrame()->forceLayout();
			page->mainFrame()->view()->adjustViewSize();
			*/
		}
		return hits;		
	}
	

	return hits;
}

void WebView::selectAll()
{

    Frame* frame = core(m_page)->mainFrame();
    if (!frame) {
        return;
    }
  
    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );
    
    if (!editor) {
        return;
    }
    
    editor->disallowEditorEvents();
    frame->selection()->selectAll();
    editor->allowEditorEvents();
    
}

void WebView::clearSelection()
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    Frame* startFrame = frame;
    do {
		frame->selection()->clear();
        frame = incrementFrame(frame, FindDirectionForward, true);
    } while (frame && frame != startFrame);
}

bool WebView::privateInteractiveAtPoint(int x, int y)
{
	Frame* frame = core(m_page->mainFrame());
	PlatformMouseEvent mouseEvent( IntPoint(x,y), 
			IntPoint(x,1), WebCore::LeftButton,
			WebCore::MouseEventPressed, 1, false, false, false, false, 
			WTF::currentTime() );
	MouseEventWithHitTestResults results =
			frame->document()->prepareMouseEvent(HitTestRequest(
				HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::MouseMove | HitTestRequest::MouseUp
), // mouseup
			IntPoint(x+frame->view()->scrollPosition().x(),y+frame->view()->scrollPosition().y()), mouseEvent);
			
	Node* node = results.targetNode();
	if( node ) 
	{
		// We consider the point 'interactive' if it or any of its parent nodes are editable.
		if( node->rootEditableElement() != NULL ) 
			return true;
		
		// Alternatively, if we clicked an element node, the following elements are considered interactive:
		if( node->nodeType() == Node::ELEMENT_NODE ) 
		{
			Element *e = static_cast<Element*>(node);		
			String tagName = e->tagName();  
			
			if (equalIgnoringCase(tagName, "input") ||
				equalIgnoringCase(tagName, "select"))
			{
				return true;
			}
		} 

		// Finally, links are interactive:
		Element* link = results.hitTestResult().URLElement();
		if( link )					
			return true;
	}
	
	// ... Otherwise, not interactive!
	return false;
}


bool WebView::isInteractiveAtPoint(int mouseX, int mouseY )
{
	Frame* frame = core(m_page->mainFrame());
	double scale = frame->view()->getScale();
	
	long contentX = (long)(mouseX / scale + 0.5);
	long contentY = (long)(mouseY / scale + 0.5);
	return privateInteractiveAtPoint( contentX, contentY );
}

bool WebView::isClickableAtPoint(int mouseX, int mouseY)
{
    Frame* frame = core(m_page->mainFrame());
    double scale = frame->view()->getScale();

    long contentX = (long)(mouseX / scale + 0.5);
    long contentY = (long)(mouseY / scale + 0.5);

    PlatformMouseEvent mouseEvent( IntPoint(contentX, contentY),
            IntPoint(contentX,1), WebCore::LeftButton,
            WebCore::MouseEventPressed, 1, false, false, false, false,
            WTF::currentTime() );
    MouseEventWithHitTestResults results = frame->document()->prepareMouseEvent(
            HitTestRequest(HitTestRequest::ReadOnly |
                           HitTestRequest::Active |
                           HitTestRequest::MouseMove |
                           HitTestRequest::MouseUp),
            IntPoint(contentX+frame->view()->scrollPosition().x(),contentY+frame->view()->scrollPosition().y()),
            mouseEvent );

    Node* node = results.targetNode();
    if (node) {
        if (results.hitTestResult().isContentEditable() || results.hitTestResult().isLiveLink())
            return true;

        for (Node* p = node; p; p = p->parentNode()) {
            if (p->isElementNode() && p->hasEventListeners(eventNames().clickEvent))
                return true;
            if (p->hasTagName(HTMLNames::bodyTag))
                break;
        }
    }
    return false;
}

void WebView::setTransparent(bool enable)
{
	m_isTransparent = enable;
	core(m_page->mainFrame())->view()->setTransparent(enable);
}

bool WebView::trySendKeyPressForClipboardOperation( unsigned short keyCode )
{
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	Node* focusedNode = frame->document()->focusedNode();
	
	// Check to see what node is focused. If a plug-in has focus, then 
	// send a key event to it, rather than execute the copy here.
	if( focusedNode )
	{
		if( focusedNode->isHTMLElement() )
		{
			HTMLElement* el = static_cast<HTMLElement*>(focusedNode);
			if( el )
			{
				if( el->tagName() == "OBJECT" || el->tagName() == "EMBED" )
				{
					// TODO : do we want a white list for the object tags? 
					// Send a key event to this object.
					frame->eventHandler()->keyEvent( PlatformKeyboardEvent( keyCode, 0x0020, false ) );
					frame->eventHandler()->keyEvent( PlatformKeyboardEvent( keyCode, 0x0020, true ) );
					return true;
				}
			}
		}
	}
	return false;
} 

void WebView::cut()
{
	if( trySendKeyPressForClipboardOperation( Key_x ) )
		return;
	
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	frame->editor()->command("Cut").execute();
}

bool WebView::copy()
{
	if( trySendKeyPressForClipboardOperation( Key_c ) )
		return false;
	
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	frame->editor()->command("Copy").execute();

	bool success = selectionIsRange();
	if (success) {
	    client()->copiedToClipboard();
	}

	return success;
}

void WebView::paste()
{
	if( trySendKeyPressForClipboardOperation( Key_v ) )
		return;
	
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	frame->editor()->command("Paste").execute();
}

void WebView::setComposingText(const char* text)
{
	if (!isEditing())
		return;

	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (!frame)
		return;

	Editor* editor = frame->editor();
	if (!editor)
		return;

	String s = String::fromUTF8(text);
	Vector<CompositionUnderline> underline;
	unsigned start = s.length(), end = start;
	//Text* oldComposingText = editor->compositionNode();

	//underline.append(CompositionUnderline(0, end, Color::black, true));
	//fprintf(stdout, "%s: changing composing text from '%s' to '%s'\n", 
	//		__PRETTY_FUNCTION__, oldComposingText ? oldComposingText->wholeText().utf8().data() : "", text);

	editor->setComposition(s, underline, start, end);
}

void WebView::commitComposingText()
{
	Frame* frame = core(m_page)->mainFrame();
	if (!frame)
		return;

	Editor* editor = frame->editor();
	if (!editor)
		return;

	if (editor->hasComposition()) {
		editor->confirmComposition();
	}
}

void WebView::performEditorAction(int fieldAction)
{
    if (!isEditing())
        return;

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    Node* focused = frame->document()->focusedNode();
    if (!focused)
        return;
	
    PalmIME::FieldAction action = static_cast<PalmIME::FieldAction>(fieldAction);
    switch (action) {
    case PalmIME::FieldAction_Next: {

            g_debug("%s: performing action 'Next'", __PRETTY_FUNCTION__);
            // we check if that the next node is valid to prevent auto focus wrapping that is done
            // in FocusController::advanceFocus
            frame->document()->updateLayoutIgnorePendingStylesheets();

            core(m_page)->focusController()->advanceFocus(FocusDirectionForward, 0);
        }
        break;
    case PalmIME::FieldAction_Previous: {

            g_debug("%s: performing action 'Previous'", __PRETTY_FUNCTION__);
            // we check if that the previous node is valid to prevent auto focus wrapping that is done
            // in FocusController::advanceFocus
            frame->document()->updateLayoutIgnorePendingStylesheets();

            core(m_page)->focusController()->advanceFocus(FocusDirectionBackward, 0);
        }
        break;
    default: 
        g_debug("%s: ignoring unsupported action 0x%02x", __PRETTY_FUNCTION__, action);
        break;
    }
}

void WebView::removeInputFocus()
{
	Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
	if (frame) {

        Node* focusedNode = frame->document()->focusedNode();
        if (focusedNode && focusedNode->isElementNode()) {
            // try to take focus away from the focused element in the document
            Element* focusedElement = static_cast<Element*>(focusedNode);
            if (focusedElement && isEditing()) {
                focusedElement->blur();
                return;
            }
        }
    }

    // we assume that the caller is expecting a blur response so fake it
    if (m_client) {
    	PalmIME::EditorState editorState;
        m_client->editorFocused(false, editorState);
    }
}

void WebView::setTextCaret(TextCaretType type)
{
	switch (type) {
	case (TextCaretNormal):
		break;
	case (TextCaretShift):
		break;
	case (TextCaretShiftLocked):
		break;
	case (TextCaretAlt):
		break;
	case (TextCaretAltLocked):
		break;
	}
}

void WebView::dragStart(PalmClipboard*, int x, int y)
{

}

void WebView::dragMove(PalmClipboard* clipboard, int x, int y)
{
	// The mac port calls both of these (webkit4)
	PlatformMouseEvent mouseEvent( IntPoint(x,y), 
			IntPoint(x,y), WebCore::LeftButton,
			WebCore::MouseEventMoved, 0, false, false, false, false, 
			WTF::currentTime() );
	
	// GREG TODO core(m_page)->mainFrame()->eventHandler()->dragSourceMovedTo( mouseEvent );

	DragController* dc = core(m_page)->dragController();
	DragData dragData( clipboard, IntPoint(x,y), IntPoint(x,y), DragOperationEvery );
	dc->dragUpdated(&dragData);
}

void WebView::dragEnter(PalmClipboard* clipboard, int x, int y)
{
	DragController* dc = core(m_page)->dragController();	
	DragData dragData( clipboard, IntPoint(x,y), IntPoint(x,y), DragOperationEvery );
	dc->dragEntered(&dragData);
}

void WebView::dragLeave(PalmClipboard* clipboard, int x, int y)
{
	DragController* dc = core(m_page)->dragController();	
	DragData dragData( clipboard, IntPoint(x,y), IntPoint(x,y), DragOperationEvery );
	dc->dragExited(&dragData);    
}

bool WebView::dragEnd(PalmClipboard* clipboard, int x, int y)
{
	DragController* dc = core(m_page)->dragController();
	DragData dragData( clipboard, IntPoint(x,y), IntPoint(x,y), DragOperationEvery );
	bool r= dc->performDrag(&dragData);
	
	PlatformMouseEvent mouseEvent( IntPoint(x,y), IntPoint(x,y), WebCore::LeftButton,
			WebCore::MouseEventReleased, 0, false, false, false, false, 
			WTF::currentTime() );
	
	core(m_page)->mainFrame()->eventHandler()->dragSourceEndedAt( mouseEvent, DragOperationMove );
	
	return r;
}

void WebView::enableScrollbars(bool enable)
{
	Frame* frame = core(m_page->mainFrame());
	if (!frame->view())
		return;
	
	//??? frame->view()->setScrollbarsMode( enable ? ScrollbarAuto : ScrollbarAlwaysOff );

	m_scrollbarsEnabled = enable;    
}

bool WebView::scrollbarsEnabled() const
{
	return m_scrollbarsEnabled;
}


void WebView::openInspectorAtPoint(int mouseX, int mouseY)
{
// FIXME    
}

void WebView::inspectAtPoint(int mouseX, int mouseY)
{
// FIXME    
}
bool WebView::privateGetElementInfoAtPoint(int x, int y, ElementInfo& info)
{
    Frame* frame = frameUnderMousePoint(x, y, core(m_page->mainFrame()));
	if (!frame)
		return false;

	// Convert from document to Frame coords (for iframes).
	FrameView* view = frame->view();
	IntPoint viewPt = view->windowToContents(IntPoint(x,y));

	Element* e = frame->document()->elementFromPoint(viewPt.x(), viewPt.y());
	if ( e ) {

		info.element= e->tagName().utf8().data();
		info.id 	= e->getAttribute(HTMLNames::idAttr).string().utf8().data();
		info.name   = e->getAttribute(HTMLNames::nameAttr).string().utf8().data();
		info.cname  = e->getAttribute(HTMLNames::classAttr).string().utf8().data();
		info.type   = e->getAttribute(HTMLNames::typeAttr).string().utf8().data();
		info.x = x;
		info.y = y;

		RenderObject* r = e->renderer();
		if (r != NULL) {
			IntRect renderBox = view->contentsToWindow(r->absoluteBoundingBoxRect());
			info.bounds.left = renderBox.x();
			info.bounds.top = renderBox.y();
			info.bounds.right = renderBox.right();
			info.bounds.bottom = renderBox.bottom();
		}
		else {
			info.bounds.left = info.bounds.top = info.bounds.right = info.bounds.bottom = 0;
		}

		// Also do a hit test to see if the node has a root editable element.
		IntPoint documentPt = viewPt;
		IntSize scrollPos(view->scrollOffset());
		documentPt.move( scrollPos.width(), scrollPos.height() );

		PlatformMouseEvent mouseEvent( viewPt, documentPt, WebCore::LeftButton,
				WebCore::MouseEventPressed, 0, false/*shift*/, false/*ctrl*/, false/*alt*/, false/*meta*/,
				WTF::currentTime() );
		HitTestRequest request(
				HitTestRequest::ReadOnly |
				HitTestRequest::Active |
				HitTestRequest::MouseMove);
		MouseEventWithHitTestResults results = frame->document()->prepareMouseEvent(request, documentPt, mouseEvent);
		Node* node = results.targetNode();
		if (NULL != node) {
			info.isEditable = node->rootEditableElement() != NULL;
		}
		else {
			info.isEditable = false;
		}

		return true;
	}
	else {
		return false;
	}
}

/**
 * Return information about the element (if there is one) at a point. Will attempt
 * to find an editable element to zoom in on by expanding the click rectangle.
 *
 * @return true if successfully found an element, false on error or no element.
 */
bool WebView::getElementInfoAtPoint(int x, int y, ElementInfo& info)
{
    int newX = x, newY = y;

    (void) findInteractivePt(x, y, newX, newY);

    return privateGetElementInfoAtPoint(newX, newY, info);
}

/**
 * Return information about an image (if there is one) at a point.
 *
 * @return true if successfully found an image, false on error or no image.
 */
bool WebView::getImageInfoAtPoint(int mouseX, int mouseY, ImageInfo& info)
{
	Frame* frame = core(m_page->mainFrame());
	if (!frame)
		return false;

	bool gotImage(false);

	PlatformMouseEvent mouseEvent( IntPoint(mouseX,mouseY),
			IntPoint(mouseX,1), WebCore::LeftButton,
			WebCore::MouseEventPressed, 1, false, false, false, false,
			WTF::currentTime() );
	MouseEventWithHitTestResults results =
			frame->document()->prepareMouseEvent(HitTestRequest(
					HitTestRequest::ReadOnly | 
					HitTestRequest::Active |
					HitTestRequest::MouseMove |
					HitTestRequest::MouseUp),
			IntPoint(mouseX+frame->view()->scrollOffset().width(),mouseY+frame->view()->scrollOffset().height()), mouseEvent);
			
	Node* node = results.targetNode();
	if( node && node->nodeType() == Node::ELEMENT_NODE )
	{
		Element *e = static_cast<Element*>(node);
		String tagName = e->tagName();
		
		if( equalIgnoringCase(tagName, "img") )
		{
			HTMLImageElement* img = static_cast<HTMLImageElement*>(node);
			info.baseUri = img->baseURI().string().utf8().data();
			if (isImageSourceData(img)) {
				info.src = "data:";
			}
			else {
				info.src = img->src().string().utf8().data();
			}
			info.title   = img->title().utf8().data();
			info.altText = img->altText().utf8().data();
			info.width   = img->naturalWidth();
			info.height  = img->naturalHeight();
			gotImage = true;

			CachedImage* cachedImg = img->cachedImage();
			if (cachedImg != NULL) {
				info.mimeType = cachedImg->response().mimeType().utf8().data();
			}
		}
	}

	return gotImage;
}

/**
 * Modify the supplied file name (if necessary) such that it will be
 * uniquely named and won't collide with another file in the same dir.
 * @note The directory must exist for this file.
 */
bool WebView::makeFileNameUnique(std::string& fname)
{
	if (fname.empty()) {
		return false;
	}
	if (!fileExists(fname.c_str())) {
		return true;
	}

	std::string base;
	std::string suffix;

	std::string::size_type last_dot = fname.rfind('.');
	if (last_dot == std::string::npos) { // If no extension
		base = fname;
	}
	else {
		base   = std::string(fname, 0, last_dot);
		suffix = std::string(fname, last_dot);
	}

	for (int i = 1; i <= 200; i++) {
		std::stringstream testPath;
		testPath << base << "(" << i << ")" << suffix;
		if (!fileExists(testPath.str().c_str())) {
			fname = testPath.str();
			return true;
		}
	}

	return false;
}

/**
 * Is the source of the image element data - i.e. "data:foo"
 * @see http://en.wikipedia.org/wiki/Data_URI_scheme
 */
bool WebView::isImageSourceData(const HTMLImageElement* img)
{
	return img->src().string().startsWith("data:", true /*caseless*/);
}

// If there is an image at this location, save it out as a JPG and return the
// name we saved it out as.
bool WebView::downloadImageAtPoint( int scaledX, int scaledY, std::string& outFileName )
{
	Frame* frame = core(m_page->mainFrame());
	if (!frame)
		return false;

	bool succeeded(false);

	PlatformMouseEvent mouseEvent( IntPoint(scaledX,scaledY), 
			IntPoint(scaledX,1), WebCore::LeftButton,
			WebCore::MouseEventPressed, 1, false, false, false, false, 
			WTF::currentTime() );
	MouseEventWithHitTestResults results =
			frame->document()->prepareMouseEvent(HitTestRequest(
					HitTestRequest::ReadOnly |
					HitTestRequest::Active |
					HitTestRequest::MouseMove |
					HitTestRequest::MouseUp),
			IntPoint(scaledX+frame->view()->scrollOffset().width(),scaledY+frame->view()->scrollOffset().height()), mouseEvent);
			
	Node* node = results.targetNode();
	if( node && node->nodeType() == Node::ELEMENT_NODE ) 
	{
		Element *e = static_cast<Element*>(node);
		String tagName = e->tagName();  
		
		if( equalIgnoringCase(tagName, "img") )
		{
			HTMLImageElement* img = static_cast<HTMLImageElement*>(node);

			CachedImage* cachedImg = img->cachedImage();
			if (cachedImg != NULL) {
				Image* image = cachedImg->image();
				if (image != NULL) {
					RefPtr<SharedBuffer> sharedBuffer = image->data();
					if (sharedBuffer != NULL && sharedBuffer->size() > 0) {
						makeAllDirectories( String( outFileName.c_str() ) );
						outFileName += "/";
						if (isImageSourceData(img)) {
							outFileName += "data";
						}
						else {
							outFileName += basename(img->src().string().utf8().data());
						}
						if (makeFileNameUnique(outFileName)) {
							FILE* pFile = fopen(outFileName.c_str(), "w");
							if (pFile != NULL) {
								size_t written = fwrite(sharedBuffer->data(), 1, sharedBuffer->size(), pFile);
								fclose(pFile);
								succeeded = written = sharedBuffer->size();
							}
							else {
								g_warning("ERROR %d opening \"%s\"", errno, outFileName.c_str());
							}
						}
					}
					else {
						g_warning("Can't get image shared data");
					}
				}
			}
		}
	}

	return succeeded;
}


/**
 * See if there is a hyperlink at the specified point and if so return information about it.
 *
 * @param scaledX The X coordinate.
 * @param scaledY The Y coordinate.
 * @param outUrl The URL of the hyperlink. Will be NULL if no link is found. Caller must free using ::free().
 * @param outDesc The descriptiion of the hyperlink. Will be NULL if no link is found. Caller must free using ::free().
 * @param outRect The bounding box of the hyperlink.
 *
 * @return true if there is a hyperlink at the specified coordinate, false if not.
 */
bool WebView::inspectUrlAtPoint(int scaledX, int scaledY, char*& outUrl, char*& outDesc, WebRect& outRect) const
{
	outUrl = NULL;
	outDesc = NULL;
	outRect.left = outRect.top = outRect.right = outRect.bottom = 0;

    Frame* frame = frameUnderMousePoint(scaledX, scaledY, core(m_page->mainFrame()));
    if (!frame || !frame->view() || !frame->document())
        return false;

    // Convert from document to Frame coords (for iframes).
    FrameView* view = frame->view();
    IntPoint viewPt = view->windowToContents(IntPoint(scaledX, scaledY));

    double scale = view->getScale();

    IntPoint documentPt = viewPt;
    IntSize scrollPos(view->scrollOffset());
    documentPt.move(scrollPos.width(), scrollPos.height());

    PlatformMouseEvent mouseEvent(viewPt, documentPt, WebCore::LeftButton, WebCore::MouseEventPressed, 1,
                                  false /*shift*/, false /*ctrl*/, false /*alt*/, false /*meta*/, WTF::currentTime());

	// preflight -- check to see if we're going to click on a "clickable" item.
	// If so, then highlight it with a special highlight. This is a READ ONLY request
	// to WebCore.
    HitTestRequest request(
                    HitTestRequest::ReadOnly |
                    HitTestRequest::Active |
                    HitTestRequest::MouseMove |
                    HitTestRequest::MouseUp);

    MouseEventWithHitTestResults results = frame->document()->prepareMouseEvent(request, documentPt, mouseEvent);
	
	WebCore::Element* link = results.hitTestResult().URLElement();
	if( link )
	{
		WebCore::HTMLLinkElement* linkEl = static_cast<WebCore::HTMLLinkElement*>( link );
		if( linkEl )
		{
            String innerTxt = link->innerText();
        
            outUrl = strdup( linkEl->href().string().utf8().data() );
            outDesc = strdup( innerTxt.utf8().data() );
        
            IntRect r = linkEl->getRect();
            outRect.left = long(r.x() * scale );
            outRect.top = long(r.y() * scale );
            outRect.right = long(r.right() * scale );
            outRect.bottom = long(r.bottom() * scale );
    
            return true;
		}	
	}
	
	return false;
}

/**
 * Handle the selection of a popup menu item.
 * @param menuObj The PopupMenu object.
 * @param selectedIdx The selected index. negative means selection was cancelled.
 */
void WebView::popupMenuSelect(void* menu, int32_t selectedIdx)
{
	if (menu) {

		PopupMenuPalm* popupMenu = reinterpret_cast<PopupMenuPalm*>(menu);

		if (selectedIdx >= 0 && selectedIdx != popupMenu->client()->selectedIndex()) {
			// this call can destroy popupMenu->client(). Example: js can overwrite
			// innerHTML of a div that contains the <select> that brings up
			// the popupMenu, which will detach the node and destroy the popupMenu
			popupMenu->client()->valueChanged(static_cast<unsigned>(selectedIdx));
		}

		// client->valueChanged may have triggered js that destroyed client - NOV-93769
		if (popupMenu->client()) {
			popupMenu->hide();
		}
	}
}

static bool webRectValid(const WebRect& rect) {

	return ((rect.left < rect.right) &&
			(rect.top < rect.bottom));	
}

bool WebView::getElementWithAttrContentRect(const char* attrName, WebRect& rect)
{
	bool ret = false;
	Document* doc = 0;
	if (!attrName)
		goto Done;

	doc = core(m_page->mainFrame())->document();
	if (!doc)
		goto Done;

	for (Node *n = doc->traverseNextNode(); n != 0; n = n->traverseNextNode()) {
		if (n->isElementNode() && n->renderer() && n->hasAttributes()) {
			Element* element = static_cast<Element*>(n);
			if (element->hasAttribute(attrName)) {

				IntRect r = n->renderBox()->absoluteOutlineBounds();

				rect.left = r.x();
				rect.top = r.y();
				rect.right = r.right();
				rect.bottom = r.bottom();

				ret = true;
				goto Done;
			}
		}
	}	
	

Done:

	if (!ret || !webRectValid(rect)) {

		int w, h;
		getContentSize(w, h);

		rect.left = 0;
		rect.top = 0;
		rect.right = w;
		rect.bottom = h;
	}

	return ret;
}

void WebView::showClickedLink( bool enable )
{
	m_showClickedLinkThisView = enable;
}

typedef std::list<RenderObject*> RenderObjectList;

static void collectRenderObjects(RenderObject* obj, RenderObjectList& objList)
{
	if (!obj)
		return;

	objList.push_back(obj);
	
	RenderObject* o = obj->firstChild();
	while (o) {
		collectRenderObjects(o, objList);
		o = o->nextSibling();
	}
}

static void collectRenderLayers(RenderLayer* layer, RenderObjectList& objList)
{
	if (!layer)
		return;
	
	collectRenderObjects(layer->renderer(), objList);

	RenderLayer* l = layer->firstChild();
	while (l) {
	    collectRenderLayers(l, objList);
		l = l->nextSibling();
	}
}

void WebView::dumpRenderTree(std::ostream &output)
{
    RenderObjectList objList;
    int w, h;

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    collectRenderLayers(frame->document()->renderer()->enclosingLayer(), objList);
    getContentSize(w, h);

    output << "{ \"address\":";
    output << std::hex << this;
    output << ", \"x\":0, \"y\":0, \"width\":";
    output << std::dec << w;
    output << ", \"height\":";
    output << std::dec << h;
    output << ", \"renderLayers\":[";

    for (RenderObjectList::const_iterator it = objList.begin(); it != objList.end(); ++it) {
        if (it != objList.begin())
            output << ", ";

        RenderObject* o = (*it);
        IntRect r = o->absoluteOutlineBounds();

        output << "{ \"address\":";
        output << std::hex << o;
        output << ", \"name\":\"";
        output << o->renderName();
        output << "\", \"parentAddress\":";
        output << std::hex << o->parent();
        output << ", \"x\":";
        output << std::dec << r.x();
        output << ", \"y\":";
        output << std::dec << r.y();
        output << ", \"width\":";
        output << std::dec << r.width();
        output << ", \"height\":";
        output << std::dec << r.height();
        output << " }";
    }

    output << "] }";
}

void WebView::dumpCompositedTree(std::ostream &output)
{
    output << "{ \"tree\":\"";
#if USE(ACCELERATED_COMPOSITING)
    if (page() &&
        page()->mainFrame() &&
        page()->mainFrame()->webcoreFrame() &&
        page()->mainFrame()->webcoreFrame()->contentRenderer() &&
        page()->mainFrame()->webcoreFrame()->contentRenderer()->compositor() &&
        page()->mainFrame()->webcoreFrame()->contentRenderer()->compositor()->rootPlatformLayer())
    {
        output << page()->mainFrame()->webcoreFrame()->contentRenderer()->compositor()->rootPlatformLayer()->layerTreeAsText(LayerTreeAsTextDebug).utf8().data();
    }
#endif
    output << "\" }";
}

void WebView::dumpMemStats(std::ostream &output)
{
#if USE(ACCELERATED_COMPOSITING)
    ContentLayerPalm::dumpMemStats(output);
#endif
}

#define DUMP_OSTREAM_TO_FILE(suffix)                             \
    void WebView::dump ## suffix (const char *fileName)          \
    {                                                            \
        std::ofstream output;                                    \
        output.open(fileName, ios_base::out | ios_base::trunc);  \
        dump ## suffix (output);                                 \
        output.close();                                          \
    }

DUMP_OSTREAM_TO_FILE(RenderTree)
DUMP_OSTREAM_TO_FILE(CompositedTree)
DUMP_OSTREAM_TO_FILE(MemStats)

bool WebView::insertStringAtCursor(const char* str)
{
    if (!m_page) {
        return false;
    }
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode()) {
        return false;
    }
    
    Editor* editor = frame->editor();
    if (!editor || !isEditing()) {
        return false;
    }

    const VisibleSelection sel(frame->selection()->selection() );
    if (sel.isNone()) {
        return false;
    }

    WebKit::EditorClient* editorClient = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );
    if (!editorClient)
        return false;

    // DFISH-2222: make sure to clean up any active word completion before inserting
    editorClient->clearWordCompletion();

    // allow the system to perform some preprocessing on the string to be inserted
    String insertionText = String::fromUTF8(str);
    editorClient->filterTextForInsertion(insertionText);
    if (insertionText.isEmpty())
        return false;

    return editor->command("InsertText").execute(insertionText);
}

bool WebView::insertStringAsKeyEvents(const char* str)
{
    if (!m_page) {
        return false;
    }

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame || !frame->document()->focusedNode()) {
        return false;
    }

    Editor* editor = frame->editor();
    if (!editor || !isEditing()) {
        return false;
    }

    WebKit::EditorClient* editorClient = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );
    if (!editorClient)
        return false;

    // allow the system to perform some preprocessing on the string to be inserted
    String insertionText = String::fromUTF8(str);
    editorClient->filterTextForInsertion(insertionText);

    for (unsigned i=0; i<insertionText.length(); i++) {
        frame->eventHandler()->keyEvent( PlatformKeyboardEvent( insertionText[i], 0x0000, false ) );
        frame->eventHandler()->keyEvent( PlatformKeyboardEvent( insertionText[i], 0x0000, true ) );
    }
    return true;
}

/**
 * Checks if node at x, y is editable by looking at the HitTestResult of a mouseevent
 * 
 * 
 */
bool WebView::isEditableAtPoint(int x, int y)
{
    Frame* frame = core(m_page->mainFrame());
    
    if (!frame) {
        return false;
    }
    
    PlatformMouseEvent mouseEvent( IntPoint(x,y), 
            IntPoint(x,1), WebCore::LeftButton,
            WebCore::MouseEventPressed, 1, false, false, false, false, 
            WTF::currentTime() );
    
    MouseEventWithHitTestResults results =
            frame->document()->prepareMouseEvent(HitTestRequest(
            		HitTestRequest::ReadOnly |
            		HitTestRequest::Active |
            		HitTestRequest::MouseMove |
            		HitTestRequest::MouseUp),
            IntPoint(x+frame->view()->scrollOffset().width(),y+frame->view()->scrollOffset().height()), mouseEvent);
            
    Node* node = results.targetNode();
    if (!node) {
        return false;
    }
    
    return node->isContentEditable();
}

Palm::WebRect WebView::getFocusedNodeRect()
{
    Palm::WebRect box;
    box.left = 0;
    box.top = 0;
    box.right = 0;
    box.bottom = 0;
    
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame) {
        return box;
    }
        
    Node* focusedNode = frame->document()->focusedNode();
    if (! focusedNode || ! focusedNode->renderer()) {
        return box;
    }
    
    IntRect renderBox = focusedNode->renderer()->absoluteBoundingBoxRect();
    box.left = renderBox.x();
    box.top = renderBox.y();
    box.right = renderBox.right();
    box.bottom = renderBox.bottom();
    
    return box;
}


void WebView::launchFullscreenView(int pluginView)
{
	Frame* frame = core(m_page->mainFrame());
	if (frame) {
		SEARCH_PLUGIN(StartFullscreenNewCard, frame, true, (void*)pluginView);
	}
}

void WebView::setViewport(const WebRect& rect, double zoom)
{
	// save viewport information for not-yet created plugin
	// pluginView::attachToWindow() will send viewport information to new created plugin
	m_viewport = rect;
	// Only send the event to plugins, for pause/resume
	// Might stop gif animation later
	Frame* frame = core(m_page->mainFrame());
	if (frame) {
	  ITERATE_PLUGIN_WITHDATA(ViewPortChanged, frame, true, const_cast<WebRect*>(&rect));

	  if (PalmBrowserSettings()->enableEnhancedViewport) {
          //Communicate the viewport information to the ScrollView
          //printf("***Setting viewport to (%d, %d, %d, %d)\n", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
          FrameView* fv = frame->view();
          fv->setScrollPosition(IntPoint(rect.left, rect.top), false);
          fv->resizeViewport(rect.right - rect.left, rect.bottom - rect.top);
          fv->scrollPositionChangedViaPlatformWidget(); // GREG TODO fv->scrollPositionChanged();
          client()->invalContents(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	  }
	}
}

const WebRect& WebView::getViewport()
{
	return m_viewport;
}

void WebView::pluginSpotlightStart(const WebRect& rect)
{
	Frame* frame = core(m_page->mainFrame());
	if (frame) {
	  ITERATE_PLUGIN_WITHDATA(PluginSpotlightStart, frame, true, const_cast<WebRect*>(&rect));
	}
}

void WebView::pluginSpotlightEnd()
{
	Frame* frame = core(m_page->mainFrame());
	if (frame) {
	  ITERATE_PLUGIN(PluginSpotlightEnd, frame, true);
	}
}

SpellingWidgetController* WebView::getSpellWidgetController()
{
    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>( core(m_page)->editorClient() );

    if (editor) {
    	return editor->getSpellWidgetController();
    }

	return 0;
}

void WebView::hideSpellingWidget()
{
	SpellingWidgetController* spellingController = getSpellWidgetController();
	if (spellingController != NULL) {
		spellingController->hide();
	}
}

void WebView::hideClipboardWidget(bool resetSelection)
{
    Palm::ClipboardController* clipboardController = getClipboardController();
    if (clipboardController)
        clipboardController->hide(resetSelection);
}

static void getLayerCompositingLayers(GraphicsLayer* rootLayer, vector<PGSurface*>& surfaces)
{
	if (!rootLayer)
		return;
	
#if DEBUG_ACCELERATED_COMPOSITING && USE(ACCELERATED_COMPOSITING)
	Vector<GraphicsLayer*> layers = rootLayer->children();
	Vector<GraphicsLayer*>::iterator child(layers.begin());
	for (; child != layers.end(); ++child) {

		getLayerCompositingLayers(*child, surfaces);

		LayerPalm* childLayer = static_cast<LayerPalm*>((*child)->nativeLayer());
		PGSurface* backing = childLayer->getRenderLayerBacking();
		if (backing) {
			surfaces.push_back(backing);
		}
	}
#endif
}

vector<PGSurface*> WebView::getCompositingLayers()
{
    vector<PGSurface*> layerBackings;
#if DEBUG_ACCELERATED_COMPOSITING && USE(ACCELERATED_COMPOSITING)
    RenderView* contentRenderer = core(m_page->mainFrame())->contentRenderer();
	if (contentRenderer) {
        if (m_data->m_layerRenderer && m_data->m_layerRenderer->rootLayerBackingSurface())
            layerBackings.push_back(m_data->m_layerRenderer->rootLayerBackingSurface());
		getLayerCompositingLayers(contentRenderer->compositor()->rootPlatformLayer(), layerBackings);
	}
#endif
    return layerBackings;
}

void WebView::setSupportsAcceleratedCompositing(bool val)
{
	m_supportsAcceleratedCompositing = val;

/* For dynamic toggling of accelerated compositing, this code seems to help. However, the
   feature is not completed so I don't want to enable this yet.

    if (page() && page()->mainFrame() && page()->mainFrame()->webcoreFrame() && page()->mainFrame()->webcoreFrame()->view())
        page()->mainFrame()->webcoreFrame()->view()->updateCompositingLayers();
*/
}

void WebView::unmapCompositingTextures()
{
    //printf( "%s: val\n", __FUNCTION__);
#if USE(ACCELERATED_COMPOSITING)
    if (m_data->m_layerRenderer) {
        m_data->m_layerRenderer->unmapTextures();
        m_data->m_layerRenderer->unlockTextures();
    }    
#endif
}

WebGLES2Context* WebView::getGLES2Context() const
{
#if USE(ACCELERATED_COMPOSITING)
    return m_data->m_gles2Context.get();
#else
    return 0;
#endif
}

/**
 * Prints the main frame of this WebView
 *
 * @param frameName       The name of the frame to print - defaults to the mainFrame if not found
 * @param lpsJobId        The Luna Print Service job Id
 * @param printableWidth  The printable width of the page in pixels
 * @param printableHeight The printable height of the page in pixels
 * @param printDpi        The print Dpi
 * @param landscape       The orientation of the page, TRUE=landscape, FALSE=portrait
 * @param reverseOrder    The order to print pages, TRUE=last-to-first, FALSE=first-to-last
 */
void WebView::print(const char* frameName, int lpsJobId, int printableWidth, int printableHeight, int printDpi, bool landscape, bool reverseOrder) const
{
    g_debug("[WebView::print] %s %d %d %d %d %s %s", frameName, lpsJobId, printableWidth, printableHeight, printDpi, landscape?"ls":"pt", reverseOrder?"up":"down");
    Frame* frame = core(m_page->mainFrame());
    if (frameName && strlen(frameName)>0) {
        Frame* foundFrame = frame->tree()->find(frameName);
        if (foundFrame) {
            g_debug("[WebView::print] found frame %s", frameName);
            frame = foundFrame;
        }
    }

    WebFrame* webFrame = kit(frame);
    if (!webFrame)
        webFrame = m_page->mainFrame();
    webFrame->print(lpsJobId,
                    printableWidth,
                    printableHeight,
                    printDpi,
                    landscape,
                    reverseOrder);
}

Palm::ClipboardController* WebView::getClipboardController()
{
    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>(core(m_page)->editorClient());
    if (editor)
        return const_cast<Palm::ClipboardController*>(editor->getClipboardController()); // TODO Una

    return 0;
}

Palm::WordCompletionController* WebView::getWordCompletionController()
{
    WebKit::EditorClient* editor = static_cast<WebKit::EditorClient*>(core(m_page)->editorClient());
    if (editor)
        return const_cast<Palm::WordCompletionController*>(editor->getWordCompletionController());

    return 0;
}

void WebView::usesSimulatedMouseClicks(bool val)
{
    m_usesSimulatedMouseClicks = val;
    if (getClipboardController())
        getClipboardController()->usesSimulatedMouseClicks(val);
}

typedef std::list<RenderLayer*> RenderLayerList;

static void collectScrollableRenderLayers(RenderLayer* layer, RenderLayerList& layerList)
{
    if (!layer)
        return;

    if (layer->parent() &&  (layer->horizontalScrollbar() ||
                             layer->verticalScrollbar()))
        layerList.push_back(layer);

    RenderLayer* l = layer->firstChild();
    while (l) {
        collectScrollableRenderLayers(l, layerList);
        l = l->nextSibling();
    }
}

std::list<ScrollableLayerItem> WebView::getScrollableLayers() const
{
    std::list<ScrollableLayerItem> scrollableLayerList;

    Frame* mainFrame = core(m_page->mainFrame());
    if (!mainFrame)
        return scrollableLayerList;

    RenderLayerList renderLayerList;
    collectScrollableRenderLayers(mainFrame->document()->renderer()->enclosingLayer(),
                                  renderLayerList);

    for (RenderLayerList::const_iterator it = renderLayerList.begin();
         it != renderLayerList.end(); ++it) {

        RenderLayer* layer = (*it);
        IntRect bbox = layer->absoluteBoundingBox();

        ScrollableLayerItem item;

        item.id = (uintptr_t) layer;

        item.absoluteBounds.left = bbox.x();
        item.absoluteBounds.top = bbox.y();
        item.absoluteBounds.right = bbox.right();
        item.absoluteBounds.bottom = bbox.bottom();

        if (layer->horizontalScrollbar()) {
            Scrollbar* bar = layer->horizontalScrollbar();
            item.horizontalBarData.value = bar->value();
            item.horizontalBarData.visible = bar->visibleSize();
            item.horizontalBarData.total = bar->totalSize();
            item.hasHorizontalBar = true;
        }
        else
            item.hasHorizontalBar = false;

        if (layer->verticalScrollbar()) {
            Scrollbar* bar = layer->verticalScrollbar();
            item.verticalBarData.value = bar->value();
            item.verticalBarData.visible = bar->visibleSize();
            item.verticalBarData.total = bar->totalSize();
            item.hasVerticalBar = true;
        }
        else
            item.hasVerticalBar = false;

        scrollableLayerList.push_back(item);
    }

    return scrollableLayerList;
}

void WebView::scrollLayer(void* id, int deltaX, int deltaY)
{
    Frame* mainFrame = core(m_page->mainFrame());
    if (!mainFrame)
        return;

    RenderLayerList renderLayerList;
    collectScrollableRenderLayers(mainFrame->document()->renderer()->enclosingLayer(),
                                  renderLayerList);

    RenderLayer* layer = 0;

    for (RenderLayerList::const_iterator it = renderLayerList.begin();
         it != renderLayerList.end(); ++it) {
        RenderLayer* l = (*it);
        if (l == id) {
            layer = l;
            break;
        }
    }

    if (!layer)
        return;

    Scrollbar* hs = layer->horizontalScrollbar();
    if (hs && deltaX) {
        hs->setValue(hs->value() + deltaX);
    }

    Scrollbar* vs = layer->verticalScrollbar();
    if (vs && deltaY) {
        vs->setValue(vs->value() + deltaY);
    }
}

}
