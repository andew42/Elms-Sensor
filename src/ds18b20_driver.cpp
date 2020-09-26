#include <Arduino.h>
#include <ds18b20_driver.h>

// *** PUBLIC ***

// Scan for devices and create a driver for each found device
int Ds18b20Driver::CreateDriverInstances(OneWire *wire, SensorDriver *firstInstance[], int maxInstances)
{
    int instances = 0;
    byte addr[8];
    while ((instances < maxInstances) && wire->search(addr))
    {
        // Check CRC
        if (OneWire::crc8(addr, 7) != addr[7])
        {
            Serial.println("CRC invalid");
            continue;
        }

        // Ignore devices that arn't 18B20
        if (addr[0] != 0x28)
        {
            Serial.println("Ignoring unknown OneWire device");
            continue;
        }

        // Create a driver for this device
        firstInstance[instances++] = new Ds18b20Driver(wire, addr);
    }
    return instances;
}

int Ds18b20Driver::GetPacketData(char *ptr)
{
    char t[16];
    dtostrf((double)_lastReadingCelsius, 1, 4, t);
    // temperature,id=ffb897721503 value=30.3750
    return sprintf(ptr, "temperature,id=%s value=%s\n", _id, t);
}

void Ds18b20Driver::Handle()
{
    // Take a temprature reading every 5 seconds
    if ((unsigned long)(millis() - _lastPollMillis) < 5000)
        return;

    _lastPollMillis = millis();

    // 1) Read result of conversion
    _wire->reset();
    _wire->select(_address);
    _wire->write(0xBE); // Read Scratchpad
    byte data[12];
    for (int i = 0; i < 9; i++)
    { // we need 9 bytes
        data[i] = _wire->read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
        raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
        raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
        raw = raw & ~1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
    _lastReadingCelsius = (float)raw / 16.0;

    // Sanity check
    _lastReadingValid = _lastReadingCelsius >= MIN_SANE_VALUE && _lastReadingCelsius <= MAX_SANE_VALUE;

    // 2) Start next conversion
    _wire->reset();
    _wire->select(_address);
    _wire->write(0x44, 0);

    // 3) Debug output
    Serial.print(_id);
    Serial.println(" updated");
}

void Ds18b20Driver::GetValues(void cb(const char *, const char *))
{
    char val[64];
    // Call back with name value pairs
    cb("Device", "DS18B20");
    cb("Id", _id);

    if (!_lastReadingValid)
    {
        cb("ERROR", InsaneTemprature);
        return;
    }

    cb("Temprature (C)", dtostrf((double)_lastReadingCelsius, 1, 4, val));
}

// *** PRIVATE ***

// Construct a driver for a 18B20 device at address
Ds18b20Driver::Ds18b20Driver(OneWire *wire, byte address[8])
{
    _wire = wire;
    memcpy(_address, address, 8);
    for (int i = 0; i < 6; i++)
        sprintf(&_id[i * 2], "%02x", address[i + 1]);
}
