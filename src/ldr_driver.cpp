#include <Arduino.h>
#include <ldr_driver.h>

// *** PUBLIC ***

// Return a driver for LDR
int LdrDriver::CreateDriverInstances(SensorDriver *firstInstance[], int maxInstances)
{
    if (maxInstances < 1)
        return 0;
    firstInstance[0] = new LdrDriver();
    return 1;
}

int LdrDriver::GetPacketData(char *ptr)
{
    _lastReading = analogRead(0);
    char l[16];
    itoa(_lastReading, l, 10);
    // light,id=LDRc25732 value=101
    return sprintf(ptr, "light,id=%s value=%s\n", _id, l);
}

void LdrDriver::GetValues(void cb(const char *, const char *))
{
    char l[16];
    // Call back with name value pairs
    cb("Device", "LDR");
    cb("Id", _id);
    cb("Light Intensity", itoa(_lastReading, l, 10));
}

// *** PRIVATE ***

// Construct a driver for a Si7051 device at address
LdrDriver::LdrDriver()
{
    // Unique id is LDR - ESP8266 id
    sprintf(_id, "LDR%x", ESP.getChipId());
}
