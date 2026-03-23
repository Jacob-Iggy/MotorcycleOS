// Final Project ═ Phase 1
// Jacob I.
// Logan P.
// Nick D.
// NOTE WE USED MICROSOFT LIVE SHARE ON VSCODE SO WE COULD COLLABORATE ON THE SAME FILE TOGETHER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_EVENTS 50

struct Time
{
    int hours;
    int minutes;
    int seconds;
};

struct Event
{
    char title[50];
    char description[200];
    struct Time timestamp;
};

int engine_state;               // 0 = off, 1 = on
int rpm;                        // 0 if engine off, 1100-16500 if on
int rpm_zone;                   // 0 = idle, 1 = normal, 2 = high, 3 = redline
int engine_temp;                // temp in degrees Celsius
int engine_temp_zone;           // 0 = cold, 1 = normal, 2 = hot, 3 = overheat
struct Time time_elapsed_total; // in seconds
struct Time time_elapsed_trip;  // in seconds
int speed;                      // in mph, 0 if engine off, 0-200 if on
float fuel;                     // in gallons, 0-4.7
int low_fuel_warning;           // 0 = no warning, 1 = low fuel warning
float distance_total;           // in miles
float distance_trip;            // in miles
int signal_state;               // 0 = off, 1 = left, 2 = right, 3 = hazards
int headlight_state;            // 0 = off, 1 = on
int battery_level;              // in percentage, 0-100
int electric_assist_state;      // 0 = off, 1 = on
int charging_state;             // 0 = off, 1 = on
int hybrid_mode;                // 0 = none, 1 = cruising, 2 = assist, 3 = regeneration

// Event System Variables
struct Event event_log[MAX_EVENTS]; // array to store event logs
int event_count;                    // number of events in the log
int newRPMZone;                     // variable to store if an rpm zone update event has been triggered, 0 = no update, 1 = increment, 2 = decrement
int hybridAssistChange;             // variable to store if hybrid assist mode changes, 0 = no change, 1 = turned on, 2 = turned off
int wheelSlipDetected;              // variable to store if wheel slip is detected, 0 = no slip, 1 = slip detected
int fuel_warning_logged;            // variable to track if low fuel warning has already been logged, 0 = not logged, 1 = logged
int battery_warning_logged;         // variable to track if low battery warning has already been logged, 0 = not logged, 1 = logged

// THREADS NEEDED

// Engine Subsystem (Nick)

// Motion Subsystem (Logan)

// Fuel Subsystem (Logan)

// Dashboard Subsystem (Nick)

// ECU Subsystem (Jacob)
void *ecu_thread(void *arg)
{

    while (1)
    {

        // if engine is off make sure rpm and speed is at 0
        if (engine_state == 0)
        {
            rpm = 0;
            speed = 0;
        }

        // this part updates the RPM zone based on the current RPM value
        if (rpm >= 1100 && rpm < 1300)
        {
            // IDLE
            if (rpm_zone != 0)
            {
                if (rpm_zone != 0)
                {
                    newRPMZone = 2; // set to decrement if we are leaving a higher zone
                    rpm_zone = 0;
                }
            }
        }
        else if (rpm >= 1300 && rpm < 8000)
        {
            // NORMAL
            if (rpm_zone != 1)
            {
                if (rpm_zone != 0)
                {
                    newRPMZone = 2; // set to decrement if we are leaving a higher zone
                }
                else
                {
                    newRPMZone = 1; // set to increment if we are moving up from idle
                }
                rpm_zone = 1;
            }
        }
        else if (rpm >= 8000 && rpm < 14500)
        {
            // HIGH
            if (rpm_zone != 2)
            {
                if (rpm_zone != 1)
                {
                    newRPMZone = 2; // set to decrement if we are leaving a higher zone
                }
                else
                {
                    newRPMZone = 1; // set to increment if we are moving up from normal
                }
                rpm_zone = 2;
            }
        }
        else if (rpm >= 14500 && rpm <= 16500)
        {
            // REDLINE
            if (rpm_zone != 3)
            {
                if (rpm_zone != 2)
                {
                    newRPMZone = 2; // set to decrement if we are leaving a higher zone
                }
                else
                {
                    newRPMZone = 1; // set to increment if we are moving up from high
                }
                rpm_zone = 3;
            }
        }
        else
        {
            // ENGINE OFF
            if (rpm_zone != -1)
            {
                newRPMZone = 2; // set to decrement if we are leaving a higher zone
                rpm_zone = -1;
            }
        }

        // this part updates the engine temperature zone based on the current engine temperature
        if (engine_temp < 60)
        {
            // COLD
            engine_temp_zone = 0;
        }
        else if (engine_temp >= 60 && engine_temp < 95)
        {
            // NORMAL
            engine_temp_zone = 1;
        }
        else if (engine_temp >= 95 && engine_temp < 105)
        {
            // HOT
            engine_temp_zone = 2;
        }
        else if (engine_temp >= 105)
        {
            // OVERHEAT
            engine_temp_zone = 3;
        }

        // if fuel is less than 0.7 gallons, enable the low fuel warning
        if (fuel < 0.7)
        {
            low_fuel_warning = 1;
        }
        else
        {
            // otherwise, disable the warning
            low_fuel_warning = 0;
        }
    }

    return NULL;
}

// Hybrid Assist System Subsystem (Logan)

// Event Logging Subsystem (Jacob)
void *event_thread(void *arg)
{

    while (1)
    {
        if (newRPMZone != 0)
        {
            if (newRPMZone == 1)
            {
                // if we are moving up to a higher RPM zone, log that event
                struct Event newEvent;
                sprintf(newEvent.title, "RPM Zone Update");
                sprintf(newEvent.description, "RPM zone increased to %d", rpm_zone);
                // set timestamp for the event here
                newEvent.timestamp = time_elapsed_trip;
                event_log[event_count] = newEvent;
                event_count++;
            }
            else if (newRPMZone == 2)
            {
                // if we are moving down to a lower RPM zone, log that event
                struct Event newEvent;
                sprintf(newEvent.title, "RPM Zone Update");
                sprintf(newEvent.description, "RPM zone decreased to %d", rpm_zone);
                // set timestamp for the event here
                newEvent.timestamp = time_elapsed_trip;
                event_log[event_count] = newEvent;
                event_count++;

                newRPMZone = 0; // reset the variable after handling the event
            }
        }

        if (hybridAssistChange != 0)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Hybrid Assist Mode Change");
            // the description will depend on whether the assist mode was turned on or off, and what the current hybrid mode is
            if (hybridAssistChange == 1)
            {
                sprintf(newEvent.description, "Hybrid assist mode turned on, current mode: %d", hybrid_mode);
            }
            else if (hybridAssistChange == 2)
            {
                sprintf(newEvent.description, "Hybrid assist mode turned off, current mode: %d", hybrid_mode);
            }
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            event_log[event_count] = newEvent;
            event_count++;

            hybridAssistChange = 0; // reset the variable after handling the event
        }

        if (fuel < 0.7 && !fuel_warning_logged)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Low Fuel Warning");
            sprintf(newEvent.description, "Fuel level low at %.2f gallons", fuel);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            event_log[event_count] = newEvent;
            event_count++;
            fuel_warning_logged = 1;
        }
        else if (fuel >= 0.7 && fuel_warning_logged)
        {
            fuel_warning_logged = 0; // reset the variable if fuel level is back above the threshold
        }

        if (battery_level < 10 && !battery_warning_logged)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Low Battery Warning");
            sprintf(newEvent.description, "Battery level low at %d%%", battery_level);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            event_log[event_count] = newEvent;
            event_count++;
            battery_warning_logged = 1;
        }
        else if (battery_level >= 10 && battery_warning_logged)
        {
            battery_warning_logged = 0; // reset the variable if battery level is back above the threshold
        }

        if (engine_temp_zone == 3)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Engine Overheat Warning");
            sprintf(newEvent.description, "Engine temperature critical at %d°C", engine_temp);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            event_log[event_count] = newEvent;
            event_count++;
        }

        if (wheelSlipDetected == 1)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Wheel Slip Detected");
            sprintf(newEvent.description, "Wheel slip detected at speed %d mph", speed);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            event_log[event_count] = newEvent;
            event_count++;
        }
    }

    return NULL;
}

// MAIN FUNCTION
int main()
{
    // engine state ═ on/off

    // RPM ═ if engine = off rpm = 0
    // if on not moving 1100═1300

    // RPM ZONE:
    // Idle = [1100═1300)
    // Normal = [1300═8000)
    // High = [8000═14500)
    // Redline = [14500═16500)

    // Engine Temp:
    // Cold < 60 deg C
    // Normal 60 ═ 95
    // Hot 95 ═ 105
    // Overheat > 105

    // Time:
    // Time Elapsed overall = random value everytime program starts
    // Time Elapsed current = time since vehicle turned on
    // HH:MM:SS

    // Speed:
    // Engine off = 0
    // Between 0 ═ 200 mph

    // Fuel:
    // Visually as a bar and numbers
    // 0 ═ 4.7 gal
    // If fuel < 0.7 indicate low on fuel

    // Distance Traveled:
    // Total = random
    // Current trip distance = distance from vehicle turned on
    // Update both as vehicle is moving
    // Current trip distance alway 0 at start

    // Signal State:
    // Left, Right, Off
    // Hazards = Both left/right

    // Headlight:
    // on/off

    // Group of Three
    // Hybrid Assist System:
    // Battery Level (num)
    // Electric Asist State (on/off)
    // Chargin state (on/off)
    // Hybrid assist mode

    // Event Logging Subsystem:
    // Display most recent events`

    printf("╔═════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                GEEKERS OS                                   ║\n");
    printf("║═════════════════════════════════════════════════════════════════════════════║\n");
    printf("║ ENG ●        TMP 96°C (HOT)        RPM 7200 (IDLE)        SPD 68 mph        ║\n");
    printf("║ FUEL   [██████████░░░░░░░░] LOW     DIST 21.3 km                             ║\n");
    printf("║ TIME ELAPSED (TOTAL):   82:41:12                                           ║\n");
    printf("║ TIME ELAPSED (TRIP):    00:34:52                                            ║\n");
    printf("║ SIGNAL: ◄ LEFT BLINK        HEADLIGHT: ● ON                                 ║\n");
    printf("║══════════════════════════ HYBRID ASSIST SYSTEM ═════════════════════════════║\n");
    printf("║ BATTERY LEVEL     [████████░░░░] 74%%                                        ║\n");
    printf("║ ELECTRIC ASSIST   ON                                                        ║\n");
    printf("║ CHARGING STATE    OFF                                                       ║\n");
    printf("║ HYBRID MODE       ASSIST                                                    ║\n");
    printf("║═══════════════════════════════ EVENT LOG ═══════════════════════════════════║\n");
    printf("║ 1. Engine started                                                           ║\n");
    printf("║ 2. Left blinker activated                                                   ║\n");
    printf("║ 3. Speed reached 68 mph                                                     ║\n");
    printf("╚═════════════════════════════════════════════════════════════════════════════╝\n");
}