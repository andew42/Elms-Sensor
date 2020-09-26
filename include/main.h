#ifndef MAIN_H
#define MAIN_H

#include <ezTime.h>
#include <Arduino.h>
#include "sensor_driver.h"

#define POLL_PERIOD_MS 20000

#define MAX_SENSOR_DRIVERS 8
extern int drivers_count;
extern SensorDriver *drivers[MAX_SENSOR_DRIVERS];

#define MAX_STARTUP_LOG_ENTRIES 5
struct startupEntry
{
    time_t time;
    uint32 reason;
};
extern Timezone myTZ;
extern startupEntry startupLog[MAX_STARTUP_LOG_ENTRIES];

extern const char *hostname;

#endif // MAIN_H
