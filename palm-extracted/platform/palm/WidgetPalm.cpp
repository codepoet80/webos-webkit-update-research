// Copyright 2008 Palm Inc.

#include "config.h"
#include "Widget.h"

#include "Cursor.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderObject.h"

#include "palmwebview.h"
#include "palmwebpage.h"
#include "palmwebviewclient.h"

namespace WebCore {

class WidgetPrivate {
public:
    //WidgetClient* client;
    IntRect frameRect;
	
    ScrollView* parent;
	bool suppressInvalidation;

    Palm::WebView* view;
};

Widget::Widget(PlatformWidget p)
    : m_data(new WidgetPrivate)
{
    m_data->parent = 0;
	m_data->suppressInvalidation = false;
	//m_data->client = 0;
    m_data->view = 0;
	
	init(p);
}

Widget::~Widget()
{
	ASSERT(!parent());
	releasePlatformWidget();
    delete m_data;
}

void Widget::setFrameRect(const IntRect& r)
{
	 m_data->frameRect = r;
}

void Widget::setFocus(bool)
{
}

void Widget::setCursor(const Cursor& cursor)
{
}

void Widget::show()
{
	notImplemented();
}

void Widget::hide()
{
	notImplemented();
}

/*
 * Strategy to painting a Widget:
 *  1.) do not paint if there is no GtkWidget set
 *  2.) We assume that GTK_NO_WINDOW is set and that geometryChanged positioned
 *      the widget correctly. ATM we do not honor the GraphicsContext translation.
 */
void Widget::paint(GraphicsContext* context, const IntRect&)
{
	// GREG : In GTK we generate an "expose" event which causes the GTK widget to paint
	// (basically an Invalidation, I think)

	notImplemented();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

void Widget::setSuppressInvalidation(bool suppress)
{
    m_data->suppressInvalidation = suppress;
}

void Widget::setContainingWindow(Palm::WebView* window)
{
    m_data->view = window;
}

Palm::WebView* Widget::containingWindow() const
{
    return m_data->view;
}

IntRect Widget::frameRect() const
{
	return m_data->frameRect;
}


void Widget::releasePlatformWidget()
{
    if (!platformWidget())
         return;
}

void Widget::retainPlatformWidget()
{
    if (!platformWidget())
         return;
}

}
