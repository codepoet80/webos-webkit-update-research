#include "config.h"
#include "Length.h"


namespace WebCore {

	int Length::calcValue(int maxValue, float scale, bool roundPercentages) const
    {
        switch (type()) {
            case Fixed:
                return scale * value();
            case Percent:
                if (roundPercentages)
                    return static_cast<int>(round(maxValue * percent() / 100.0));
                return maxValue * rawValue() / (100 * percentScaleFactor);
            case Auto:
                return maxValue;
            default:
                return undefinedLength;
        }
    }
} // namespace WebCore
