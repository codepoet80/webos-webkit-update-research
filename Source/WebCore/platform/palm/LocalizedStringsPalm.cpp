

#include "config.h"

#include "LocalizedStrings.h"
#include "PlatformString.h"
#include "math.h"

namespace WebCore {

String submitButtonDefaultLabel() { return "Submit"; }
String inputElementAltText() { return String(); }
String resetButtonDefaultLabel() { return "Reset"; }
String searchableIndexIntroduction() { return "Searchable Index"; }
String fileButtonChooseFileLabel() { return "Choose File"; }
String fileButtonNoFileSelectedLabel() { return "No file selected"; }
String contextMenuItemTagOpenLinkInNewWindow() { return "Open in New Window"; }
String contextMenuItemTagDownloadLinkToDisk() { return "Save Link..."; }
String contextMenuItemTagCopyLinkToClipboard() { return "Copy Link"; }
String contextMenuItemTagOpenImageInNewWindow() { return "Open Image"; }
String contextMenuItemTagDownloadImageToDisk() { return "Save Image"; }
String contextMenuItemTagCopyImageToClipboard() { return "Copy Image"; }
String contextMenuItemTagOpenFrameInNewWindow() { return "Open Frame"; }
String contextMenuItemTagCopy() { return "Copy"; }
String contextMenuItemTagGoBack() { return "Go Back"; }
String contextMenuItemTagGoForward() { return "Go Forward"; }
String contextMenuItemTagStop() { return "Stop"; }
String contextMenuItemTagReload() { return "Reload"; }
String contextMenuItemTagCut() { return "Cut"; }
String contextMenuItemTagPaste() { return "Paste"; }
String contextMenuItemTagNoGuessesFound() { return String(); }
String contextMenuItemTagIgnoreSpelling() { return "Ignore"; }
String contextMenuItemTagLearnSpelling() { return "Add To Dictionary"; }
String contextMenuItemTagSearchWeb() { return "Search The Web"; }
String contextMenuItemTagLookUpInDictionary() { return "Look Up In Dictionary"; }
String contextMenuItemTagOpenLink() { return "Open Link"; }
String contextMenuItemTagIgnoreGrammar() { return "Ignore"; }
String contextMenuItemTagSpellingMenu() { return "Spelling"; }
String contextMenuItemTagShowSpellingPanel(bool show) { return String(); }
String contextMenuItemTagCheckSpelling() { return "Check Spelling"; }
String contextMenuItemTagCheckSpellingWhileTyping() { return String(); }
String contextMenuItemTagCheckGrammarWithSpelling() { return String(); }
String contextMenuItemTagFontMenu() { return "Fonts"; }
String contextMenuItemTagBold() { return "Bold"; }
String contextMenuItemTagItalic() { return "Italic"; }
String contextMenuItemTagUnderline() { return "Underline"; }
String contextMenuItemTagOutline() { return "Outline"; }
String contextMenuItemTagWritingDirectionMenu() { return "Direction"; }
String contextMenuItemTagDefaultDirection() { return "Default"; }
String contextMenuItemTagLeftToRight() { return "LTR"; }
String contextMenuItemTagRightToLeft() { return "RTL"; }
String contextMenuItemTagInspectElement() { return "Inspect"; }
String unknownFileSizeText() { return "Unknown"; }
String searchMenuRecentSearchesText() { return String(); }
String searchMenuClearRecentSearchesText() { return String(); }
String searchMenuNoRecentSearchesText() { return String(); }
String contextMenuItemTagTextDirectionMenu() { return String(); }

String AXDefinitionListTermText()
{
    return String::fromUTF8(("term"));
}

String AXDefinitionListDefinitionText()
{
    return String::fromUTF8(("definition"));
}

String AXButtonActionVerb()
{
    return String::fromUTF8(("press"));
}

String AXRadioButtonActionVerb()
{
    return String::fromUTF8(("select"));
}

String AXTextFieldActionVerb()
{
    return String::fromUTF8(("activate"));
}

String AXCheckedCheckBoxActionVerb()
{
    return String::fromUTF8(("uncheck"));
}

String AXUncheckedCheckBoxActionVerb()
{
    return String::fromUTF8(("check"));
}

String AXLinkActionVerb()
{
    return String::fromUTF8(("jump"));
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
}
    
String missingPluginText()
{
    return String::fromUTF8(("Missing Plug-in"));
}

String crashedPluginText()
{
    return String::fromUTF8(("Plug-in Failure"));
}


String multipleFileUploadText(unsigned numberOfFiles)
{
    // FIXME: If this file gets localized, this should really be localized as one string with a wildcard for the number.
    return String::number(numberOfFiles) + String::fromUTF8((" files"));
}

String imageTitle(const String& filename, const IntSize& size)
{
    return String::fromUTF8(("image_title"));
//    GOwnPtr<gchar> string(g_strdup_printf(C_("Title string for images", "%s  (%dx%d pixels)"),
//                                          filename.utf8().data(),
//                                          size.width(), size.height()));
//
//    return String::fromUTF8(string.get());
}

#if ENABLE(VIDEO)

String mediaElementLiveBroadcastStateText() { return "Live Broadcast"; }
String mediaElementLoadingStateText() { return "Loading..."; }

String localizedMediaControlElementString(const String& name)
{
    if (name == "AudioElement")
        return String::fromUTF8(("audio element controller"));
    if (name == "VideoElement")
        return String::fromUTF8(("video element controller"));
    if (name == "MuteButton")
        return String::fromUTF8(("mute"));
    if (name == "UnMuteButton")
        return String::fromUTF8(("unmute"));
    if (name == "PlayButton")
        return String::fromUTF8(("play"));
    if (name == "PauseButton")
        return String::fromUTF8(("pause"));
    if (name == "Slider")
        return String::fromUTF8(("movie time"));
    if (name == "SliderThumb")
        return String::fromUTF8(("timeline slider thumb"));
    if (name == "RewindButton")
        return String::fromUTF8(("back 30 seconds"));
    if (name == "ReturnToRealtimeButton")
        return String::fromUTF8(("return to realtime"));
    if (name == "CurrentTimeDisplay")
        return String::fromUTF8(("elapsed time"));
    if (name == "TimeRemainingDisplay")
        return String::fromUTF8(("remaining time"));
    if (name == "StatusDisplay")
        return String::fromUTF8(("status"));
    if (name == "FullscreenButton")
        return String::fromUTF8(("fullscreen"));
    if (name == "SeekForwardButton")
        return String::fromUTF8(("fast forward"));
    if (name == "SeekBackButton")
        return String::fromUTF8(("fast reverse"));

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaControlElementHelpText(const String& name)
{
    if (name == "AudioElement")
        return String::fromUTF8(("audio element playback controls and status display"));
    if (name == "VideoElement")
        return String::fromUTF8(("video element playback controls and status display"));
    if (name == "MuteButton")
        return String::fromUTF8(("mute audio tracks"));
    if (name == "UnMuteButton")
        return String::fromUTF8(("unmute audio tracks"));
    if (name == "PlayButton")
        return String::fromUTF8(("begin playback"));
    if (name == "PauseButton")
        return String::fromUTF8(("pause playback"));
    if (name == "Slider")
        return String::fromUTF8(("movie time scrubber"));
    if (name == "SliderThumb")
        return String::fromUTF8(("movie time scrubber thumb"));
    if (name == "RewindButton")
        return String::fromUTF8(("seek movie back 30 seconds"));
    if (name == "ReturnToRealtimeButton")
        return String::fromUTF8(("return streaming movie to real time"));
    if (name == "CurrentTimeDisplay")
        return String::fromUTF8(("current movie time in seconds"));
    if (name == "TimeRemainingDisplay")
        return String::fromUTF8(("number of seconds of movie remaining"));
    if (name == "StatusDisplay")
        return String::fromUTF8(("current movie status"));
    if (name == "SeekBackButton")
        return String::fromUTF8(("seek quickly back"));
    if (name == "SeekForwardButton")
        return String::fromUTF8(("seek quickly forward"));
    if (name == "FullscreenButton")
        return String::fromUTF8(("Play movie in fullscreen mode"));

    ASSERT_NOT_REACHED();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    if (!isfinite(time))
        return String::fromUTF8(("indefinite time"));

    int seconds = (int)fabsf(time);
    int days = seconds / (60 * 60 * 24);
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (days) {
        return String::number(days) + String::fromUTF8(" days ") +
                       String::number(hours) + String::fromUTF8(" hours ") +
                       String::number(minutes) + String::fromUTF8(" minutes ") +
                       String::number(seconds) + String::fromUTF8(" seconds");
    }

    if (hours) {
        return String::number(hours) + String::fromUTF8(" hours ") +
                       String::number(minutes) + String::fromUTF8(" minutes ") +
                       String::number(seconds) + String::fromUTF8(" seconds");
    }

    if (minutes) {
               return String::number(minutes) + String::fromUTF8(" minutes ") +
                       String::number(seconds) + String::fromUTF8(" seconds");
    }

       return String::number(seconds) + String::fromUTF8(" seconds");
}
#endif  // ENABLE(VIDEO)

String validationMessageValueMissingText()
{
    return String::fromUTF8(("value missing"));
}

String validationMessageTypeMismatchText()
{
    return String::fromUTF8(("type mismatch"));
}

String validationMessagePatternMismatchText()
{
    return String::fromUTF8(("pattern mismatch"));
}

String validationMessageTooLongText()
{
    return String::fromUTF8(("too long"));
}

String validationMessageRangeUnderflowText()
{
    return String::fromUTF8(("range underflow"));
}

String validationMessageRangeOverflowText()
{
    return String::fromUTF8(("range overflow"));
}

String validationMessageStepMismatchText()
{
    return String::fromUTF8(("step mismatch"));
}



} // WebCore

