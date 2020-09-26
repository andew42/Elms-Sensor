#include <sensor_driver.h>

class LdrDriver : SensorDriver
{
public:
    // Creates a driver instance for witty cloud LDR
    static int CreateDriverInstances(SensorDriver *firstInstance[], int maxInstances);
    int GetPacketData(char *ptr);
    void Handle() {}
    bool IsLastReadingValid() { return true; }
    void GetValues(void cb(const char *, const char *));

private:
    char _id[16];
    int _lastReading = 0;
    LdrDriver();
};
