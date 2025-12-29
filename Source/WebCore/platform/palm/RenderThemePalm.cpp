/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <assert.h>
#include "config.h"
#include "RenderThemePalm.h"

#include "GraphicsContext.h"
#include "RenderBox.h"
#include "RenderObject.h"

#include "PGContext.h"
#include "PGSurface.h"
#include "FormControlGraphicsPalm.h"

#include "webkitpalmsettings.h"
#include "CaretSymbols.h"

#define THEME_COLOR 204
#define THEME_FONT  210

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

/*
 * Approach to theming:
 *  a) keep one copy of each to be drawn widget, GtkEntry, GtkButton, Gtk...
 *     + the button will look like the native control
 *     + we don't need to worry about style updates and loading the right GtkStyle
 *     - resources are wasted. The native windows will not be used, we might have issues
 *       with
 *
 *  b) Use GtkStyle directly and copy and paste Gtk+ code
 *
 *
 * We will mix a and b
 *
 * - Create GtkWidgets to hold the state (disabled/enabled), selected, not selected.
 * - Use a GdkPixmap to make the GtkStyle draw to and then try to convert set it the
 *   source of the current operation.
 *
 */
 
static bool s_RenderThemeInitialized = false;

// checkboxes
static PGSurface*			s_img_cb_en_unchecked = 0;
static PGSurface*			s_img_cb_en_checked = 0;
static PGSurface*			s_img_cb_dis_unchecked = 0;
static PGSurface*			s_img_cb_dis_checked = 0;

// radio buttons
static PGSurface*			s_img_rad_en_unselected = 0;
static PGSurface*			s_img_rad_en_selected = 0;
static PGSurface*			s_img_rad_dis_unselected = 0;
static PGSurface*			s_img_rad_dis_selected = 0;

// push button
static PGSurface*			s_img_push_dis_up = 0; // disabled
static PGSurface*			s_img_push_en_up = 0;  // enabled up (not pressed)
static PGSurface*			s_img_push_en_down = 0;// enabled down (pressed)

// Caret Symbols
static PGSurface*			s_img_alt = 0;
static PGSurface*			s_img_altlock = 0;
static PGSurface*			s_img_shift= 0;
static PGSurface*			s_img_shiftlock= 0;

const float k_fMenuListArrowWidth  = 12.0f;
const float k_fMenuListArrowHeight =  8.0f;
const int   k_nMenuListMargin = 2;

// ...

namespace WebCore {
	


static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
        case PushButtonPart:
        case ButtonPart:
        case TextFieldPart:
        case TextAreaPart:
        case SearchFieldPart:
        case MenulistPart:
        case RadioPart:
        case CheckboxPart:
            return true;
        default:
            return false;
    }
}


PassRefPtr<RenderTheme> RenderThemePalm::create()
{
    return adoptRef(new RenderThemePalm());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemePalm::create().releaseRef();
    return rt;
}

RenderThemePalm::RenderThemePalm()
{
	if( !s_RenderThemeInitialized )
	{
		// Initialize the resources.
		s_img_cb_en_unchecked = PGSurface::createFromPNGStream( cb_en_unchecked_data );
		s_img_cb_en_checked = PGSurface::createFromPNGStream( cb_en_checked_data );
		s_img_cb_dis_unchecked = PGSurface::createFromPNGStream( cb_dis_unchecked_data );
		s_img_cb_dis_checked = PGSurface::createFromPNGStream( cb_dis_checked_data );
		
		s_img_rad_en_unselected = PGSurface::createFromPNGStream( rad_en_unchecked_data );
		s_img_rad_en_selected = PGSurface::createFromPNGStream( rad_en_checked_data );
		s_img_rad_dis_selected = PGSurface::createFromPNGStream( rad_dis_checked_data );
		s_img_rad_dis_unselected = PGSurface::createFromPNGStream( rad_dis_unchecked_data );

		s_img_push_dis_up = PGSurface::createFromPNGStream( Browser_form_button_disabled_data );
		s_img_push_en_up = PGSurface::createFromPNGStream( Browser_form_button_static_data );
		s_img_push_en_down = PGSurface::createFromPNGStream( Browser_form_button_press_data );
		
		s_img_alt = PGSurface::createFromPNGStream( kAltPng );
		s_img_altlock = PGSurface::createFromPNGStream( kAltLockPng );
		s_img_shift = PGSurface::createFromPNGStream( kShiftPng );
		s_img_shiftlock = PGSurface::createFromPNGStream( kShiftLockPng );
		
		s_RenderThemeInitialized = true;
	}
}

RenderThemePalm::~RenderThemePalm()
{
}

PGSurface*	RenderThemePalm::platformCaretDecoration( int enumTextCaretDecoration )
{
	switch( enumTextCaretDecoration ) {
	case TextCaretDecoration::ArrowHollow:
		return s_img_shift;
	case TextCaretDecoration::ArrowFilled:
		return s_img_shiftlock;
	case TextCaretDecoration::CircleHollow:
		return s_img_alt;
	case TextCaretDecoration::CircleFilled:
		return s_img_altlock;
	case TextCaretDecoration::Number:
		return 0;
	}
	
	return 0;
}

int RenderThemePalm::baselinePosition(const RenderObject* o) const
{
    // FIXME: This strategy is possibly incorrect for the GTK+ port.
    if (o->style()->appearance() == CheckboxPart ||
        o->style()->appearance() == RadioPart) {
		const RenderBox* box = toRenderBox(o);
        return box->marginTop() + box->height() - 2;
	}
    return RenderTheme::baselinePosition(o);
}

bool RenderThemePalm::supportsFocusRing(const RenderStyle* style) const
{
	// We lie to WebKit and say that we support focus rings (which we don't).
	// We do this because WebKit's algorithm to calculate the focus rect is
	// horrible (like O(n^3)) and takes *forever* on our device. If you want
	// more info see NOV-97430.
	return true;
}

bool RenderThemePalm::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

void RenderThemePalm::systemFont(int propId, FontDescription&) const
{
}

/**
 * Set the radio button size (also used for checkboxes).
 */
void RenderThemePalm::setRadioSize(RenderStyle* style) const 
{ 
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

	// FIXME:  A hard-coded size of 16 is used.  This is wrong but necessary for now.  It matches Firefox.
	// At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
	// the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
	// metrics.
	const int ff = 16;
	if (style->width().isIntrinsicOrAuto())
		style->setWidth(Length(ff, Fixed));

	if (style->height().isAuto())
		style->setHeight(Length(ff, Fixed));		
}

bool RenderThemePalm::paintRadio(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{ 
	if( s_img_rad_en_unselected && s_img_rad_en_selected && s_img_rad_dis_selected && s_img_rad_dis_unselected )
	{
		PGContext* c = i.context->platformContext();
		PGSurface* s = 0;
		
		if( isEnabled(o) ) {
			s = isChecked(o) ? s_img_rad_en_selected : s_img_rad_en_unselected;
		}
		else {
			s = isChecked(o) ? s_img_rad_dis_selected : s_img_rad_dis_unselected;
		}
	
		c->bitblt( s, 0, 0, (int)(s->width()), (int)(s->height()),
				(int)(rect.x()), (int)(rect.y()), (int)(rect.right()), (int)rect.bottom() );
	}
	else
	{
		i.context->save();
		i.context->setStrokeThickness(1);
		i.context->setFillColor(Color(128,0,128),sRGBColorSpace);
		i.context->setStrokeColor(Color(0, 0, 0),sRGBColorSpace);
		
		if( isChecked(o) )
			i.context->setFillColor(Color(0,0,0),sRGBColorSpace);
		else if( isPressed(o) )		
			i.context->setFillColor(Color(0,255,0),sRGBColorSpace);
		
		if( !isEnabled(o) )
			i.context->setStrokeColor(Color(180,180,180),sRGBColorSpace);
	   
		i.context->drawEllipse(rect);
		i.context->restore();
	}
    return false;
}

void RenderThemePalm::setCheckboxSize(RenderStyle* style) const 
{ 
    setRadioSize(style);
}

bool RenderThemePalm::paintCheckbox(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
	if( s_img_cb_en_unchecked && s_img_cb_en_checked && s_img_cb_dis_checked && s_img_cb_dis_unchecked )
	{
		PGContext* c = i.context->platformContext();

		static PGSurface* surface(NULL);

		if (isEnabled(o)) {
			surface = isChecked(o) ? s_img_cb_en_checked : s_img_cb_en_unchecked;
		}
		else {
			surface = isChecked(o) ? s_img_cb_dis_checked : s_img_cb_dis_unchecked;
		}

		c->bitblt( surface, 0, 0, 
				(int)(surface->width()), (int)(surface->height()),
				(int)(rect.x()), (int)(rect.y()), (int)(rect.right()), 
				(int)(rect.bottom()) );

		//else if( isPressed(o) )		
		//	i.context->setFillColor(Color(0,255,0));
	}
	else
	{
		i.context->save();
		i.context->setStrokeThickness(1);
		i.context->setFillColor(Color(128,0,128),sRGBColorSpace);
		i.context->setStrokeColor(Color(0, 0, 0),sRGBColorSpace);
	
		if( isChecked(o) )
			i.context->setFillColor(Color(0,0,0),sRGBColorSpace);
		else if( isPressed(o) )		
			i.context->setFillColor(Color(0,255,0),sRGBColorSpace);
		
		if( !isEnabled(o) )
			i.context->setStrokeColor(Color(180,180,180),sRGBColorSpace);
	   
		i.context->drawRect(rect);
		i.context->restore();
	}
    return false;
}

bool RenderThemePalm::paintButton(RenderObject* o, const PaintInfo& i, const IntRect& rect) 
{
	static PGSurface* surface(NULL);

	//fprintf(stdout, "Drawing button\n");
	if (isEnabled(o)) {
		surface = isPressed(o) ? s_img_push_en_down : s_img_push_en_up;
		//if (isPressed(o))
		//	fprintf(stdout, "Pushbutton is pressed!\n");
	}
	else {
		surface = s_img_push_dis_up;
	}

	// This math assumes a square source image.
	assert(surface->width() == surface->height());
	PGContext* c = i.context->platformContext();
	int imgWidth = surface->width();
	int cornerRadius = imgWidth / 3;
	int middleWidth = imgWidth - 2 * cornerRadius;
	int srcCol3TL = imgWidth - cornerRadius;

	int dstRadius;
	if (rect.height() <= rect.width()) {
		dstRadius = rect.height() / 3;
	}
	else {
		dstRadius = rect.width() / 3;
	}

	// TL
	c->bitblt( surface, 0, 0, cornerRadius, cornerRadius,
			rect.x(), rect.y(), rect.x() + dstRadius, rect.y() + dstRadius );
	
	// TC
	c->bitblt( surface, cornerRadius, 0, srcCol3TL, cornerRadius,
			rect.x() + dstRadius, rect.y(), rect.right() - dstRadius, rect.y() + dstRadius );
	
	// TR
	c->bitblt( surface, srcCol3TL, 0, imgWidth, cornerRadius,
			rect.right() - dstRadius, rect.y(), rect.right(), rect.y() + dstRadius );

	// ML
	c->bitblt( surface, 0, cornerRadius, cornerRadius, srcCol3TL,
			rect.x(), rect.y() + dstRadius, rect.x() + dstRadius, rect.y() + rect.height() - dstRadius );
	
	// MC
	c->bitblt( surface, cornerRadius, cornerRadius, srcCol3TL, srcCol3TL,
			rect.x() + dstRadius, rect.y() + dstRadius, rect.right() - dstRadius, rect.bottom() - dstRadius );
	
	// MR
	c->bitblt( surface, srcCol3TL, cornerRadius, imgWidth, srcCol3TL,
			rect.right() - dstRadius, rect.y() + dstRadius, rect.right(), rect.bottom() - dstRadius );

	// BL
	c->bitblt( surface, 0, srcCol3TL, cornerRadius, imgWidth,
			rect.x(), rect.bottom() - dstRadius, rect.x() + dstRadius, rect.bottom() );

	// BC
	c->bitblt( surface, cornerRadius, srcCol3TL, srcCol3TL, imgWidth,
			rect.x() + dstRadius, rect.bottom() - dstRadius, rect.right() - dstRadius, rect.bottom() );

	// BR
	c->bitblt( surface, srcCol3TL, srcCol3TL, imgWidth, imgWidth,
			rect.right() - dstRadius, rect.bottom() - dstRadius, rect.right(), rect.bottom() );

	return false;
}

bool RenderThemePalm::paintTextArea(RenderObject*, const PaintInfo& i, const IntRect& rect)
{
	i.context->save();
	i.context->setStrokeThickness(1);
	i.context->setStrokeColor(Color(200, 200, 200),sRGBColorSpace);
	i.context->setFillColor(Color(255,255,255),sRGBColorSpace);
	i.context->setStrokeStyle( SolidStroke );
	i.context->drawRect( rect );
	i.context->restore();
	return false;
}

bool RenderThemePalm::paintTextField(RenderObject*, const PaintInfo& i, const IntRect& rect)
{
	i.context->save();
	i.context->setStrokeThickness(1);
	i.context->setStrokeColor(Color(200, 200, 200),sRGBColorSpace);
	i.context->setFillColor(Color(255,255,255),sRGBColorSpace);
	i.context->setStrokeStyle( SolidStroke );
	i.context->drawRect( rect );
	i.context->restore();
	return false;
}

void RenderThemePalm::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // Height is locked to auto.
    style->setHeight(Length(Auto));

    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    // Add in the padding that we'd like to use.
    style->resetPadding();

    style->setPaddingLeft(Length(k_nMenuListMargin, Fixed));
    style->setPaddingRight(Length(int(k_fMenuListArrowWidth) + 2*k_nMenuListMargin, Fixed));
    style->setPaddingTop(Length(k_nMenuListMargin, Fixed));
    style->setPaddingBottom(Length(k_nMenuListMargin, Fixed));

	// Set the button's vertical size.
    setButtonSize(style);
}

void RenderThemePalm::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
	adjustMenuListStyle(selector, style, e);
}

/**
 * Draw a rounded rectangle. This includes the fill color as well as the border line.
 *
 * @param c            The drawing context.
 * @param r            The bounging rectangle.
 * @param cornerRadius The width & height of the oval to use for the rounded corners.
 * @param fillColor    The color to fill the rounded rectangle with.
 * @param strokeColor  The line color.
 */
void RenderThemePalm::drawRoundedRect(PGContext* c, const IntRect& r, const IntSize& cornerRadius, const Color& fillColor, 
			const Color& strokeColor)
{
	c->push();
	c->setStrokeColor(PColor32(strokeColor.red(), strokeColor.green(), strokeColor.blue(), strokeColor.alpha()));
	c->setFillColor(PColor32(fillColor.red(), fillColor.green(), fillColor.blue(), fillColor.alpha()));
    c->setStrokeStyle(c->SolidStroke);
	c->drawRoundRect(r.x(), r.y(), r.right(), r.bottom(), cornerRadius.width(), cornerRadius.height());
	c->pop();
}

void RenderThemePalm::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect,
		const Color& fillColor, const Color& borderColor)
{
	ASSERT(!i.context->paintingDisabled());
	i.context->save();

	const Color menuListEnabledArrowColor(0x0,0x0,0x0);
	const Color menuListDisabledArrowColor(0x90,0x90,0x60);

	if (fillColor.isValid()) {
		// Draw the bounding round rectangle.
		const IntSize radius(5,5);
		i.context->setStrokeThickness(1);
		PGContext* c = i.context->platformContext();
		drawRoundedRect(c, rect, radius, fillColor, borderColor);
	}

	// And now the drop down arrow
	FloatPoint	pts[3];

	const float wd2 = k_fMenuListArrowWidth  / 2.0f;
	const float hd2 = k_fMenuListArrowHeight / 2.0f;
	float vcenter = (rect.y() + rect.bottom()) / 2.0f;
	pts[0].setX(rect.right() - k_nMenuListMargin - k_fMenuListArrowWidth);
	pts[0].setY(vcenter - hd2);
	pts[1].setX(rect.right() - k_nMenuListMargin - wd2);
	pts[1].setY(vcenter + hd2);
	pts[2].setX(rect.right() - k_nMenuListMargin);
	pts[2].setY(pts[0].y());
	i.context->setStrokeStyle(NoStroke);
	if( isEnabled(o) )
		i.context->setFillColor(menuListEnabledArrowColor,sRGBColorSpace);
	else
		i.context->setFillColor(menuListDisabledArrowColor,sRGBColorSpace);
	i.context->drawConvexPolygon(G_N_ELEMENTS(pts), pts, true /* AA */);

	i.context->restore();
}

/**
 * Render a menu list (AKA a SELECT tag).
 *
 * @return true if painted the theme, false and the caller will do so.
 */
bool RenderThemePalm::paintMenuList(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
	if (i.context->paintingDisabled())
		return false;

	const Color menuListFillColor(0xee,0xee,0xee);
	const Color menuListEnabledOutlineColor(0x0,0x0,0x0);
	const Color menuListDisabledOutlineColor(0x40,0x40,0x40);

	if (isEnabled(o))
		paintMenuList( o, i, rect, menuListFillColor, menuListEnabledOutlineColor );
	else
		paintMenuList( o, i, rect, menuListFillColor, menuListDisabledOutlineColor );
	
	return false;
}

/**
 * Render a styled menu list (AKA a SELECT tag) that. These are usually drawn with a simple pushbutton
 * type appearance.
 *
 * @return true if painted the theme, false and the caller will do so.
 */
bool RenderThemePalm::paintMenuListButton(RenderObject* o, const PaintInfo& i, const IntRect& rect)
{
	if (i.context->paintingDisabled())
		return false;

	// We don't pass in a fill/border color because the RenderObject draws that for me.
	Color	disabledColor;
	paintMenuList( o, i, rect, disabledColor, disabledColor );
	
	return false;
}

double RenderThemePalm::caretBlinkInterval() const
{
    return 0.6;
}

// The platform selection color.
Color RenderThemePalm::platformActiveSelectionBackgroundColor() const
{
	return Color( 0xff,0xff,0x66 ); // #FFFF66
}

Color RenderThemePalm::platformInactiveSelectionBackgroundColor() const
{
	return Color( 0xff,0xff,0x66 ); // #FFFF66
}

Color RenderThemePalm::platformActiveSelectionForegroundColor() const
{
	return Color( 0,0,0 );
}

Color RenderThemePalm::platformInactiveSelectionForegroundColor() const
{
	return Color( 0, 0, 0 );
}


}
