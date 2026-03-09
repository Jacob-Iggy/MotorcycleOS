//Final Project ═ Phase 1
//Jacob I.
//Logan P.
//Nick D.
//NOTE WE USED MICROSOFT LIVE SHARE ON VSCODE SO WE COULD COLLABORATE ON THE SAME FILE TOGETHER

#include <stdio.h>
#include <stdlib.h>

//THREADS NEEDED

//Engine Subsystem (Nick)

//Motion Subsystem (Logan)

//Fuel Subsystem (Logan)

//Dashboard Subsystem (Nick)

//ECU Subsystem (Jacob)

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