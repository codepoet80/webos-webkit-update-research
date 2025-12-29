
#include <GRegexTextIndexer.h>
#include <palmwebglobal.h>

using namespace std;

namespace WebCore {

/*
 * The following regular expressions drive the text indexing.  There is an OReilly book that is a regular
 * expression cookbook and has two authors with websites containing some useful conversation about trying
 * to match some of the patterns we care about.  The following links may be useful:
 *
 * http://blog.stevenlevithan.com/archives/validate-phone-number
 * http://www.regular-expressions.info/email.html
 *
 */

static const char* const phone =
"(((\\+\\d{1,3}[-. ]?\\(?\\d\\)?[-. ]?\\d{1,5})|(\\(?\\d{2,6}\\)?))[-. ]?)?"
"(\\d{3,4})[-. ]?(\\d{4})(( x| ext)?\\d{1,5}){0,1}\\b"
;

/*
 * The following are from RFC3986 which defines the general URI syntax, which can be
 * found at http://tools.ietf.org/html/rfc3986.  A lot of the pieces are useful for
 * attempting to match particular schemes or any URI.
 */
static const string unreserved = "[-a-z0-9._~]";
static const string sub_delims = "[!$&'()*+,;=]";
static const string pct_encoded = "%[0-9a-f][0-9a-f]";
static const string pchar = unreserved + "|" + pct_encoded + "|" + sub_delims + "|" + "[:@]";
static const string path = "(/("+pchar+")*)*";
static const string query = "(\\?("+pchar+"|[?/])*)?";
static const string fragment = "(#("+pchar+ "|[?/])*)?";

static const string scheme = "[a-z][-a-z0-9+.]*";
static const string userinfo = "((" + unreserved + "|" + pct_encoded + "|" + sub_delims + "|:)*@)?";
static const string port = "(:\\d+)?";
static const string IPv4address = "((\\d|[1-9]\\d|1\\d{2}|2[0-4][0-9]|25[0-5])\\.){3}(\\d|[1-9]\\d|1\\d{2}|2[0-4][0-9]|25[0-5])";
static const string reg_name = "\\b(" + unreserved + "|" + pct_encoded + "|" + sub_delims + ")+\\b";
static const string host = "\\b(" + IPv4address + "|" + reg_name + ")\\b";
static const string authority = userinfo + host + port;
static const string path_rootless = "(" + pchar + ")+" + path;
static const string hier_part = "(//" + authority + path + "\\b|" + path + "|" + path_rootless + ")";

/*
 * Since attempting to match a schemeless url is pretty tricky, we try to be highly restrictive on the
 * tail end of domain by matching only well-known endings (except for the two letter variations).  This
 * gives a fairly solid anchor for matching something that really is a URL.  The URI parts are used after
 * encountering a domain or ip number.  Since numbers are included in word boundaries, a word boundary is
 * required at the start and end of the host to further constrain the potential match.
 */
static const string schemelessDomain = string("([a-z])+(\\.[a-z]+)*") + // restrictive domain segments
    "\\.((com|edu|org|net|gov|biz|mobi|info|int|info|museum|asia|pro|tel|travel)|[a-z]{2})"; // top level domain
static const string schemelessHost =  "\\b(" + IPv4address + "|" + schemelessDomain + ")\\b";
static const string schemelessUrl = schemelessHost + port + path + query + fragment;

/*
 * For links, the scheme is pretty conservative at the moment.
 */

static const string linkScheme = "\\b(https?|ftp|file)\\b";
static const string linkUri = linkScheme + ":" + hier_part + query + fragment;

/*
 * The regular expressions that drive the text indexer. Entries with no flags will also be
 * ignored when finding matches.
 */
static TextReplacementRule defaultRules[] = {
    /*
     * The first two expressions allow the indexer to be run on already indexed html.  They will eat all anchors
     * and tags to pass through to the output.  Entries with no flags will also be ignored when finding matches.
     *
     * The indexer isn't normally run on already linked content.  The two expression that support
     * this behavior conflict with the common practice of embedding uri between angle brackets.
     * There should probably be a programatic option to add these two expressions.
    {
        "anchor",
        "(?P<anchor>(?i)<a([^>]*)>(.*?)</a>)",
        "\\g<anchor>",// passthrough
        0x0100
    },
    {
        "tag",
        "(?P<tag>(?i)<[a-z][a-z0-9]*[^>]*>)",
        "\\g<tag>",// passthrough
        0x0100
    },
    */
    {
        "phone",
        string("(?P<phone>") + phone + ")",
        "<a href=\"tel:\\g<phone>\">\\g<phone></a>",
        Palm::TextIndexer_PhoneNumber
    },
    {
        "email",
        "(?P<email>(?i)\\b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,4}\\b)",
        "<a href=\"mailto:\\g<email>\">\\g<email></a>",
        Palm::TextIndexer_EmailAddress
    },

    {
        "link",
        "(?P<link>(?i)" + linkUri + ")",
        "<a href=\"\\g<link>\">\\g<link></a>",
        Palm::TextIndexer_WebLink
    },

    {
        "schemeless",
        string("(?P<schemeless>(?i)\\b") + schemelessUrl + ")",
        "<a href=\"http://\\g<schemeless>\">\\g<schemeless></a>",
        Palm::TextIndexer_SchemalessWebLink
    },
    {
        "float",
        "(?P<float>[0-9]*\\.[0-9]+)",
        "\\g<float>",// passthrough
        0x0000
    },

    {
        "grin",
        "(?P<grin>\\^\\^|\\^_\\^|\\^-\\^|:grin\\b|:biggrin\\b)",
        "<img src=\"<imagePath>/emoticon-grin.png\" alt=\"\\g<grin>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "smile",
        "(?P<smile>:\\)|:-\\)|=\\)|=-\\)|:smile\\b)",
        "<img src=\"<imagePath>/emoticon-smile.png\" alt=\"\\g<smile>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "confused",
        "(?P<confused>o_O\\b|:confused\\b|:wtf\\b)",
        "<img src=\"<imagePath>/emoticon-confused.png\" alt=\"\\g<confused>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "cool",
        "(?P<cool>\\b8\\)|\\b8-\\)|\\bB\\)|\\bB-\\)|:cool\\b|\\(H\\))",
        "<img src=\"<imagePath>/emoticon-cool.png\" alt=\"\\g<cool>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "frown",
        "(?P<frown>:\\(|:-\\(|=\\(|=-\\(|:sad\\b)",
        "<img src=\"<imagePath>/emoticon-frown.png\" alt=\"\\g<frown>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "gasp",
        "(?P<gasp>:O\\b|:-O\\b|:omg\\b|:o|:-o\\b|=O\\b|=-O\\b|=o\\b|=-o\\b|:surprised\\b|:shock\\b)",
        "<img src=\"<imagePath>/emoticon-gasp.png\" alt=\"\\g<gasp>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "footinmouth",
        "(?P<footinmouth>:!|:-!|:eek\\b)",
        "<img src=\"<imagePath>/emoticon-footinmouth.png\" alt=\"\\g<footinmouth>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "cry",
        "(?P<cry>:'\\(|='\\(|:cry\\b)",
        "<img src=\"<imagePath>/emoticon-cry.png\" alt=\"\\g<cry>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "kiss",
        "(?P<kiss>:-\\*|:\\*|=\\*|=-\\*|:kiss\\b)",
        "<img src=\"<imagePath>/emoticon-kiss.png\" alt=\"\\g<kiss>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "neutral",
        "(?P<neutral>:\\||:-\\||:neutral\\b|:meh\\b)",
        "<img src=\"<imagePath>/emoticon-neutral.png\" alt=\"\\g<neutral>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "heart",
        "(?P<heart>&lt;3|:heart\\b)",
        "<img src=\"<imagePath>/emoticon-heart.png\" alt=\"\\g<heart>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "innocent",
        "(?P<innocent>\\bO:\\)|\\bO:-\\)|\\bo:\\)|\\bo:-\\)|:innocent\\b|:angel\\b)",
        "<img src=\"<imagePath>/emoticon-innocent.png\" alt=\"\\g<innocent>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "wink",
        "(?P<wink>;\\)|;-\\)|:wink\\b)",
        "<img src=\"<imagePath>/emoticon-wink.png\" alt=\"\\g<wink>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "yuck",
        "(?P<yuck>:P\\b|:-P\\b|:p\\b|:-p\\b|:b\\b|:-b\\b|=p\\b|=P\\b|=b\\b|=-b\\b|=-p\\b|=-P\\b|:yuck\\b|:razz\\b)",
        "<img src=\"<imagePath>/emoticon-yuck.png\" alt=\"\\g<yuck>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "naughty",
        "(?P<naughty>&gt;:-\\)|&gt;:\\)|&gt;:-&gt;|&gt;:&gt;|:evil\\b|:twisted\\b)",
        "<img src=\"<imagePath>/emoticon-naughty.png\" alt=\"\\g<naughty>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "angry",
        "(?P<angry>&gt;:o\\b|&gt;:-o\\b|&gt;:O\\b|&gt;:-O\\b|&gt;:\\(|&gt;:-\\(|:angry\\b|:mad\\b)",
        "<img src=\"<imagePath>/emoticon-angry.png\" alt=\"\\g<angry>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "embarrassed",
        "(?P<embarrassed>:\\[|:-\\[|=\\[|=-\\[|:redface\\b|:\\$)",
        "<img src=\"<imagePath>/emoticon-embarrassed.png\" alt=\"\\g<embarrassed>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "undecided",
        "(?P<undecided>:/|:-/|:\\\\|:-\\\\|=/|=-/|=\\\\|=-\\\\|:doh\\b)",
        "<img src=\"<imagePath>/emoticon-undecided.png\" alt=\"\\g<undecided>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "sick",
        "(?P<sick>:-&amp;|:&amp;|=&amp;|=-&amp;|:-@|:@|=@|=-@|:sick\\b)",
        "<img src=\"<imagePath>/emoticon-sick.png\" alt=\"\\g<sick>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "eww",
        "(?P<eww>:S\\b|:-S\\b|:s\\b|:-s\\b|%-\\(|%\\(|\\bX-\\(|\\bX\\(|:eww\\b|:gross\\b)",
        "<img src=\"<imagePath>/emoticon-eww.png\" alt=\"\\g<eww>\">",
        Palm::TextIndexer_Emoticon
    },
    {
        "laugh",
        "(?P<laugh>:-D\\b|:D\\b|=D\\b|=-D\\b|:laugh\\b|:lol\\b)",
        "<img src=\"<imagePath>/emoticon-laugh.png\" alt=\"\\g<laugh>\">",
        Palm::TextIndexer_Emoticon
    }
};

GRegexTextIndexer::GRegexTextIndexer()
    : imagePath("file:///usr/palm/emoticons")
{
    int nDefaults = sizeof(defaultRules) / sizeof(TextReplacementRule);
    for (int i = 0; i < nDefaults; i++) {
        addRule(defaultRules[i]);
    }
}

bool GRegexTextIndexer::compile()
{
    TextReplacementRule imageLoc;
    imageLoc.name = "imagePath";
    imageLoc.expression = "(?P<imagePath><imagePath>)",
    imageLoc.replacement = getImagePath();
    imageLoc.flagMask = 0xFF;
    GRegexReplace staticBackReferences;
    staticBackReferences.addRule(imageLoc);

    vector<TextReplacementRule>::iterator it;
    for (it = rules.begin(); it != rules.end(); it++) {
        TextReplacementRule& r = *it;
        r.replacement = staticBackReferences.processTextString(r.replacement);
    }

    return GRegexReplace::compile();
}

void GRegexTextIndexer::setImagePath(const string& path)
{
    imagePath = path;
}

string GRegexTextIndexer::getImagePath()
{
    return imagePath;
}


} // namespace WebCore

