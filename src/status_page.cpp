#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "main.h"
#include "status_page.h"

const char *head = R"(
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,shrink-to-fit=no,viewport-fit=cover">
</head>)";

const char *style = R"(
<style>
body {
background-color: #202020;
font-family: "San Francisco", "Segoe UI", sans-serif;
}
.board {
border-radius: 4px;
display: table;
padding: 8px;
background-color: rgb(64, 51, 153);
color: rgb(223, 223, 223);
margin-bottom: 4px;
}
.sensor {
border-radius: 4px;
display: table;
padding: 8px;
background-color: rgb(163, 93, 36);
color: rgb(223, 223, 223);
margin-bottom: 4px;
}
.row {
display: table-row;
}
.anno {
display: table-cell;
color: #a8a8a8;
font-weight: 500;
padding: 2px
}
.data {
display: table-cell;
padding: 2px
}
input {
border-style: solid;
border-color: brown;
background-color: black;
color: rgb(223, 223, 223);;
margin-top: 10px;
height: 30px;
width: 130px;
display: block;
}
input:active {
background-color: #404040;
}
input:focus {
outline-color: darkred;
}
</style>)";

const char *boardBegin = R"(
<div class="board">)";

const char *boardRow = R"(
<div class="row">
<div class="anno">%s</div>
<div class="data">%s</div>
</div>)";

const char *boardEnd = R"(
</div>)";

const char *sensorBegin = R"(
<div class="sensor">)";

const char *sensorRow = R"(
<div class="row">
<div class="anno">%s</div>
<div class="data">%s</div>
</div>)";

const char *sensorEnd = R"(
</div>)";

const char *buttons = R"(
<div>
<form method="post" action="/commands">
<input type="submit" value="RESTART" name="restart">
<input type="submit" value="RECALIBRATE" name="recalibrate">
</form>
</div>)";

// Web page
char webPage[8192];
int webPageLen = 0;

const char *BuildStatusPage()
{
    // Html head
    strcpy(webPage, "<html lang=\"en\">");
    strcat(webPage, head);

    // body
    strcat(webPage, "<body>");

    // Html page header
    strcat(webPage, style);
    webPageLen = strlen(webPage);
    char tmp[24];

    // Board info
    webPageLen += sprintf(&webPage[webPageLen], boardBegin);
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "Host Name", hostname);
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "IP", WiFi.localIP().toString().c_str());
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "CPU Speed (MHz)", itoa(ESP.getCpuFreqMHz(), tmp, 10));
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "Free Heap (bytes)", itoa(ESP.getFreeHeap(), tmp, 10));
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "Heap Frag (%)", itoa(ESP.getHeapFragmentation(), tmp, 10));
    webPageLen += sprintf(&webPage[webPageLen], boardRow, "Poll Period (ms)", itoa(POLL_PERIOD_MS, tmp, 10));
    // Startup log
    for (int i = 0; i < MAX_STARTUP_LOG_ENTRIES && startupLog[i].time != 0; i++)
    {
        const char *reason = "";
        switch (startupLog[i].reason)
        {
        case REASON_DEFAULT_RST:
            reason = "Power On";
            break;
        case REASON_WDT_RST:
            reason = "Hardware Watchdog";
            break;
        case REASON_EXCEPTION_RST:
            reason = "Exception";
            break;
        case REASON_SOFT_WDT_RST:
            reason = "Software Watchdog";
            break;
        case REASON_SOFT_RESTART:
            reason = "Software Restart";
            break;
        case REASON_DEEP_SLEEP_AWAKE:
            reason = "Deep-Sleep Wake";
            break;
        case REASON_EXT_SYS_RST:
            reason = "Power On or Reset Button";
            break;
        default:
            reason = "Unknown";
            break;
        }
        webPageLen += sprintf(&webPage[webPageLen], boardRow, myTZ.dateTime(startupLog[i].time, UTC_TIME).c_str(), reason);
    }
    webPageLen += sprintf(&webPage[webPageLen], boardEnd);

    // Sensor info
    for (int i = 0; i < drivers_count; i++)
    {
        webPageLen += sprintf(&webPage[webPageLen], sensorBegin);
        drivers[i]->GetValues([](const char *n, const char *v) {
            webPageLen += sprintf(&webPage[webPageLen], sensorRow, n, v);
        });
        webPageLen += sprintf(&webPage[webPageLen], sensorEnd);
    }

    // Buttons
    webPageLen += sprintf(&webPage[webPageLen], buttons);

    // Closing tags
    webPageLen += sprintf(&webPage[webPageLen], "</body></html>");

    // Report length
    Serial.printf("Web Page Length %i bytes\n", webPageLen);
    return webPage;
}
