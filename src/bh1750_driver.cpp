#include <Arduino.h>
#include <bh1750_driver.h>

// *** PUBLIC ***

// Scan for device and create a driver if found
int Bh1750Driver::CreateDriverInstances(TwoWire *i2c, SensorDriver *firstInstance[], int maxInstances)
{
    if (maxInstances < 1)
        return 0;
    int instances = 0;

    // Bh1750 can be at 0x23 (or 0x5C) configure hires continous
    i2c->beginTransmission(0x23);
    // Measurement at 1 lux resolution, measurement time 120ms
    i2c->write((int8_t)0x10);
    uint8_t e = i2c->endTransmission();
    Serial.printf("Bh1750 endTransmission %i\n", e);
    if (e == 0)
    {
        firstInstance[instances++] = new Bh1750Driver(i2c, 0x23);
        delay(10);
    }
    return instances;
}

int Bh1750Driver::GetPacketData(char *ptr)
{
    char t[16];
    itoa(_lastLux, t, 10);
    // lux,id=BHc25732 value=191
    return sprintf(ptr, "lux,id=%s value=%s\n", _id, t);
}

void Bh1750Driver::Handle()
{
    // Take a light reading every 5 seconds
    if ((unsigned long)(millis() - _lastPollMillis) < 5000)
        return;

    _lastPollMillis = millis();

    // Read the lux value
    if (_i2c->requestFrom(_address, 2) == 2)
    {
        _lastLux = (_i2c->read() << 8) | _i2c->read();
        // Compensate for window as per data sheet
        _lastLux = _lastLux / 1.2f;
    }
    else
        _lastLux = 0xffff;

    // Debug output
    Serial.print(_id);
    Serial.println(" updated");
}

void Bh1750Driver::GetValues(void cb(const char *, const char *))
{
    char val[64];
    // Call back with name value pairs
    cb("Device", "BH1750");
    sprintf(val, " %#x", _address);
    cb("Address", val);
    cb("Id", _id);

    if (!IsLastReadingValid())
    {
        cb("ERROR", "Insane LUX");
        return;
    }

    cb("Light Intensity (Lux)", itoa(_lastLux, val, 10));
}

// *** PRIVATE ***

// Construct a driver for a Bh1750Driver device at address
Bh1750Driver::Bh1750Driver(TwoWire *i2c, int address)
{
    _i2c = i2c;
    _address = address;

    // Unique id is BH - ESP8266 id
    sprintf(_id, "BH%x", ESP.getChipId());
}
