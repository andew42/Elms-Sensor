#include <Wire.h>
#include <sensor_driver.h>

class Bh1750Driver : SensorDriver
{
public:
    // Creates a driver instance for each physical device found starting at
    // firstInstance. Creates no more than maxInstances. Returns the number
    // of instances created.
    static int CreateDriverInstances(TwoWire *wire, SensorDriver *firstInstance[], int maxInstances);
    int GetPacketData(char *ptr);
    void Handle();
    bool IsLastReadingValid() { return _lastLux < 0xffff; }
    void GetValues(void callback(const char *, const char *));

private:
    TwoWire *_i2c;
    int _address;
    char _id[14];
    uint16_t _lastLux = 0xffff;
    long _lastPollMillis = 0;

    Bh1750Driver(TwoWire *i2c, int address);
};
