#include "weboseventreporter_webkit.h"
#include <stdarg.h>
#include <glib.h>
#include <vector>
#include "Document.h"
#include "text/CString.h"
#include "v8.h"

namespace webOS
{

int Reporter::s_verbosity = 0;

// V8 calls back to this function to report data.
static void
externalReporterFunction(int type, const char* name, const char* value)
{
    switch( type )
    {
    case 1:
        Reporter::Event::logStart(0, name, value);
        break;
    case 2:
        Reporter::Event::logStop(0, name, value);
        break;
    case 0:
        Reporter::Event::logEvent(0, name, value);
        break;
    }
}

void Reporter::Event::logStart(const void* token, const char* name, const char* value)
{
#ifndef WEBOS_DESKTOP
    if (s_verbosity && webos_eventreporter_is_active()) {
        webos_eventreporter_event_start(token, name, value);
    }
#endif
}

void Reporter::Event::logStop(const void* token, const char* name, const char* value)
{
#ifndef WEBOS_DESKTOP
    if (s_verbosity && webos_eventreporter_is_active()) {
        webos_eventreporter_event_stop(token, name, value);
    }
#endif
}

Reporter::Event::Event(const void* token, const char* name, const char* fmt, ...) :
      m_token(token)
      , m_started(false)
{
    va_list args;

#ifndef WEBOS_DESKTOP
    if (s_verbosity && webos_eventreporter_is_active()) {
        m_name = name;
        va_start(args, fmt);
        char* buff;
        if (vasprintf(&buff, fmt, args) != -1) {
            webos_eventreporter_event_start(m_token, name, buff);
            m_started = true;
            free(buff);
        }
        va_end(args);
    }
#endif
}

void Reporter::Event::stop()
{
    if (!m_started)
        return;

#ifndef WEBOS_DESKTOP
    if (s_verbosity && webos_eventreporter_is_active()) {
        webos_eventreporter_event_stop(m_token, m_name.c_str(), "");
    }
#endif

    m_started = false;
}

Reporter::Event::~Event()
{
    stop();
}

void Reporter::Event::logEvent(const void* token, const char* name, const char* fmt, ...)
{
    va_list args;

#ifndef WEBOS_DESKTOP
    if (s_verbosity && webos_eventreporter_is_active()) {
        va_start(args, fmt);
        char* buff;
        if (vasprintf(&buff, fmt, args) != -1) {
            webos_eventreporter_event(token, name, buff);
            free(buff);
        }
        va_end(args);
    }
#endif
}

void Reporter::init(GMainLoop *mainLoop)
{
#ifndef WEBOS_DESKTOP
    webos_eventreporter_init(mainLoop);
    v8::V8::SetReporterCallback(externalReporterFunction);
#endif
}

void Reporter::setVerbosity(int verbosity)
{
    s_verbosity = verbosity;
}

}
