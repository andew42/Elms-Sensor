#ifndef SENSORDRIVER_H
#define SENSORDRIVER_H

#define MIN_SANE_VALUE -40
#define MAX_SANE_VALUE 60

class SensorDriver
{
public:
    virtual int GetPacketData(char *ptr) = 0;
    virtual void Handle() = 0;
    virtual bool IsLastReadingValid() = 0;
    virtual void GetValues(void callback(const char *, const char *)) = 0;
    virtual void Recalibrate() {}

protected:
    const char *InsaneTemprature = "Reported temprature is outside sane range";
};

#endif // SENSORDRIVER_H
