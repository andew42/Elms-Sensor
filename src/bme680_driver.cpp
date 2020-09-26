#include <Arduino.h>
#include <LittleFS.h>
#include <bme680_driver.h>

const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_28d/bsec_iaq.txt"
};

// Save sensor state every 12 hours
#define SAVE_PERIOD_MS (12 * 60 * 60 * 1000)

// *** PUBLIC ***

int Bme680Driver::CreateDriverInstances(TwoWire *i2c, SensorDriver *firstInstance[], int maxInstances, float trim1, float trim2)
{
    if (maxInstances < 1)
        return 0;
    int instances = 0;

    // Bme680 can be at 0x77 (PRIMARY)
    i2c->beginTransmission(0x77);
    uint8_t e = i2c->endTransmission();
    Serial.printf("Bme680Driver 0x77 endTransmission %i\n", e);
    if (e == 0)
    {
        // Found primary sensor
        firstInstance[instances++] = new Bme680Driver(i2c, 0x77, "BME", trim1);
    }

    if (maxInstances < (instances + 1))
        return instances;

    // ... and / or 0x76 (SECONDARY)
    i2c->beginTransmission(0x76);
    e = i2c->endTransmission();
    Serial.printf("Bme680Driver 0x76 endTransmission %i\n", e);
    if (e == 0)
    {
        // Found secondary sensor
        firstInstance[instances++] = new Bme680Driver(i2c, 0x76, "BMF", trim2);
    }

    return instances;
}

int Bme680Driver::GetPacketData(char *ptr)
{
    char t[16];
    dtostrf((double)_lastTemp, 1, 4, t);

    char pressure[16];
    dtostrf((double)_lastPressure / 100, 1, 2, pressure);

    char humidity[16];
    dtostrf((double)_lastHumidity, 1, 2, humidity);

    char iaq[16];
    dtostrf((double)_lastIaq, 1, 0, iaq);

    char co2[16];
    dtostrf((double)_lastCo2Equivalent, 1, 2, co2);

    // (temprature) temperature,id=BMc25732 value=30.5072
    // (pressure) pressure,id=BMc25732 value=1004.13
    // (humidity) humidity,id=BMc25732 value=48.09
    // (static air quality) iaq,id=BMc25732 value=25
    // (static air quality accuracy) accuracy,id=BMc25732 value=25
    // (CO2 estimate) co2,id=BMc25732 value=500
    return sprintf(ptr,
                   "temperature,id=%s value=%s\n"
                   "pressure,id=%s value=%s\n"
                   "humidity,id=%s value=%s\n"
                   "iaq,id=%s value=%s\n"
                   "accuracy,id=%s value=%i\n"
                   "co2,id=%s value=%s\n",
                   _id, t,
                   _id, pressure,
                   _id, humidity,
                   _id, iaq,
                   _id, _lastIaqAccuracy,
                   _id, co2);
}

void Bme680Driver::Handle()
{
    if (_iaqSensor.run())
    {
        _lastTemp = _iaqSensor.temperature;
        _lastPressure = _iaqSensor.pressure;
        _lastHumidity = _iaqSensor.humidity;
        _lastIaq = _iaqSensor.staticIaq;
        _lastIaqAccuracy = _iaqSensor.staticIaqAccuracy;
        _lastCo2Equivalent = _iaqSensor.co2Equivalent;

        // Sanity check
        _lastReadingValid = _lastTemp >= MIN_SANE_VALUE && _lastTemp <= MAX_SANE_VALUE;

        // Debug output
        Serial.print(_id);
        Serial.println(" updated");

        // Save state if accuracy is 3 and haven't saved it for a while
        if (_lastIaqAccuracy == 3 &&
            ((_lastSaveMs == 0) || ((unsigned long)(millis() - _lastSaveMs) >= SAVE_PERIOD_MS)))
        {
            uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
            _iaqSensor.getState(bsecState);
            if (!IsBadStatus("getState()"))
            {
                File f = LittleFS.open(_id, "w");
                if (f)
                {
                    f.write((char *)&bsecState, BSEC_MAX_STATE_BLOB_SIZE);
                    f.close();
                }
            }
            // Save again in a while
            _lastSaveMs = millis();
        }
    }
    else
    {
        _lastReadingValid = !IsBadStatus("Handle()");
    }
}

void Bme680Driver::GetValues(void cb(const char *, const char *))
{
    char val[64], tmp[64];
    // Call back with name value pairs
    cb("Device", "BME680");
    sprintf(tmp, "%i.%i.%i.%i", _iaqSensor.version.major, _iaqSensor.version.minor, _iaqSensor.version.major_bugfix, _iaqSensor.version.minor_bugfix);
    cb("BSEC", tmp);
    sprintf(val, " %#x", _address);
    cb("Address", val);
    cb("Id", _id);
    cb("Subtract Trim (C) ", dtostrf((double)_trim, 1, 2, val));
    cb("Saved State", LittleFS.exists(_id) ? "YES" : "NO");

    if (!_lastReadingValid)
    {
        if (_iaqSensor.status != BSEC_OK || _iaqSensor.bme680Status != BME680_OK)
        {
            sprintf(val, "status:%d - bme680Status:%d", _iaqSensor.status, _iaqSensor.bme680Status);
            cb("ERROR", val);
        }
        else
            cb("ERROR", InsaneTemprature);
        return;
    }

    cb("Temprature (C)", dtostrf((double)_lastTemp, 1, 4, val));
    cb("Pressure (mb)", dtostrf((double)_lastPressure / 100, 1, 2, val));
    cb("Humidity (%)", dtostrf((double)_lastHumidity, 1, 2, val));

    const char *description = "";
    if (_lastIaq < 51)
        description = "Excellent";
    else if (_lastIaq < 101)
        description = "Good";
    else if (_lastIaq < 151)
        description = "Lightly Polluted";
    else if (_lastIaq < 201)
        description = "Moderately Polluted";
    else if (_lastIaq < 251)
        description = "Heavily Polluted";
    else if (_lastIaq < 351)
        description = "Severely Polluted";
    else
        description = "Extremely Polluted";
    sprintf(tmp, "%s (%s)", dtostrf((double)_lastIaq, 1, 0, val), description);
    cb("Air Quality (0-500)", tmp);

    description = "ERROR";
    if (_lastIaqAccuracy < 1)
        description = "Just Started";
    else if (_lastIaqAccuracy < 2)
        description = "History Uncertain";
    else if (_lastIaqAccuracy < 3)
        description = "Calibrating...";
    else if (_lastIaqAccuracy < 4)
        description = "Calibrated";
    sprintf(tmp, "%s (%s)", itoa(_lastIaqAccuracy, val, 10), description);
    cb("Air Quality Accuracy (0-3)", tmp);

    cb("CO2 Estimate (ppm)", dtostrf((double)_lastCo2Equivalent, 1, 2, val));
}

void Bme680Driver::Recalibrate()
{
    LittleFS.remove(_id);
}

// *** PRIVATE ***

Bme680Driver::Bme680Driver(TwoWire *i2c, int address, const char *prefix, float trim)
{
    _i2c = i2c;
    _address = address;

    // Unique id is BM - ESP8266 id
    sprintf(_id, "%s%x", prefix, ESP.getChipId());

    // Configure the sensor with operational data
    _iaqSensor.setConfig(bsec_config_iaq);

    // Configure sensor library
    _trim = trim;
    _iaqSensor.setTemperatureOffset(trim);
    _iaqSensor.begin(_address, *_i2c);
    IsBadStatus("begin()");
    bsec_virtual_sensor_t sensorList[6] = {
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_STABILIZATION_STATUS};
    _iaqSensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP);
    IsBadStatus("updateSubscription()");

    // Load last state if available (air quality history)
    File f = LittleFS.open(_id, "r");
    if (f)
    {
        uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
        f.readBytes((char *)&bsecState, BSEC_MAX_STATE_BLOB_SIZE);
        f.close();
        Serial.println("Setting saved BME680 state");
        _iaqSensor.setState(bsecState);
        IsBadStatus("setState()");
    }
}

bool Bme680Driver::IsBadStatus(const char *str)
{
    if (_iaqSensor.status != BSEC_OK || _iaqSensor.bme680Status != BME680_OK)
    {
        char msg[256];
        sprintf(msg, "%s status:%d bme680Status:%d wireStatus:%d", str, _iaqSensor.status, _iaqSensor.bme680Status, _i2c->status());
        Serial.println(msg);
        return true;
    }
    return false;
}
