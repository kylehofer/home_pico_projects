#ifndef PROJECTS_GARDEN_BED_SRC_BED
#define PROJECTS_GARDEN_BED_SRC_BED

#include "Node.h"
#include "metrics/simple/UInt8Metric.h"
#include "metrics/simple/UInt16Metric.h"
#include "metrics/simple/BooleanMetric.h"

using namespace Sparkplug;

enum class LightState
{
    OFF,
    ON,
    TRANSITION
};

class Bed
{
private:
    uint16_t target = 0;
    uint16_t current = 0;

    time_t transitionStart;
    time_t transitionEnd;
    LightState state = LightState::OFF;

    std::shared_ptr<BooleanMetric> forceOn;
    std::shared_ptr<BooleanMetric> lightStatus;

    std::shared_ptr<UInt8Metric> startHour;
    std::shared_ptr<UInt8Metric> startMinute;

    std::shared_ptr<UInt8Metric> endHour;
    std::shared_ptr<UInt8Metric> endMinute;

    std::shared_ptr<UInt8Metric> intensity;
    std::shared_ptr<UInt16Metric> transitionTime;

protected:
public:
    Bed(Node *parent);
    void sync(const time_t &time);
};

#endif /* PROJECTS_GARDEN_BED_SRC_BED */
