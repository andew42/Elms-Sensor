#include <Wire.h>
#include <sensor_driver.h>
#include <bsec.h>

class Bme680Driver : SensorDriver
{
public:
    // Creates a driver instance for each physical device found starting at
    // firstInstance. Creates no more than maxInstances. Returns the number
    // of instances created.
    static int CreateDriverInstances(TwoWire *wire, SensorDriver *firstInstance[], int maxInstances, float trim1, float trim2 = 0.0f);
    int GetPacketData(char *ptr);
    void Handle();
    bool IsLastReadingValid() {return _lastReadingValid;}
    void GetValues(void callback(const char *, const char *));
    void Recalibrate();

private:
    TwoWire *_i2c;
    int _address;
    char _id[14];
    Bsec _iaqSensor;
    float _lastTemp;
    float _lastPressure;
    float _lastHumidity;
    float _lastIaq;
    uint8_t _lastIaqAccuracy;
    float _lastCo2Equivalent;
    bool _lastReadingValid = false;
    uint32_t _lastSaveMs = 0;
    float _trim;

    Bme680Driver(TwoWire *i2c, int address, const char *prefix, float trim);
    bool IsBadStatus(const char *str);
};
