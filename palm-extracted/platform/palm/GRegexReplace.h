#ifndef __GRegexReplace_h__
#define __GRegexReplace_h__

#include <vector>
#include <string>
#include <glib.h>

namespace WebCore
{

struct TextReplacementRule;
struct PatternMatch;

/**
 * Thin wrapper around the glib regex functionality.  Documentation about the api
 * can be found at http://developer.gnome.org/glib/2.28/glib-Perl-compatible-regular-expressions.html.
 * To enable a single expression being used to scan for multiple patterns, the named
 * capture groups feature is used.  Each named group is treated as a rule to match and
 * all of the named expressions are combined into one regular expression using the
 * alternation operator "|".
 * <p>
 * The original text indexer assigned different search patterns to a bit within a bit field.
 * The named capture groups are therefore assigned a bit mask to represent the kind of
 * search the sub-expression represents.  The bit can be used to enable/disable replacement
 * of matches on the sub-expression.  This does not eliminate the sub-expression from the
 * overall expression, which is compiled as soon as an attempt is made to match/replace text.
 */
class GRegexReplace
{
public:
    /**
     * Construct an indexer with a default set of rules for normal operation.
     */
    GRegexReplace();
    virtual ~GRegexReplace();


    /**
     * Find all of the subexpression matches in the given utf8 encoded string.
     * The location of the match and the bit (flag) associated with the kind of pattern
     * will be stored in the given matches argument.
     */
    virtual void findMatches(const std::string& in,
                std::vector<PatternMatch>& matches, int flags = 0xFFFF);

    /**
     * Take a utf8 encoded string and apply the regular expression against it repeatedly
     * substituting any match with the output pattern which can include the named backreference
     * of the match.  The given flags are combined with the mask of the sub expression
     * to determine if a replacement should be performed.  A value of 0 would therefore do
     * no replacements.   The resulting string with replacements is returned, also in utf8
     * encoding.
     */
    virtual std::string processTextString(const std::string& text, int flags = 0xFFFF);

    /**
     * Add a named capture group to represent a sub-expression to match against.  The resulting
     * expression will contain the sub expressions in the order they were added, separated by
     * the alternation operator "|".
     */
    void addRule(const TextReplacementRule& rule);

    /**
     * Combine all of the rules into a regular expression and compile it
     * for future text processing.
     *
     * @return true if the regular expression was successfully compiled,
     * false otherwise.
     */
    virtual bool compile();

protected:

    virtual gchar* processText(const char* in, int flags);

    virtual void evaluateMatch(const GMatchInfo* info, GString* res);

private:
    static gboolean evalCallback(const GMatchInfo* info, GString* res, gpointer data);

protected:
    std::vector<TextReplacementRule> rules;
    int flags;

private:
    GRegex* regex;
};

struct TextReplacementRule
{
    std::string name;
    std::string expression;
    std::string replacement;
    guint32 flagMask;
};

/**
 * Structure holding pattern match location information.
 *
 * The structure used to be called GeneratedLink but it is really a pattern match and something
 * like an emoticon is a graphical represention of a plain text pattern (which could be used
 * for other things as well).
 */
struct PatternMatch
{
    PatternMatch() : start(0), end(0), type(0) { }
    ~PatternMatch() { }
    PatternMatch(const PatternMatch& c) {
        start = c.start;
        end = c.end;
        type = c.type;
    }
    PatternMatch& operator=(const PatternMatch& c) {
        if (this == &c)
            return *this;
        start = c.start;
        end = c.end;
        type = c.type;
        return *this;
    }
    int start;
    int end;
    int type;
    int len() const { return end-start; }
};

}

#endif // __GRegexReplace_h__
