#include <Arduino.h>
#include <si705_driver.h>

// *** PUBLIC ***

// Scan for device and create a driver if found
int Si705Driver::CreateDriverInstances(TwoWire *i2c, SensorDriver *firstInstance[], int maxInstances)
{
    if (maxInstances < 1)
        return 0;
    int instances = 0;

    // Si705 can be at 0x40
    i2c->beginTransmission(0x40);
    uint8_t e = i2c->endTransmission();
    Serial.printf("Si705Driver endTransmission %i\n", e);
    if (e == 0)
        firstInstance[instances++] = new Si705Driver(i2c, 0x40);
    return instances;
}

int Si705Driver::GetPacketData(char *ptr)
{
    char t[16];
    dtostrf((double)_lastReadingCelsius, 1, 4, t);
    // temperature,id=SLc25732 value=29.5556
    return sprintf(ptr, "temperature,id=%s value=%s\n", _id, t);
}

void Si705Driver::Handle()
{
    // Take a temprature reading every 5 seconds
    if ((unsigned long)(millis() - _lastPollMillis) < 5000)
        return;

    _lastPollMillis = millis();

    // Read last temprature (avoid reading garbage on first call)
    if (_conversionStarted)
    {
        _i2c->requestFrom(_address, 2);
        byte msb = _i2c->read();
        byte lsb = _i2c->read();
        uint16_t val = msb << 8 | lsb;
        _lastReadingCelsius = (175.72 * val) / 65536 - 46.85;

        // Sanity check
        _lastReadingValid = _lastReadingCelsius >= MIN_SANE_VALUE && _lastReadingCelsius <= MAX_SANE_VALUE;
    }

    // Start next conversion
    _i2c->beginTransmission(_address);
    _i2c->write(0xF3);
    _i2c->endTransmission();
    _conversionStarted = true;

    // Debug output
    Serial.print(_id);
    Serial.println(" updated");
}

void Si705Driver::GetValues(void cb(const char *, const char *))
{
    char val[64];
    // Call back with name value pairs
    cb("Device", _chipType);
    sprintf(val, " %#x", _address);
    cb("Address", val);
    cb("Id", _id);

    if (!_lastReadingValid)
    {
        cb("ERROR", InsaneTemprature);
        return;
    }

    cb("Temprature (C)", dtostrf((double)_lastReadingCelsius, 1, 4, val));
}

// *** PRIVATE ***

// Max resolution is 14bits
void Si705Driver::set14BitResolution()
{
    _i2c->beginTransmission(_address);
    _i2c->write(0xE6);
    _i2c->write(0);
    _i2c->endTransmission();
}

// 50 = 0x32 = Si7050
// 51 = 0x33 = Si7051
// 53 = 0x35 = Si7053
// 54 = 0x36 = Si7054
// 55 = 0x37 = Si7055
uint8_t Si705Driver::readChipType()
{
    _i2c->beginTransmission(_address);
    _i2c->write(0xfc);
    _i2c->write(0xc9);
    _i2c->endTransmission();
    _i2c->requestFrom(_address, 6);
    int chipType = _i2c->read();
    for (int i = 0; i < 5; i++)
        _i2c->read();
    return chipType;
}

// 0xFF = Firmware version 1.0
// 0x20 = Firmware version 2.0
uint8_t Si705Driver::readFirmwareVersion()
{
    _i2c->beginTransmission(_address);
    _i2c->write(0x84);
    _i2c->write(0xB8);
    _i2c->endTransmission();
    _i2c->requestFrom(_address, 1);
    return _i2c->read();
}

// Construct a driver for a Si7051 device at address
Si705Driver::Si705Driver(TwoWire *i2c, int address)
{
    _i2c = i2c;
    _address = address;

    // Get chip type
    sprintf(_chipType, "Si705%c", (readChipType() - 0x32) + '0');

    // Unique id is SL - ESP8266 id
    sprintf(_id, "SL%x", ESP.getChipId());

    // 14-bit measurement resolution
    set14BitResolution();

    // Read firmware version
    switch (readFirmwareVersion())
    {
    case 0xFF:
        _firmwareVersion = "1";
        break;
    case 0x20:
        _firmwareVersion = "2";
        break;
    default:
        _firmwareVersion = "?";
        break;
    }
}
