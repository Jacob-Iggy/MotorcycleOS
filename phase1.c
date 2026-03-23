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
// engine subsystem - updates RPM, temps, simulates idle to high
void *engine_thread(void *arg)
{

    int rpm_direction = 1; // 1 = rpms increase, -1 = rpms decrease

    while (1)
    {
        if (engine_state == 0)
        {
            // if engines off rpms go down by 150 until at 0 and temps lower
            if (rpm > 0)
            {
                rpm -= 150;
                if (rpm < 0)
                    rpm = 0;
            }
            if (engine_temp > 20)
            {
                engine_temp -= 1;
            }
        }
        else
        {
            // simulates increasing rpms for phase 1
            rpm += rpm_direction * 150;

            // lowers rpms back down for simulation and repeats
            if (rpm >= 8500)
            {
                rpm = 8500;
                rpm_direction = -1;
            }
            else if (rpm <= 1200)
            {
                rpm = 1200;
                rpm_direction = 1;
            }

            // if decreases or increases wrong sets to max and mins
            if (rpm < 1100)
                rpm = 1100;
            if (rpm > 16500)
                rpm = 16500;

            // rpm zones connect to temps, the higher zone the faster engine increases in temp, vice versa but idle lets it cool
            if (rpm_zone == 3)
            {
                engine_temp += 3;
            }
            else if (rpm_zone == 2)
            {
                engine_temp += 2;
            }
            else if (rpm_zone == 1)
            {
                if (engine_temp < 88)
                {
                    engine_temp += 1;
                }
            }
            else if (rpm_zone == 0)
            {
                if (engine_temp > 75)
                {
                    engine_temp -= 1;
                }
            }
            // max of 130 degrees
            if (engine_temp > 130)
                engine_temp = 130;
        }

        sleep(1); // update every second
    }

    return NULL;
}

// Motion Subsystem (Logan)
void *motion_thread(void *arg)
{
    while (1)
    {
        // check what the current speed is
        // 50mph -> increase
        // 70mph -> decrease
        if (speed == 50)
        {
            while (speed >= 50 && speed < 70)
            {
				// sleep for 1 second then increment speed
                usleep(100000);
                speed++;
                // calculate distance
                // NOTE: speed is in mph so need to calculate distance covered every second
                // to match dashboard refreshes
                float distance = (float)speed / 3600;
                // update distance counters
                distance_total += distance;
                distance_trip += distance;
            }
        }
        else if (speed == 70)
        {
            while (speed <= 70 && speed > 50)
            {
				// sleep for 1 second then increment speed
                usleep(100000);
                speed--;
                // calculate distance
                float distance = (float)speed / 3600;
                // update distance counters
                distance_total += distance;
                distance_trip += distance;
            }
        }
    }
    return NULL;
}

// Fuel Subsystem (Logan)
void *fuel_thread(void *arg)
{
    while (1)
    {
        if (rpm_zone == 0)
        { // Idle = [1100═1300)
            fuel -= 0.002;
        }
        else if (rpm_zone == 1)
        { // Normal = [1300═8000)
            fuel -= 0.005;
        }
        else if (rpm_zone == 2)
        { // High = [8000═14500)
            fuel -= 0.007;
        }
        else if (rpm_zone == 3)
        { // Redline = [14500═16500)
            fuel -= 0.009;
        }

		//check if the fuel is 0 and shutoff the engine
		if (fuel <= 0) {
			engine_state = 0;
		}
		// sleep for 1 second then decrement speed
        usleep(100000);
    }
    return NULL;
}

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
            rpm_zone = -1;
        }

        // this part updates the RPM zone based on the current RPM value
        if (rpm >= 1100 && rpm < 1300)
        {
            // IDLE
            if (rpm_zone != 0)
            {
                if (rpm_zone == -1)
                {
                    newRPMZone = 1; // set to increment if we are moving up from engine off
                }
                else
                {
                    newRPMZone = 2; // set to decrement if we are leaving a higher zone
                }
                rpm_zone = 0;
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
void *hybrid_assist_thread(void *arg)
{
    while (1)
    {
        // Low-Speed Electric Cruising
        if (speed > 0 && speed <= 30)
        {
            // set the electric assist state to on
            electric_assist_state = 1;
            // set hybrid mode
            hybrid_mode = 1;
            // vehicle uses battery assist so battery gets drained
            battery_level - 0.05;
        }

        // Electric Assist During Acceleration
        if (speed > 30 && speed <= 60)
        {
            electric_assist_state = 1;
            hybrid_mode = 2;
            battery_level - 0.07;
        }
        else if (speed > 60 && speed <= 90)
        {
            electric_assist_state = 1;
            hybrid_mode = 2;
            battery_level - 0.09;
        }

        // Regenerative Charging
        if (hybrid_mode == 3)
        {
            // recharging state so charge battery incrementally
            battery_level += 0.05;
        }
    }
    return NULL;
}

void push_event(const struct Event *newEvent)
{
    if (event_count < MAX_EVENTS)
    {
        event_log[event_count] = *newEvent;
        event_count++;
        return;
    }

    memmove(&event_log[0], &event_log[1], sizeof(struct Event) * (MAX_EVENTS - 1));
    event_log[MAX_EVENTS - 1] = *newEvent;
}

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
                push_event(&newEvent);
            }
            else if (newRPMZone == 2)
            {
                // if we are moving down to a lower RPM zone, log that event
                struct Event newEvent;
                sprintf(newEvent.title, "RPM Zone Update");
                sprintf(newEvent.description, "RPM zone decreased to %d", rpm_zone);
                // set timestamp for the event here
                newEvent.timestamp = time_elapsed_trip;
                push_event(&newEvent);
            }
            newRPMZone = 0; // reset the variable after handling the event
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
            push_event(&newEvent);

            hybridAssistChange = 0; // reset the variable after handling the event
        }

        if (fuel < 0.7 && !fuel_warning_logged)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Low Fuel Warning");
            sprintf(newEvent.description, "Fuel level low at %.2f gallons", fuel);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            push_event(&newEvent);
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
            push_event(&newEvent);
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
            push_event(&newEvent);
        }

        if (wheelSlipDetected == 1)
        {
            struct Event newEvent;
            sprintf(newEvent.title, "Wheel Slip Detected");
            sprintf(newEvent.description, "Wheel slip detected at speed %d mph", speed);
            // set timestamp for the event here
            newEvent.timestamp = time_elapsed_trip;
            push_event(&newEvent);
        }

        usleep(100000);
    }

    return NULL;
}

// Dashboard Subsystem
// prints dashboard
void print_dashboard(void)
{

    char *eng_status = (engine_state == 1) ? "ON " : "OFF";

    char *signal_label;
    if (signal_state == 1)
        signal_label = "< LEFT      ";
    else if (signal_state == 2)
        signal_label = "RIGHT >     ";
    else if (signal_state == 3)
        signal_label = "< HAZARD >  ";
    else
        signal_label = "OFF         ";

    char *headlight_label = (headlight_state == 1) ? "* ON " : "o OFF";

    char *temp_zone_label;
    if (engine_temp_zone == 0)
        temp_zone_label = "COLD    ";
    else if (engine_temp_zone == 1)
        temp_zone_label = "NORMAL  ";
    else if (engine_temp_zone == 2)
        temp_zone_label = "HOT     ";
    else
        temp_zone_label = "OVERHEAT";

    char *rpm_zone_label;
    if (rpm_zone == 0)
        rpm_zone_label = "IDLE   ";
    else if (rpm_zone == 1)
        rpm_zone_label = "NORMAL ";
    else if (rpm_zone == 2)
        rpm_zone_label = "HIGH   ";
    else if (rpm_zone == 3)
        rpm_zone_label = "REDLINE";
    else
        rpm_zone_label = "OFF    ";

    char *hybrid_mode_label;
    if (hybrid_mode == 1)
        hybrid_mode_label = "CRUISING    ";
    else if (hybrid_mode == 2)
        hybrid_mode_label = "ASSIST      ";
    else if (hybrid_mode == 3)
        hybrid_mode_label = "REGENERATION";
    else
        hybrid_mode_label = "NONE        ";

    char *elec_assist_label = (electric_assist_state == 1) ? "ON " : "OFF";
    char *charging_label = (charging_state == 1) ? "ON " : "OFF";

    // fuel data (4.7 gal tank)
    char fuel_bar[21];
    int fuel_filled = (int)((fuel / 4.7f) * 20);
    if (fuel_filled < 0)
        fuel_filled = 0;
    if (fuel_filled > 20)
        fuel_filled = 20;
    for (int i = 0; i < 20; i++)
    {
        fuel_bar[i] = (i < fuel_filled) ? '#' : '-';
    }
    fuel_bar[20] = '\0';

    char *low_fuel_label = (low_fuel_warning == 1) ? "!! LOW FUEL !!" : "              ";

    // battery display
    char batt_bar[21];
    int batt_filled = (battery_level * 20) / 100;
    if (batt_filled < 0)
        batt_filled = 0;
    if (batt_filled > 20)
        batt_filled = 20;
    for (int i = 0; i < 20; i++)
    {
        batt_bar[i] = (i < batt_filled) ? '#' : '-';
    }
    batt_bar[20] = '\0';

    // print dash
    printf("╔═════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                GEEKERS OS                                  ║\n");
    printf("║═════════════════════════════════════════════════════════════════════════════║\n");
    printf("║  ENGINE: %-3s  |  TEMP: %3d C (%-8s)  |  RPM: %5d (%-7s)           ║\n",
           eng_status, engine_temp, temp_zone_label, rpm, rpm_zone_label);
    printf("║  SPEED:  %3d mph                                                           ║\n",
           speed);
    printf("║─────────────────────────────────────────────────────────────────────────────║\n");
    printf("║  FUEL   [%-20s]  %4.2f gal  %-14s                   ║\n",
           fuel_bar, fuel, low_fuel_label);
    printf("║  DIST TOTAL: %8.1f mi        DIST TRIP: %6.1f mi                      ║\n",
           distance_total, distance_trip);
    printf("║─────────────────────────────────────────────────────────────────────────────║\n");
    printf("║  TIME ELAPSED (TOTAL): %02d:%02d:%02d                                            ║\n",
           time_elapsed_total.hours, time_elapsed_total.minutes, time_elapsed_total.seconds);
    printf("║  TIME ELAPSED (TRIP):  %02d:%02d:%02d                                            ║\n",
           time_elapsed_trip.hours, time_elapsed_trip.minutes, time_elapsed_trip.seconds);
    printf("║─────────────────────────────────────────────────────────────────────────────║\n");
    printf("║  SIGNAL: %-12s       HEADLIGHT: %-5s                              ║\n",
           signal_label, headlight_label);
    printf("║═══════════════════════════ HYBRID ASSIST SYSTEM ════════════════════════════║\n");
    printf("║  BATTERY  [%-20s]  %3d%%                                       ║\n",
           batt_bar, battery_level);
    printf("║  ELECTRIC ASSIST: %-3s    CHARGING: %-3s    HYBRID MODE: %-12s     ║\n",
           elec_assist_label, charging_label, hybrid_mode_label);
    printf("║═══════════════════════════════ EVENT LOG ═══════════════════════════════════║\n");

    // event log
    int start = (event_count >= 3) ? event_count - 3 : 0;
    int display_num = 1;
    for (int i = start; i < event_count; i++)
    {
        printf("║  %d. [%02d:%02d:%02d] %-57s  ║\n",
               display_num,
               event_log[i].timestamp.hours,
               event_log[i].timestamp.minutes,
               event_log[i].timestamp.seconds,
               event_log[i].description);
        display_num++;
    }
    // if less than 3 events leave space
    for (int i = display_num; i <= 3; i++)
    {
        printf("║  %d. ---                                                                     ║\n", i);
    }

    printf("╚═════════════════════════════════════════════════════════════════════════════╝\n");
}

// function to refresh dash
void refresh_dashboard(void (*print_fn)(void))
{
    // clears screen and moves cursor as given in notes
    printf("\033[H\033[J");
    // reprint dash
    print_fn();
    // display output immediately
    fflush(stdout);
}

// dash subsystem
void *dashboard_thread(void *arg)
{
    while (1)
    {
        refresh_dashboard(print_dashboard);
        usleep(500000); // refreshes dash every 0.5 sec
    }
    return NULL;
}

// MAIN FUNCTION
int main()
{
    srand(time(NULL));

    engine_state = 1;
    rpm = 1200; // start at idle RPM
    rpm_zone = 0;
    engine_temp = 45; // cold start
    engine_temp_zone = 0;

    speed = 50;
    fuel = 3.5f;
    low_fuel_warning = 0;

    // Total distance - random start val
    distance_total = (float)(rand() % 10000 + 1000);
    distance_trip = 0.0f;

    // Total time elapsed - random starting value (HH:MM:SS)
    time_elapsed_total.hours = rand() % 100;
    time_elapsed_total.minutes = rand() % 60;
    time_elapsed_total.seconds = rand() % 60;

    // Trip time always starts at zero
    time_elapsed_trip.hours = 0;
    time_elapsed_trip.minutes = 0;
    time_elapsed_trip.seconds = 0;

    signal_state = 0;
    headlight_state = 1;
    battery_level = 74;
    electric_assist_state = 1;
    charging_state = 0;
    hybrid_mode = 2; // hybrid assist

    event_count = 0;
    newRPMZone = 0;
    hybridAssistChange = 0;
    wheelSlipDetected = 0;

    // create threads
    pthread_t engine_tid, motion_tid, fuel_tid, ecu_tid, hybrid_assist_tid, event_tid, dashboard_tid;

    pthread_create(&engine_tid, NULL, engine_thread, NULL);
    pthread_create(&motion_tid, NULL, motion_thread, NULL);
    pthread_create(&fuel_tid, NULL, fuel_thread, NULL);
    pthread_create(&ecu_tid, NULL, ecu_thread, NULL);
    pthread_create(&hybrid_assist_tid, NULL, hybrid_assist_thread, NULL);
    pthread_create(&event_tid, NULL, event_thread, NULL);
    pthread_create(&dashboard_tid, NULL, dashboard_thread, NULL);

    // Wait (threads run indefinitely)
    pthread_join(engine_tid, NULL);
    pthread_join(motion_tid, NULL);
    pthread_join(fuel_tid, NULL);
    pthread_join(ecu_tid, NULL);
    pthread_join(hybrid_assist_tid, NULL);
    pthread_join(event_tid, NULL);
    pthread_join(dashboard_tid, NULL);

    return 0;
}

// engine state ═ on/off

// RPM ═ if engine = off rpm = 0
// if on not moving 1100═1300

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

// Event Logging Subsystem:
// Display most recent events`
