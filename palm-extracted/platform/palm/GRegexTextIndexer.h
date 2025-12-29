#ifndef __GRegexTextIndexer_h__
#define __GRegexTextIndexer_h__

#include <vector>
#include <string>
#include <glib.h>
#include <GRegexReplace.h>

namespace WebCore
{

/**
 * A data driven mechanism to find patterns in plain text and optionally convert them
 * to an html fragment.  This implementation is based upon the glib regular expression
 * library, which has an api expressed in terms of utf8.
 */
class GRegexTextIndexer : public GRegexReplace
{
public:
    /**
     * Construct an indexer with a default set of rules for normal operation.
     */
    GRegexTextIndexer();

    /**
     * Combine all of the rules into a regular expression and compile it
     * for future text processing.  This is implemented to resolve all of the
     * static backreferences in the replacement patterns.  An example is the
     * imagePath which specifies where to find the images for emoticons.
     *
     * @return true if the regular expression was successfully compiled,
     * false otherwise.
     */
    bool compile();

    /**
     * Set the value to be used for referencing images.  This will be used to
     * fill in named back references of the form \g<imagePath>
     */
    void setImagePath(const std::string& path);

    /**
     * Get the value to be used for referencing images.  This will be used to
     * fill in named back references of the form \g<imagePath>
     */
    std::string getImagePath();

private:
    std::string imagePath;
};

} // namespace WebCore

#endif // __GRegexTextIndexer_h__
