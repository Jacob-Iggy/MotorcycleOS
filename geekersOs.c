// Final Project ═ Phase 2
// Jacob I.
// Logan P.
// Nick D.
// NOTE WE USED MICROSOFT LIVE SHARE ON VSCODE SO WE COULD COLLABORATE ON THE
// SAME FILE TOGETHER

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>

#define MAX_EVENTS 50

// time struct to store hours, minutes, and seconds
struct Time
{
  int hours;
  int minutes;
  int seconds;
};

// event struct to store title, description, and timestamp
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
float battery_level;            // in percentage, 0-100
int electric_assist_state;      // 0 = off, 1 = on
int electric_assist_allowed;    // 0 = not allowed, 1 = allowed based on conditions
int charging_state;             // 0 = off, 1 = on
int hybrid_mode;                // 0 = none, 1 = cruising, 2 = assist, 3 = regeneration

// direction global variable for motion and hybrid assist
int direction;

// Event System Variables
struct Event event_log[MAX_EVENTS]; // array to store event logs
int event_count;                    // number of events in the log
struct Event event_queue[5];        // queue to store events that need to be processed by the event thread, 5 events max in the queue
int event_queue_count;              // number of events currently in the queue
int newRPMZone;                     // variable to store if an rpm zone update event has been
                                    // triggered, 0 = no update, 1 = increment, 2 = decrement
int hybridAssistChange;             // variable to store if hybrid assist mode changes, 0 =
                                    // no change, 1 = turned on, 2 = turned off
int wheelSlipDetected;              // variable to store if wheel slip is detected, 0 = no
                                    // slip, 1 = slip detected
float last_fuel_amount_logged;      // variable to track what the last low fuel
                                    // warning was logged at
float last_battery_amount_logged;   // variable to track what the last low battery
                                    // warning was logged at
int ecu_update;                     // variable to track if something has changed like fuel or speed so the ecu must go through and check the system status
int engine_off_decelerate;          // variable to track if the engine was just turned off and speed isn't 0, we must decelarate
int max_speed;                      // variable to limit max speed, changed by ecu if overheating
int max_rpm;                        // variable to limit max rpm, changed by ecu if overheating

int hybrid_update; //variable for hybrid assist thread to know if it needs to check conditions and update assist state

// Define all base mutex locks for each major system
pthread_mutex_t engineLock;
pthread_mutex_t motionLock;
pthread_mutex_t fuelLock;
pthread_mutex_t ecuLock;
pthread_mutex_t hybridAssistLock;
pthread_mutex_t dashboardLock;

// mutex to protect the conditional variable for motion thread
pthread_mutex_t motionConditionalLock;
// condition variable for motion thread to check if engine is on
pthread_cond_t engineOnConditional;

// mutex to protect the conditional variable for hybrid assist thread
pthread_mutex_t hybridAssistConditionalLock;
// condition variable for hybrid assist thread to speed has changed
pthread_cond_t speedChangeConditional;

// mutex to protect the conditional variable for ECU
pthread_mutex_t ecuConditionalLock;

// mutex to protect event queue
pthread_mutex_t eventQueueLock;

// Define conditional variable for ECU to wait on for if there is a change to check
pthread_cond_t ecuConditional;

// Define conditional variable to signal event thread when there is a new event in the queue
pthread_cond_t eventQueueConditional;

// Define conditional variable to signal when the event queue is not full and can accept new events
pthread_cond_t eventQueueNotFullConditional;

// Condition variable for fuel thread to wait on when engine is off
pthread_cond_t fuelEngineOnConditional;

// Mutex to protect the fuel engine condition variable
pthread_mutex_t fuelEngineOnLock;

// helper function to notify ecu thread of a change that must be looked at, can be called by any subsytem
void notify_ecu(void)
{
  pthread_mutex_lock(&ecuConditionalLock);
  ecu_update = 1;
  pthread_cond_signal(&ecuConditional);
  pthread_mutex_unlock(&ecuConditionalLock);
}

// function to enqueue a new event into the event queue, will be called by any subsystem when they want to log an event
void enqueue_event(const struct Event *newEvent)
{
  // first lock the event queue mutex to ensure exclusive access to the queue
  pthread_mutex_lock(&eventQueueLock);

  // wait until there is open space in the event queue, if the queue is full we wait on the eventQueueNotFullConditional and release the lock while waiting
  while (event_queue_count >= 5)
  {
    pthread_cond_wait(&eventQueueNotFullConditional, &eventQueueLock);
  }

  // once there is space we add our event to the back of the queue
  event_queue[event_queue_count] = *newEvent;
  event_queue_count++;

  // then we signal that there is a new event in the queue and we unlock the mutex
  pthread_cond_signal(&eventQueueConditional);
  pthread_mutex_unlock(&eventQueueLock);
}

// THREADS NEEDED

// Engine Subsystem
// engine subsystem - updates RPM, temps, simulates idle to high
void *engine_thread(void *arg)
{

  int rpm_direction = 1; // 1 = rpms increase, -1 = rpms decrease

  while (1)
  {
    // critical
    pthread_mutex_lock(&engineLock);

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
      // also respect max_rpm limit set by ECU (overheat / low fuel protection)
      if (rpm < 1100)
        rpm = 1100;
      if (rpm > max_rpm)
        rpm = max_rpm;

      // rpm zones connect to temps, the higher zone the faster engine increases
      // in temp, vice versa but idle lets it cool
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

    pthread_mutex_unlock(&engineLock);

    // notify ECU that engine state/rpm/temp may have changed
    notify_ecu();

    // if engine is on, signal the fuel thread that it can consume fuel, and signal motion thread
    if (engine_state == 1)
    {
      pthread_mutex_lock(&fuelEngineOnLock);
      pthread_cond_signal(&fuelEngineOnConditional);
      pthread_mutex_unlock(&fuelEngineOnLock);
      pthread_mutex_lock(&motionConditionalLock);
      pthread_cond_signal(&engineOnConditional);
      pthread_mutex_unlock(&motionConditionalLock);
    }

    sleep(1); // update every second
  }

  return NULL;
}

// Motion Subsystem
// NOTES FOR WHEN WE MEET:
// wherever we have the engine turning on we need to set these for sync with motion
// pthread_mutex_lock(&motionConditionalLock);
// pthread_cond_signal(&engineOnConditional);
// pthread_mutex_unlock(&motionConditionalLock);
void *motion_thread(void *arg)
{
  //direction and initial speed are auto determined by command line
  while (1)
  {
    // Condition Variable
    // Wait until the engine is actually ON
    pthread_mutex_lock(&motionConditionalLock);
    while (engine_state == 0 && engine_off_decelerate == 0)
    {
      pthread_cond_wait(&engineOnConditional, &motionConditionalLock);
    }
    pthread_mutex_unlock(&motionConditionalLock);

    //================
    // CRITICAL SECTION
    //================

    // lock the motion mutex
    pthread_mutex_lock(&motionLock);

    // check if the engine is in deceleration mode
    if (engine_off_decelerate == 1)
    {
      // gradually reduce speed since engine is off
      if (speed > 0)
      {
        speed -= 2;
        if (speed < 0)
        {
          speed = 0;
        }

        // Distance still tracks so update
        float distance = (float)speed / 3600;
        distance_total += distance;
        distance_trip += distance;
      }
      else
      {
        // Speed is 0 so tell ECU that deceleration is complete
        engine_off_decelerate = 0;
      }
    }
    else
    {
      // Normal functionality of motion thread
      // enforce max speed set by ECU 
      //might need to decelerate based on ECU changes to max speed
      if (speed > max_speed)
      {
        //slowly decelrate
        speed -= 2;
        //check if speed is less than max speed
        if (speed < max_speed)
        {
          //clamp speed
          speed = max_speed;
        }
      } else {
        //make sure speed isnt 0 or max speed
        if (!(speed <= 0 || speed == max_speed))
        {
          //increment speed by the direction either 1 or -1
          speed += direction;
        }
      }

      // clamp speed to ensure its never less than 0
      if (speed <= 0)
      {
        speed = 0;
      }

      // calculate distance
      float distance = (float)speed / 3600;
      // update distance counters
      distance_total += distance;
      distance_trip += distance;
    }

    //unlock motion mutex
    pthread_mutex_unlock(&motionLock);
    //====================
    // END CRITICAL SECTION
    //====================

    // notify ecu that speed has changed so it can handle its functionality
    notify_ecu();

    // signal hybrid assist thread that speed has changed
    pthread_mutex_lock(&hybridAssistConditionalLock);
    //update hybrid update variable to 1 to notify the hybrid assist thread that it needs to check 
    //conditions and potentially update assist state based on the new speed
    hybrid_update = 1;
    pthread_cond_signal(&speedChangeConditional);
    pthread_mutex_unlock(&hybridAssistConditionalLock);

    sleep(1);
  }
  return NULL;
}

// Fuel Subsystem
void *fuel_thread(void *arg)
{
  while (1)
  {
    // crit sect. wait for engine to be on
    pthread_mutex_lock(&fuelEngineOnLock);

    // wait on the condition variable until signaled by the engine thread
    while (engine_state == 0)
    {
      pthread_cond_wait(&fuelEngineOnConditional, &fuelEngineOnLock);
    }

    pthread_mutex_unlock(&fuelEngineOnLock);

    pthread_mutex_lock(&fuelLock);

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

    // check if the fuel is 0 and shutoff the engine
    if (fuel <= 0)
    {
      engine_state = 0;
      fuel = 0;
    }

    // check if low fuel threshold just crossed — enqueue an event if so
    if (fuel < 0.7 && last_fuel_amount_logged > 0.7)
    {
      struct Event low_fuel_event;
      strncpy(low_fuel_event.title, "LOW FUEL", sizeof(low_fuel_event.title));
      snprintf(low_fuel_event.description, sizeof(low_fuel_event.description),
               "Fuel dropped below threshold: %.2f gal", fuel);
      low_fuel_event.timestamp = time_elapsed_total;
      enqueue_event(&low_fuel_event);
    }
    last_fuel_amount_logged = fuel;

    pthread_mutex_unlock(&fuelLock);

    // notify ECU that fuel has changed
    notify_ecu();

    sleep(1);
  }
  return NULL;
}

// ECU Subsystem
void *ecu_thread(void *arg)
{

  while (1)
  {

    // first thing is to lock the conditional lock or wait for it to be signaled
    pthread_mutex_lock(&ecuConditionalLock);

    // if there isn't an update, we wait for ecu conditional to be signaled and release the lock while waiting
    while (ecu_update == 0)
    {
      pthread_cond_wait(&ecuConditional, &ecuConditionalLock);
    }

    ecu_update = 0;                            // reset the variable after being signaled and waking up
    pthread_mutex_unlock(&ecuConditionalLock); // unlock the conditional lock after fully waking up

    // now below are all the things ECU will check through

    // if engine is off make sure rpm and speed is at 0
    if (engine_state == 0)
    {
      rpm = 0;
      rpm_zone = -1;
      if (speed > 0)
      {
        engine_off_decelerate = 1; // set the variable to indicate we need to decelerate the speed in motion thread
      }
    }

    if (engine_state == 1 && speed == 0)
    {
      rpm = 1200; // if engine is on but speed is 0, set to idle rpm
    }

    // determine the new rpm zone from rpm
    int old_zone = rpm_zone;
    int computed_zone;

    if (rpm >= 1100 && rpm < 1300)
    {
      computed_zone = 0; // IDLE
    }
    else if (rpm >= 1300 && rpm < 8000)
    {
      computed_zone = 1; // NORMAL
    }
    else if (rpm >= 8000 && rpm < 14500)
    {
      computed_zone = 2; // HIGH
    }
    else if (rpm >= 14500 && rpm <= 16500)
    {
      computed_zone = 3; // REDLINE
    }
    else
    {
      computed_zone = -1; // OFF / INVALID
    }

    // only log a change if the zone actually changed
    if (computed_zone != old_zone)
    {
      if (old_zone == -1 && computed_zone != -1)
      {
        newRPMZone = 1; // engine/off -> active zone
      }
      else if (computed_zone == -1 && old_zone != -1)
      {
        newRPMZone = 2; // active zone -> off
      }
      else if (computed_zone > old_zone)
      {
        newRPMZone = 1; // moved up
      }
      else if (computed_zone < old_zone)
      {
        newRPMZone = 2; // moved down
      }
      rpm_zone = computed_zone;
    }

    // this part updates the engine temperature zone based on the current engine
    // temperature
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

    // if engine is overheating or fuel is low, limit max speed & rpm
    if (engine_temp_zone == 3 || fuel < 0.7)
    {
      max_speed = 80;
      max_rpm = 8000;
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

    // hybrid assist system checks
    // if battery level is less than 20%, disable electric assist and set hybrid mode to none
    if (battery_level < 20)
    {
      electric_assist_allowed = 0;
      hybrid_mode = 0;
    }
    // if engine is off, disable electric assist
    else if (engine_state == 0)
    {
      electric_assist_allowed = 0;
    }
    // otherwise, allow electric assist based on conditions in the hybrid assist thread
    else
    {
      electric_assist_allowed = 1;
    }
  }

  return NULL;
}

// Hybrid Assist System Subsystem
void *hybrid_assist_thread(void *arg)
{
  // keep track of previous mode for ECU
  int previous_mode = hybrid_mode;

  while (1)
  {
    // Condition Variable
    // Wait for speed change from motion thread before applying hybrid assist logic
    pthread_mutex_lock(&hybridAssistConditionalLock);
    //loop while hybrid update is 0 to avoid waiting on the conditional if the variable is already set to 1 from a previous signal
    while (hybrid_update == 0) {
      pthread_cond_wait(&speedChangeConditional, &hybridAssistConditionalLock);
    }
    pthread_mutex_unlock(&hybridAssistConditionalLock);

    //================
    // CRITICAL SECTION
    //================

    pthread_mutex_lock(&hybridAssistLock);

    // only run hybrid assist if ECU has allowed it
    if (electric_assist_allowed == 1)
    {
      // check if engine is on and decelerating, if so turn on regeneration mode
      if (direction == -1 && engine_state == 1)
      {
        // regeneration
        hybrid_mode = 3;
        electric_assist_state = 0;
        charging_state = 1;
        battery_level += 0.07;
      }
      else if (direction == 1)
      {
        // handle all cases for hybrid assist when accelerating
        if (speed > 0 && speed <= 30)
        {
          // Low-Speed Electric Cruising
          // set the electric assist state to on
          electric_assist_state = 1;
          // set hybrid mode
          hybrid_mode = 1;
          // make sure charging state is off
          charging_state = 0;
          // vehicle uses battery assist so battery gets drained
          battery_level -= 0.05;
        }
        else if (speed > 30 && speed <= 60)
        {
          // Electric Assist During Acceleration
          electric_assist_state = 1;
          hybrid_mode = 2;
          charging_state = 0;
          battery_level -= 0.07;
        }
        else if (speed > 60 && speed <= 90)
        {
          electric_assist_state = 1;
          hybrid_mode = 2;
          charging_state = 0;
          battery_level -= 0.09;
        }
      }
    }
    else
    {
      // ECU has disable electric assist
      electric_assist_state = 0;
      charging_state = 0;
      hybrid_mode = 0;
    }

    // clamp battery level to ensure its never out of bounds
    if (battery_level > 100)
    {
      battery_level = 100;
    }
    if (battery_level < 0)
    {
      battery_level = 0;
    }

    // check hybrid mode for event logging
    if (hybrid_mode != previous_mode)
    {
      hybridAssistChange = 1;
      previous_mode = hybrid_mode;
    }

    // unlock hybrid assist mutex
    pthread_mutex_unlock(&hybridAssistLock);
    //====================
    // END CRITICAL SECTION
    //====================

    // notify ecu that hybrid assist status has changed so it can handle any necessary changes based on the new mode
    notify_ecu();

    sleep(1);
  }
  return NULL;
}

// Event Logging Subsystem
void *event_thread(void *arg)
{

  while (1)
  {
    pthread_mutex_lock(&eventQueueLock);

    while (event_queue_count == 0)
    {
      pthread_cond_wait(&eventQueueConditional, &eventQueueLock);
    }

    struct Event event_to_log = event_queue[0];
    for (int i = 1; i < event_queue_count; i++)
    {
      event_queue[i - 1] = event_queue[i];
    }
    event_queue_count--;

    if (event_count < MAX_EVENTS)
    {
      event_log[event_count] = event_to_log;
      event_count++;
    }
    else
    {
      memmove(&event_log[0], &event_log[1],
              sizeof(struct Event) * (MAX_EVENTS - 1));
      event_log[MAX_EVENTS - 1] = event_to_log;
    }

    pthread_cond_signal(&eventQueueNotFullConditional);
    pthread_mutex_unlock(&eventQueueLock);
  }

  return NULL;
}

// function to print a row of the dashboard
// got this from chatgpt, it essentially takes our text , formats it, and then
// calculates the width of the row and prints it properly with the left border,
// the text, then adds the right amount of spaces and then the right border at
// the end
void print_dash_row(const char *fmt, ...)
{
  char line[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  int width = 77; // inside width between the two border chars
  int len = strlen(line);

  if (len > width)
    len = width;

  printf("║");
  printf("%.*s", len, line);

  for (int i = len; i < width; i++)
  {
    printf(" ");
  }

  printf("║\n");
}

// function to print the dashboard
void print_dashboard(void)
{
  char *eng_status = (engine_state == 1) ? "ON" : "OFF";

  char *signal_label;
  if (signal_state == 1)
    signal_label = "< LEFT";
  else if (signal_state == 2)
    signal_label = "RIGHT >";
  else if (signal_state == 3)
    signal_label = "< HAZARD >";
  else
    signal_label = "OFF";

  char *headlight_label = (headlight_state == 1) ? "* ON" : "o OFF";

  char *temp_zone_label;
  if (engine_temp_zone == 0)
    temp_zone_label = "COLD";
  else if (engine_temp_zone == 1)
    temp_zone_label = "NORMAL";
  else if (engine_temp_zone == 2)
    temp_zone_label = "HOT";
  else
    temp_zone_label = "OVERHEAT";

  char *rpm_zone_label;
  if (rpm_zone == 0)
    rpm_zone_label = "IDLE";
  else if (rpm_zone == 1)
    rpm_zone_label = "NORMAL";
  else if (rpm_zone == 2)
    rpm_zone_label = "HIGH";
  else if (rpm_zone == 3)
    rpm_zone_label = "REDLINE";
  else
    rpm_zone_label = "OFF";

  char *hybrid_mode_label;
  if (hybrid_mode == 1)
    hybrid_mode_label = "CRUISING";
  else if (hybrid_mode == 2)
    hybrid_mode_label = "ASSIST";
  else if (hybrid_mode == 3)
    hybrid_mode_label = "REGENERATION";
  else
    hybrid_mode_label = "NONE";

  char *elec_assist_label = (electric_assist_state == 1) ? "ON" : "OFF";
  char *charging_label = (charging_state == 1) ? "ON" : "OFF";

  char fuel_bar[21];
  int fuel_filled = (int)((fuel / 4.7f) * 20);
  if (fuel_filled < 0)
    fuel_filled = 0;
  if (fuel_filled > 20)
    fuel_filled = 20;
  for (int i = 0; i < 20; i++)
    fuel_bar[i] = (i < fuel_filled) ? '#' : '-';
  fuel_bar[20] = '\0';

  char *low_fuel_label = (low_fuel_warning == 1) ? "!! LOW FUEL !!" : "";

  char batt_bar[21];
  int batt_filled = (battery_level * 20) / 100;
  if (batt_filled < 0)
    batt_filled = 0;
  if (batt_filled > 20)
    batt_filled = 20;
  for (int i = 0; i < 20; i++)
    batt_bar[i] = (i < batt_filled) ? '#' : '-';
  batt_bar[20] = '\0';

  printf("╔════════════════════════════════════════════════════════════════════"
         "═════════╗\n");
  print_dash_row("                                GEEKERS OS");
  printf("║════════════════════════════════════════════════════════════════════"
         "═════════║\n");

  print_dash_row("  ENGINE: %s  |  TEMP: %3d C (%s)  |  RPM: %5d (%s)",
                 eng_status, engine_temp, temp_zone_label, rpm, rpm_zone_label);

  print_dash_row("  SPEED: %3d mph", speed);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  FUEL   [%-20s]  %4.2f gal  %s", fuel_bar, fuel,
                 low_fuel_label);

  print_dash_row("  DIST TOTAL: %8.1f mi        DIST TRIP: %6.1f mi",
                 distance_total, distance_trip);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  TIME ELAPSED (TOTAL): %02d:%02d:%02d",
                 time_elapsed_total.hours, time_elapsed_total.minutes,
                 time_elapsed_total.seconds);

  print_dash_row("  TIME ELAPSED (TRIP):  %02d:%02d:%02d",
                 time_elapsed_trip.hours, time_elapsed_trip.minutes,
                 time_elapsed_trip.seconds);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  SIGNAL: %s       HEADLIGHT: %s", signal_label,
                 headlight_label);

  printf("║═══════════════════════════ HYBRID ASSIST SYSTEM "
         "════════════════════════════║\n");

  print_dash_row("  BATTERY  [%-20s]  %3.2f%%", batt_bar, battery_level);

  print_dash_row("  ELECTRIC ASSIST: %s    CHARGING: %s    HYBRID MODE: %s",
                 elec_assist_label, charging_label, hybrid_mode_label);

  printf("║═══════════════════════════════ EVENT LOG "
         "═══════════════════════════════════║\n");

  int start = (event_count >= 3) ? event_count - 3 : 0;
  int display_num = 1;

  for (int i = start; i < event_count; i++)
  {
    print_dash_row("  %d. [%02d:%02d:%02d] %s", display_num,
                   event_log[i].timestamp.hours, event_log[i].timestamp.minutes,
                   event_log[i].timestamp.seconds, event_log[i].description);
    display_num++;
  }

  for (int i = display_num; i <= 3; i++)
  {
    print_dash_row("  %d. ---", i);
  }

  printf("╚════════════════════════════════════════════════════════════════════"
         "═════════╝\n");
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

void *time_thread(void *arg)
{
  while (1)
  {
    time_elapsed_trip.seconds++;
    if (time_elapsed_trip.seconds == 60)
    {
      time_elapsed_trip.seconds = 0;
      time_elapsed_trip.minutes++;
    }
    if (time_elapsed_trip.minutes == 60)
    {
      time_elapsed_trip.minutes = 0;
      time_elapsed_trip.hours++;
    }

    time_elapsed_total.seconds++;
    if (time_elapsed_total.seconds == 60)
    {
      time_elapsed_total.seconds = 0;
      time_elapsed_total.minutes++;
    }
    if (time_elapsed_total.minutes == 60)
    {
      time_elapsed_total.minutes = 0;
      time_elapsed_total.hours++;
    }
    sleep(1);
  }
  return NULL;
}

void init_from_args(int argc, char *argv[])
{
  if (argc < 6)
  {
    printf("Usage: %s <RPM> <ENGINE_STATE> <SPEED> <FUEL_LEVEL> <A/D> <BATTERY_LEVEL>\n", argv[0]);
    exit(1);
  }

  rpm = atoi(argv[1]);
  engine_state = atoi(argv[2]);
  speed = atoi(argv[3]);
  fuel = atof(argv[4]);                     // accepts gallons directly
  direction = (argv[5][0] == 'A') ? 1 : -1; // A = accelerating, D = decelerating
  battery_level = atof(argv[6]);
}

// MAIN FUNCTION
int main(int argc, char *argv[])
{
  srand(time(NULL));

  init_from_args(argc, argv);

  rpm_zone = 0;
  engine_temp = 45; // cold start
  engine_temp_zone = 0;

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
  electric_assist_state = 1;
  electric_assist_allowed = 0;
  charging_state = 0;
  hybrid_mode = 2; // hybrid assist

  event_count = 0;
  event_queue_count = 0;
  newRPMZone = 0;
  hybridAssistChange = 0;
  wheelSlipDetected = 0;
  last_fuel_amount_logged = -1;
  last_battery_amount_logged = -1;
  ecu_update = 1; // set to 1 so ecu begins by checking initial conditions
  engine_off_decelerate = 0;
  max_speed = 200;
  max_rpm = 16500;
  hybrid_update = 1; // set to 1 so hybrid assist thread checks conditions on startup

  // initialize all mutex's
  pthread_mutex_init(&engineLock, NULL);
  pthread_mutex_init(&motionLock, NULL);
  pthread_mutex_init(&fuelLock, NULL);
  pthread_mutex_init(&ecuLock, NULL);
  pthread_mutex_init(&hybridAssistLock, NULL);
  pthread_mutex_init(&eventQueueLock, NULL);
  pthread_mutex_init(&dashboardLock, NULL);
  pthread_mutex_init(&ecuConditionalLock, NULL);
  pthread_mutex_init(&motionConditionalLock, NULL);
  pthread_mutex_init(&hybridAssistConditionalLock, NULL);
  pthread_mutex_init(&fuelEngineOnLock, NULL);

  // initialize conditionals
  pthread_cond_init(&ecuConditional, NULL);
  pthread_cond_init(&eventQueueConditional, NULL);
  pthread_cond_init(&eventQueueNotFullConditional, NULL);
  pthread_cond_init(&engineOnConditional, NULL);
  pthread_cond_init(&speedChangeConditional, NULL);
  pthread_cond_init(&fuelEngineOnConditional, NULL);

  // create threads
  pthread_t engine_tid, motion_tid, fuel_tid, ecu_tid, hybrid_assist_tid,
      event_tid, dashboard_tid, time_tid;

  pthread_create(&engine_tid, NULL, engine_thread, NULL);
  pthread_create(&motion_tid, NULL, motion_thread, NULL);
  pthread_create(&fuel_tid, NULL, fuel_thread, NULL);
  pthread_create(&ecu_tid, NULL, ecu_thread, NULL);
  pthread_create(&hybrid_assist_tid, NULL, hybrid_assist_thread, NULL);
  pthread_create(&event_tid, NULL, event_thread, NULL);
  pthread_create(&dashboard_tid, NULL, dashboard_thread, NULL);
  pthread_create(&time_tid, NULL, time_thread, NULL);
  // Wait (threads run indefinitely)
  pthread_join(engine_tid, NULL);
  pthread_join(motion_tid, NULL);
  pthread_join(fuel_tid, NULL);
  pthread_join(ecu_tid, NULL);
  pthread_join(hybrid_assist_tid, NULL);
  pthread_join(event_tid, NULL);
  pthread_join(dashboard_tid, NULL);
  pthread_join(time_tid, NULL);
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
