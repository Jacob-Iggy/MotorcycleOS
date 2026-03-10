//Final Project ═ Phase 1
//Jacob I.
//Logan P.
//Nick D.
//NOTE WE USED MICROSOFT LIVE SHARE ON VSCODE SO WE COULD COLLABORATE ON THE SAME FILE TOGETHER

#include <stdio.h>
#include <stdlib.h>

# define MAX_EVENTS 50

int engine_state; // 0 = off, 1 = on
int rpm; // 0 if engine off, 1100-16500 if on
int rpm_zone; // 0 = idle, 1 = normal, 2 = high, 3 = redline
int engine_temp; // temp in degrees Celsius
int engine_temp_zone; // 0 = cold, 1 = normal, 2 = hot, 3 = overheat
int time_elapsed_total; // in seconds
int time_elapsed_trip; // in seconds
int speed; // in mph, 0 if engine off, 0-200 if on
float fuel; // in gallons, 0-4.7
int low_fuel_warning; // 0 = no warning, 1 = low fuel warning
float distance_total; // in miles
float distance_trip; // in miles
int signal_state; // 0 = off, 1 = left, 2 = right, 3 = hazards
int headlight_state; // 0 = off, 1 = on
int battery_level; // in percentage, 0-100
int electric_assist_state; // 0 = off, 1 = on
int charging_state; // 0 = off, 1 = on
int hybrid_mode; // 0 = none, 1 = cruising, 2 = assist, 3 = regeneration
char event_log[MAX_EVENTS][100]; // array to store event logs
int event_count; // number of events in the log

//THREADS NEEDED

//Engine Subsystem (Nick)

//Motion Subsystem (Logan)

//Fuel Subsystem (Logan)

//Dashboard Subsystem (Nick)

//ECU Subsystem (Jacob)
void* ecu_thread(void* arg) {

    while (1) {

        // if engine is off make sure rpm and speed is at 0
        if (engine_state == 0) {
            rpm = 0;
            speed = 0;
        } 

        // this part updates the RPM zone based on the current RPM value
        if (rpm >= 1100 && rpm < 1300) {
            // IDLE
            rpm_zone = 0;
        } else if (rpm >= 1300 && rpm < 8000) {
            // NORMAL
            rpm_zone = 1;
        } else if (rpm >= 8000 && rpm < 14500) {
            // HIGH
            rpm_zone = 2;
        } else if (rpm >= 14500 && rpm <= 16500) {
            // REDLINE
            rpm_zone = 3;
        } else {
            // ENGINE OFF
            rpm_zone = -1;
        }

        // this part updates the engine temperature zone based on the current engine temperature
        if (engine_temp < 60) {
            // COLD
            engine_temp_zone = 0;
        } else if (engine_temp >= 60 && engine_temp < 95) {
            // NORMAL
            engine_temp_zone = 1;
        } else if (engine_temp >= 95 && engine_temp < 105) {
            // HOT
            engine_temp_zone = 2;
        } else if (engine_temp >= 105) {
            // OVERHEAT
            engine_temp_zone = 3;
        }

        // if fuel is less than 0.7 gallons, enable the low fuel warning
        if (fuel < 0.7) {
            low_fuel_warning = 1;
        } else {
            // otherwise, disable the warning
            low_fuel_warning = 0;
        }

    }

    return NULL;
}

//Hybrid Assist System Subsystem (Logan)

//Event Logging Subsystem (Jacob)

//MAIN FUNCTION
int main() {
    //engine state ═ on/off
    
    //RPM ═ if engine = off rpm = 0
    //if on not moving 1100═1300

    //RPM ZONE:
    //Idle = [1100═1300)
    //Normal = [1300═8000)
    //High = [8000═14500)
    //Redline = [14500═16500)

    //Engine Temp:
    //Cold < 60 deg C
    //Normal 60 ═ 95
    //Hot 95 ═ 105
    //Overheat > 105

    //Time:
    //Time Elapsed overall = random value everytime program starts
    //Time Elapsed current = time since vehicle turned on
    //HH:MM:SS

    //Speed:
    //Engine off = 0
    //Between 0 ═ 200 mph

    //Fuel:
    //Visually as a bar and numbers
    //0 ═ 4.7 gal
    //If fuel < 0.7 indicate low on fuel

    //Distance Traveled:
    //Total = random
    //Current trip distance = distance from vehicle turned on
    //Update both as vehicle is moving
    //Current trip distance alway 0 at start

    //Signal State:
    //Left, Right, Off
    //Hazards = Both left/right

    //Headlight:
    //on/off

    //Group of Three
    //Hybrid Assist System:
    //Battery Level (num)
    //Electric Asist State (on/off)
    //Chargin state (on/off)
    //Hybrid assist mode

    //Event Logging Subsystem:
    //Display most recent events`

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