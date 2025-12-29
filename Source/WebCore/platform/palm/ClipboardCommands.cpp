/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "ClipboardCommands.h"
#include "webkitpalmstrings.h"
#include "Node.h"
#include "htmlediting.h"

#include "palmwebframeprivate.h"
#include "palmwebframe.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "EditorClientPalm.h"


namespace Palm {

ClipboardCommand::ClipboardCommand()
    : m_frame(0)
    , m_bounds()
    , m_textPosition()
{
}

bool ClipboardCutCommand::execute() const
{
    if (!m_frame)
        return false;

    return m_frame->editor()->command("Cut").execute();
}

bool ClipboardCutCommand::enabled(const WebCore::Frame* frame)
{
    if (!frame)
        return false;

    if (frame->selection()->isRange() && frame->selection()->isContentEditable())
        return true;

    return false;
}

std::string& ClipboardCutCommand::toString() const {
    static std::string name("Cut");
    return name;
}

String ClipboardCutCommand::toLocalizedString() const {
    return getLocalizedBrowserString("Cut");
}

bool ClipboardCopyCommand::execute() const
{
    if (!m_frame)
        return false;

    bool val = m_frame->editor()->command("Copy").execute();

    Palm::WebFrame* webFrame = kit(const_cast<WebCore::Frame*>(m_frame));
    if (webFrame && webFrame->page() && webFrame->page()->view() && webFrame->page()->view()->client())
        webFrame->page()->view()->client()->copiedToClipboard();
    return val;
}

bool ClipboardCopyCommand::enabled(const WebCore::Frame* frame)
{
    if (!frame)
        return false;

    if (frame->selection()->isRange())
        return true;

    return false;
}

std::string& ClipboardCopyCommand::toString() const {
    static std::string name("Copy");
    return name;
}

String ClipboardCopyCommand::toLocalizedString() const {
    return getLocalizedBrowserString("Copy");
}

bool ClipboardPasteCommand::execute() const
{
    if (!m_frame)
        return false;

    return m_frame->editor()->command("Paste").execute();
}

bool ClipboardPasteCommand::enabled(const WebCore::Frame* frame)
{
    if (!frame)
        return false;

    String text = WebCore::Pasteboard::generalPasteboard()->plainText();
    if (!text.isEmpty() && frame->selection()->isContentEditable())
        return true;

    return false;
}
std::string& ClipboardPasteCommand::toString() const {
    static std::string name("Paste");
    return name;
}

String ClipboardPasteCommand::toLocalizedString() const {
    return getLocalizedBrowserString("Paste");
}

bool ClipboardSelectAllCommand::execute() const
{
    if (!m_frame)
        return false;

    bool val = m_frame->editor()->command("SelectAll").execute();

    if (m_frame->editor()->client()) {
        WebKit::EditorClient* client = static_cast<WebKit::EditorClient*>(m_frame->editor()->client());
        if (client->getClipboardController())
            client->getClipboardController()->setMarkersActive(true);
    }

    return val;
}

bool ClipboardSelectAllCommand::enabled(const WebCore::Frame* frame)
{
    if (!frame)
        return false;

    if (frame->selection()->isContentEditable()) {
        WebCore::Node* node = highestEditableRoot(frame->selection()->start());
        if (node && node->textContent().isEmpty())
            return false;
    }

    if (!frame->selection()->isRange())
        return true;

    return false;
}

std::string& ClipboardSelectAllCommand::toString() const {
    static std::string name("SelectAll");
    return name;
}

String ClipboardSelectAllCommand::toLocalizedString() const {
    return getLocalizedBrowserString("Select All");
}

bool ClipboardSelectCommand::execute() const
{
    if (!m_frame)
        return false;

    bool val = m_frame->editor()->command("SelectWord").execute();

    if (m_frame->editor()->client()) {
        WebKit::EditorClient* client = static_cast<WebKit::EditorClient*>(m_frame->editor()->client());
        if (client->getClipboardController())
            client->getClipboardController()->setMarkersActive(true);
    }

    return val;
}

bool ClipboardSelectCommand::enabled(const WebCore::Frame* frame)
{
    if (!frame)
        return false;

    if (frame->selection()->isContentEditable()) {
        WebCore::Node* node = highestEditableRoot(frame->selection()->start());
        if (node && node->textContent().isEmpty())
            return false;
    }

    if (!frame->selection()->isRange())
        return true;

    return false;
}
std::string& ClipboardSelectCommand::toString() const {
    static std::string name("SelectWord");
    return name;
}

String ClipboardSelectCommand::toLocalizedString() const {
    return getLocalizedBrowserString("Select");
}


}
