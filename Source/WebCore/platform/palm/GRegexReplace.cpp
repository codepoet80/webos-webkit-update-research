
#include <GRegexReplace.h>

using namespace std;

namespace WebCore {

GRegexReplace::GRegexReplace()
    : regex(NULL)
{
}

GRegexReplace::~GRegexReplace()
{
    if (regex) {
        g_regex_unref(regex);
        regex = NULL;
    }
}

void GRegexReplace::addRule(const TextReplacementRule& rule)
{
    if (!regex) {
        rules.push_back(rule);
    }
}

bool GRegexReplace::compile()
{
    // only compile if we haven't already
    if (!regex) {
        int nRules = rules.size();
        string expr = rules[0].expression;
        for (int i = 1; i < nRules; i++) {
            expr += "|" + rules[i].expression;
        }

        GError* err = NULL;
        regex = g_regex_new (expr.c_str(), (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, &err);
        if (err != NULL) {
            g_warning("Failed to compile regexp: %s", err->message);
            g_error_free(err);
            regex = NULL;
            return false;
        }
    }
    return true;
}

void GRegexReplace::findMatches(const string& in, vector<PatternMatch>& matches, int flags)
{
    if (!regex) {
        compile();
    }

    if (regex) {
        GError* error = NULL;
        GMatchInfo* info;
        const char* buff = in.c_str();
        gint lastMatchEndBytes = 0;
        gint lastMatchEndChars = 0;
        g_regex_match_full (regex, buff, -1, 0, (GRegexMatchFlags)0, &info, &error);
        while (g_match_info_matches (info)) {
            // find the subexpression that matched so we know what flag to store
            vector<TextReplacementRule>::iterator it;
            for (it = rules.begin(); it != rules.end(); it++) {
                TextReplacementRule& r = *it;
                const char* const exprName = r.name.c_str();
                gchar* match = g_match_info_fetch_named (info, exprName);
                if (match) {
                    if ((*match != 0) && (r.flagMask & flags)) {
                        // There is a match.  Determine the coordinates in characters
                        // relative to the last match and store the information.
                        PatternMatch m;
                        gint startInBytes = 0;
                        gint endInBytes = 0;
                        m.type = r.flagMask;
                        g_match_info_fetch_named_pos(info, exprName, &startInBytes, &endInBytes);
                        m.start = lastMatchEndChars +
                            g_utf8_strlen(buff + lastMatchEndBytes, startInBytes - lastMatchEndBytes);
                        m.end = m.start +
                            g_utf8_strlen(buff + startInBytes, endInBytes - startInBytes);
                        matches.push_back(m);

                        // update the coordinates of the last match
                        lastMatchEndBytes = endInBytes;
                        lastMatchEndChars = m.end;
                    }
                    g_free(match);
                }
            }

            // find the next match (if any)
            g_match_info_next (info, &error);
        }
        g_match_info_free (info);
        if (error != NULL) {
            g_printerr ("Error while matching: %s\n", error->message);
            g_error_free (error);
        }
    }
}

gchar* GRegexReplace::processText(const char* in, int flags)
{
    if (!regex) {
        compile();
    }

    if (regex) {
        GError* err = NULL;
        this->flags = flags;
        gchar* res = g_regex_replace_eval (regex, in, -1, 0, (GRegexMatchFlags) 0, evalCallback, this, &err);
        if (err != NULL) {
            g_warning("Failed to process: %s", err->message);
            g_error_free(err);
        }
        return res;
    }
    // no regex
    return NULL;
}

string GRegexReplace::processTextString(const string& text, int flags)
{
    string s;
    gchar* result = processText(text.c_str(), flags);
    if (result) {
        s = result;
        g_free(result);
    }
    return s;
}

void GRegexReplace::evaluateMatch(const GMatchInfo* info, GString* res)
{
    vector<TextReplacementRule>::iterator it;
    for (it = rules.begin(); it != rules.end(); it++) {
        TextReplacementRule& r = *it;
        gchar* match = g_match_info_fetch_named (info, r.name.c_str());
        if (match && (*match != 0)) {
            //g_message("Match - %s: %s", r.name.c_str(), match);
            if (flags & r.flagMask) {
                // rewrite
                GError* err = NULL;
                gchar* rewrite = g_match_info_expand_references(info, r.replacement.c_str(), &err);
                if (err) {
                    g_warning("Failed to generate replacement: %s", err->message);
                    g_error_free(err);
                }
                g_string_append(res, rewrite);
                g_free(rewrite);
            } else {
                // pass it through if not enabled
                g_string_append(res, match);
            }
            g_free(match);
            return;
        }
    }
}

gboolean GRegexReplace::evalCallback(const GMatchInfo* info, GString* res, gpointer data)
{
    GRegexReplace* indexer = (GRegexReplace*) data;
    if (indexer) {
        indexer->evaluateMatch(info, res);
    }
    return false;
}

} // namespace WebCore

