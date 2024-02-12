#ifndef SHED
#define SHED

#include "Publishable.h"
#include "metrics/simple/UInt8Metric.h"
#include "metrics/simple/BooleanMetric.h"

class Shed
{
private:
    uint lightOutSlice;
    uint16_t lightPwmWrap;
    uint8_t lightIntensity = 25;
    uint lightPwmChannel;

    std::shared_ptr<BooleanMetric> doorOpen;
    std::shared_ptr<UInt8Metric> intensity;

protected:
public:
    Shed(Publishable *parent);
    void sync();
};

#endif /* SHED */
