/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */


#ifndef CLIPBOARDCOMMANDS_H
#define CLIPBOARDCOMMANDS_H

#include "config.h"

#include <string>

#include "Frame.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "Pasteboard.h"

namespace Palm {

class WebPage;

class ClipboardCommand {
public:
    ClipboardCommand();
    virtual ~ClipboardCommand() {}

    // TODO should contain UI string?
    // TODO should have some method setBounds() called during layout time?
    // TODO should have method to test if mouse event landed in it's bounds?

    virtual bool execute() const = 0;
    virtual bool enabled(const WebCore::Frame* frame) = 0;
    virtual std::string& toString() const = 0; // for debugging
    virtual String toLocalizedString() const = 0;
    virtual void setBounds(WebCore::IntRect bounds) { m_bounds = bounds; }
    virtual WebCore::IntRect bounds() const { return m_bounds; }
    virtual void setTextPosition(WebCore::IntPoint point) { m_textPosition = point; }
    virtual WebCore::IntPoint textPosition() const { return m_textPosition; }
    virtual void setFrame(const WebCore::Frame* frame) { m_frame = frame; }

protected:
    const WebCore::Frame* m_frame;

private:
    WebCore::IntRect m_bounds;
    WebCore::IntPoint m_textPosition;
};

class ClipboardCutCommand : public ClipboardCommand {
public:
    ClipboardCutCommand() { }
    virtual ~ClipboardCutCommand() { }

    virtual bool execute() const;
    virtual bool enabled(const WebCore::Frame* frame);
    virtual std::string& toString() const;
    virtual String toLocalizedString() const;
};

class ClipboardCopyCommand : public ClipboardCommand {
public:
    ClipboardCopyCommand() { }
    virtual ~ClipboardCopyCommand() { }

    virtual bool execute() const;
    virtual bool enabled(const WebCore::Frame* frame);
    virtual std::string& toString() const;
    virtual String toLocalizedString() const;
};

class ClipboardPasteCommand : public ClipboardCommand {
public:
    ClipboardPasteCommand() { }
    virtual ~ClipboardPasteCommand() { }

    virtual bool execute() const;
    virtual bool enabled(const WebCore::Frame* frame);
    virtual std::string& toString() const;
    virtual String toLocalizedString() const;
};

class ClipboardSelectAllCommand : public ClipboardCommand {
public:
    ClipboardSelectAllCommand() { }
    virtual ~ClipboardSelectAllCommand() { }

    virtual bool execute() const;
    virtual bool enabled(const WebCore::Frame* frame);
    virtual std::string& toString() const;
    virtual String toLocalizedString() const;
};

class ClipboardSelectCommand : public ClipboardCommand {
public:
    ClipboardSelectCommand() { }
    virtual ~ClipboardSelectCommand() { }

    virtual bool execute() const;
    virtual bool enabled(const WebCore::Frame* frame);
    virtual std::string& toString() const;
    virtual String toLocalizedString() const;
};




}

#endif
