
#include "config.h"
#include "ScrollbarThemePalm.h"


namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemePalm theme;
    return &theme;
}

ScrollbarThemePalm::~ScrollbarThemePalm()
{
}

int ScrollbarThemePalm::scrollbarThickness(ScrollbarControlSize controlSize)
{
	return 0;
}

}

