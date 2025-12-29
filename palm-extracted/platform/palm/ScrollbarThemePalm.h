

#ifndef ScrollbarThemeGtk_h
#define ScrollbarThemeGtk_h

#include "ScrollbarTheme.h"

namespace WebCore {

class ScrollbarThemePalm : public ScrollbarTheme {
public:
    virtual ~ScrollbarThemePalm();

    virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar);
};

}
#endif
