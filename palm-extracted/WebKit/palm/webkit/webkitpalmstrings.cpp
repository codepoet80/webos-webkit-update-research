/*
    Copyright 2011 Palm Inc.
*/

#include "config.h"

#include "palmwebglobal.h"
#include "DiskCachePalm.h"
#include "webkitpalmstrings.h"
#include "webkitpalmsettings.h"
#include <DatabaseTracker.h>

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include <string>

namespace {

class LocalizedStringsMap {
public:
    LocalizedStringsMap();
    String find(const String& key);

private:
    static const std::string stringsFileName;
    void read();
    HashMap<String, String> m_map;
    String m_locale;
};

const std::string LocalizedStringsMap::stringsFileName("/data/strings.conf");

LocalizedStringsMap::LocalizedStringsMap()
{
    read();
}

// Read in the localized string from file for the current locale.
void LocalizedStringsMap::read()
{
    String locale(Palm::WebGlobal::getLocale());
    if (locale.isEmpty())
        return;

    if (m_locale == locale)
        return;

    m_locale = locale;
    m_map.clear();

    GKeyFile* keyfile;
    GKeyFileFlags flags;
    GError* error = 0;

    keyfile = g_key_file_new();
    if (!keyfile)
        return;
    flags = GKeyFileFlags(G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);

    // Load the file from webkitDataPath/data
    std::string filePath(PalmBrowserSettings()->webkitDataPath + stringsFileName);
    if (!g_key_file_load_from_file( keyfile, filePath.c_str(), flags, &error)) {
        if (error) {
            g_warning("%s load error: (%s).", filePath.c_str(), error->message );
            g_error_free(error);
            error = 0;
        }
    }

    gsize numKeys = 0;
    gchar** keys = g_key_file_get_keys(keyfile, "General", &numKeys, &error);
    if (error) {
        g_strfreev(keys);
        g_error_free(error);
    }

    for (gsize i = 0; i < numKeys; ++i) {
        gchar* key = keys[i];
        gchar* value = 0;

        // Format of the key is KEY[LOCALE].
        const char* localeLocationInKey = strstr(key, locale.utf8().data());
        if (!localeLocationInKey)
            continue; // This key is not for the current locale.

        value = g_key_file_get_string(keyfile, "General", key, &error);
        if (!error && value) {
            int keyLen = localeLocationInKey - key - 1;
            String mapKey(key, keyLen);
            String mapValue = String::fromUTF8(value, strlen(value));
            m_map.add(mapKey, mapValue);
            g_free(value);
        }
        else
            g_error_free(error);
    }

    g_strfreev(keys);
    g_key_file_free(keyfile);
}

String LocalizedStringsMap::find(const String& key)
{
    String locale(Palm::WebGlobal::getLocale());
    if (m_locale != locale)
        read();

    HashMap<String, String>::iterator it = m_map.find(key);
    if (it != m_map.end())
        return it->second;

    return String();
}

}

namespace Palm {

String getLocalizedBrowserString(const char* key)
{
    static LocalizedStringsMap map;

    String keyStr(key);
    String value = map.find(keyStr);
    if (!value.isEmpty())
        return value;

    return keyStr;
}

}
