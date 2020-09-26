#include <OneWire.h>
#include <sensor_driver.h>

class Ds18b20Driver : SensorDriver
{
public:
    // Creates a driver instance for each physical device found starting at
    // firstInstance. Creates no more than maxInstances. Returns the number
    // of instances created.
    static int CreateDriverInstances(OneWire *wire, SensorDriver *firstInstance[], int maxInstances);
    int GetPacketData(char *ptr);
    void Handle();
    bool IsLastReadingValid() {return _lastReadingValid;}
    void GetValues(void callback(const char *, const char *));

private:
    OneWire *_wire;
    byte _address[8];
    char _id[14];
    float _lastReadingCelsius;
    long _lastPollMillis = 0;
    bool _lastReadingValid = false;

    Ds18b20Driver(OneWire *wire, byte address[8]);
};
