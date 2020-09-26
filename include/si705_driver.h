#include <Wire.h>
#include <sensor_driver.h>

class Si705Driver : SensorDriver
{
public:
    // Creates a driver instance for each physical device found starting at
    // firstInstance. Creates no more than maxInstances. Returns the number
    // of instances created.
    static int CreateDriverInstances(TwoWire *wire, SensorDriver *firstInstance[], int maxInstances);
    int GetPacketData(char *ptr);
    void Handle();
    bool IsLastReadingValid() { return _lastReadingValid; }
    void GetValues(void cb(const char *, const char *));

private:
    TwoWire *_i2c;
    int _address;
    char _chipType[8];
    char _id[16];
    const char *_firmwareVersion;
    float _lastReadingCelsius;
    bool _lastReadingValid = false;
    bool _conversionStarted = false;
    long _lastPollMillis = 0;

    Si705Driver(TwoWire *i2c, int address);
    void set14BitResolution();
    uint8_t readChipType();
    uint8_t readFirmwareVersion();
};
