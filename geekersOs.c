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
// Phase 3 Includes
#include <termios.h> //controls i/o with terminal
#include <sys/select.h>
#include <ctype.h>

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

// enum for system state
typedef enum
{
  ENGINE_OFF_STATE,
  IDLE_STATE,
  NORMAL_STATE,
  HIGH_LOAD_STATE,
  CRITICAL_STATE
} SystemState;

SystemState system_state;

// Phase 3 Globals
// volatile keyword given by chatgpt to ensure this variable is never cached by a thread and is always read from main memory
// so that a thread never ends up in an infinite loop if it didnt read variable correctly
volatile int running = 1;         // acts as a kill switch for the while (1) loops to appropriately terminate the program on 'quit'
struct termios original_terminal; // struct variable to store the original terminal settings so we can restore them on exit

// Engine State Variable
pthread_mutex_t engineStateLock; // mutex lock for engine state
int engine_state;

// ENGINE SUBSYSTEM VARIABLES
pthread_mutex_t engineLock; // mutex lock for engine subsystem
int refueling = 0;
int rpm;              // 0 if engine off, 1100-16500 if on
int rpm_zone;         // 0 = idle, 1 = normal, 2 = high, 3 = redline
int engine_temp;      // temp in degrees Celsius
int engine_temp_zone; // 0 = cold, 1 = normal, 2 = hot, 3 = overheat
int max_rpm;          // variable to limit max rpm, changed by ecu if overheating

// MOTION SUBSYSTEM VARIABLES
pthread_mutex_t motionLock;         // mutex lock for motion subsystem
pthread_cond_t engineOnConditional; // condition variable for motion thread to check if engine is on
int speed;                          // in mph, 0 if engine off, 0-200 if on
float distance_total;               // in miles
float distance_trip;                // in miles
int direction;                      // direction global variable for motion and hybrid assist
int engine_off_decelerate;          // variable to track if the engine was just turned off and speed isn't 0, we must decelarate
int max_speed;                      // variable to limit max speed, changed by ecu if overheating

// FUEL SUBSYSTEM VARIABLES
pthread_mutex_t fuelLock;               // mutex lock for fuel subsystem
pthread_cond_t fuelEngineOnConditional; // Condition variable for fuel thread to wait on when engine is off
float fuel;                             // in gallons, 0-4.7
int low_fuel_warning;                   // 0 = no warning, 1 = low fuel warning
float last_fuel_amount_logged;          // variable to track what the last low fuel warning was logged at

// HYBRID SUBSYSTEM VARIABLES
pthread_mutex_t hybridAssistLock;            // mutex lock for hybrid assist subsystem
pthread_mutex_t hybridAssistConditionalLock; // mutex to protect the conditional variable for hybrid assist thread
pthread_cond_t speedChangeConditional;       // condition variable for hybrid assist thread to speed has changed
float battery_level;                         // in percentage, 0-100
int electric_assist_state;                   // 0 = off, 1 = on
int electric_assist_allowed;                 // 0 = not allowed, 1 = allowed based on conditions
int charging_state;                          // 0 = off, 1 = on
int hybrid_mode;                             // 0 = none, 1 = cruising, 2 = assist, 3 = regeneration
int hybrid_update;                           // variable for hybrid assist thread to know if it needs to check conditions and update assist state
int battery_mode_on;                         // variable for input "B" for phase 3 (1 means battery mode is on, 0 means battery mode is off)

// DASHBOARD SUBSYSTEM VARIABLES
pthread_mutex_t dashboardLock;  // mutex lock for dashboard subsystem
int left_signal_on;             // variable to track if left signal is on for blinking behavior
int right_signal_on;            // variable to track if right signal is on for blinking behavior
int hazard_state;               // 0 = off, 1 = on
int headlight_state;            // 0 = off, 1 = on
struct Time time_elapsed_total; // in seconds
struct Time time_elapsed_trip;  // in seconds

// EVENT SUBSYSTEM VARIABLES
pthread_mutex_t eventQueueLock;              // mutex to protect event queue
pthread_cond_t eventQueueConditional;        // Define conditional variable to signal event thread when there is a new event in the queue
pthread_cond_t eventQueueNotFullConditional; // Define conditional variable to signal when the event queue is not full and can accept new events
struct Event event_log[MAX_EVENTS];          // array to store event logs
int event_count;                             // number of events in the log
struct Event event_queue[5];                 // queue to store events that need to be processed by the event thread, 5 events max in the queue
int event_queue_count;                       // number of events currently in the queue

// ECU SUBSYSTEM VARIABLES
pthread_mutex_t ecuLock;            // mutex lock for ECU subsystem
pthread_mutex_t ecuConditionalLock; // mutex to protect the conditional variable for ECU
pthread_cond_t ecuConditional;      // Define conditional variable for ECU to wait on for if there is a change to check
int ecu_update;                     // variable to track if something has changed like fuel or speed so the ecu must go through and check the system status

// CHANGE VARIABLES FOR EVENT LOGGING
int limiting_speed_rpm_event; // variable to track if we just limited speed and rpm so we can log an event
int wheelSlipDetected;        // variable to store if wheel slip is detected, 0 = no
                              // slip, 1 = slip detected

// TERMINAL HELPER FUNCTIONS
// setup terminal with new modifications
void setup_terminal(void)
{
  // create a new terminal instance with termios
  struct termios new_terminal;
  // get the current terminal settings and store them to the global variable
  // for restoration later
  tcgetattr(STDIN_FILENO, &original_terminal);

  // copy original settings into the new_terminal struct for modification
  new_terminal = original_terminal;

  // disable canonical mode (ICANON)
  // normally input is buffered until ENTER is pressed
  // turning this off means each key press is available immediately
  new_terminal.c_lflag &= ~ICANON;

  // disable echo (ECHO)
  // prevents typed characters (W, S, etc.) from appearing on screen
  new_terminal.c_lflag &= ~ECHO;

  // set minimum number of characters for read()
  // VMIN = 0, read() will NOT wait for input (non-blocking)
  new_terminal.c_cc[VMIN] = 0;

  // set timeout for read()
  // VTIME = 0, read() returns immediately (no waiting)
  new_terminal.c_cc[VTIME] = 0;

  // apply the modified terminal settings immediately
  // TCSANOW = apply changes right now
  tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);
}

// restore terminal to original state stored in global variable
void restore_terminal(void)
{
  // restore the terminal back to its original settings
  tcsetattr(STDIN_FILENO, TCSANOW, &original_terminal);
}

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

  // signal that there is a new event in the queue and we unlock the mutex
  pthread_cond_signal(&eventQueueConditional);
  pthread_mutex_unlock(&eventQueueLock);
}

// Input Subsystem
// handles all keyboard inputs for phase 3
void *input_thread(void *arg)
{
  // loop infinitely while the program is running
  while (running)
  {
    // set up the file descriptor set for select() to monitor stdin for input
    fd_set readfds;
    // set up the timeout for select() so we can periodically check for input without blocking indefinitely
    struct timeval timeout;
    // variable to store the key that was pressed
    char key;

    // initialize the file descriptor set to monitor stdin
    FD_ZERO(&readfds);
    // add stdin file descriptor to the set
    FD_SET(STDIN_FILENO, &readfds);

    // set the timeout to 0.1 seconds so we check for input every 0.1 seconds
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // check every 0.1 seconds

    // define a ready variable to store the result of select()
    // it will be > 0 if there is input ready to be read, 0 if timeout occurred
    // and < 0 if an error occurred
    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

    // if there is an input to be read, read it and handle the corresponding action
    if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds))
    {
      // read a single character from stdin, since we set VMIN = 0 and VTIME = 0 in our terminal settings
      if (read(STDIN_FILENO, &key, 1) > 0)
      {
        // convert the key to uppercase to simplify handling both lowercase and uppercase inputs
        key = toupper(key);

        // use a switch case to handle conditions
        switch (key)
        {
        case 'W':
          // TODO: accelerate
          pthread_mutex_lock(&motionLock);
          direction = 1; // set direction to 1 for accelerating
          pthread_mutex_unlock(&motionLock);
          notify_ecu(); // notify ecu of a change so it can check conditions and potentially update system state
          break;

        case 'S':
          // TODO: decelerate
          pthread_mutex_lock(&motionLock);
          direction = -1; // set direction to -1 for decelerating
          pthread_mutex_unlock(&motionLock);
          notify_ecu(); // notify ecu of a change so it can check conditions and potentially update system state
          break;

        case 'C':
          // TODO: cruise
          pthread_mutex_lock(&motionLock);
          direction = 0; // set direction to 0 for cruising
          pthread_mutex_unlock(&motionLock);
          notify_ecu(); // notify ecu of a change so it can check conditions and potentially update system state
          break;

        case 'A':
          // TODO: left signal
          pthread_mutex_lock(&dashboardLock);
          left_signal_on = left_signal_on == 1 ? 0 : 1; // toggle left signal on/off, if its currently on set to off, if its currently off set to left signal
          pthread_mutex_unlock(&dashboardLock);
          break;

        case 'D':
          // TODO: right signal
          pthread_mutex_lock(&dashboardLock);
          right_signal_on = right_signal_on == 1 ? 0 : 1; // toggle right signal on/off, if its currently on set to off, if its currently off set to right signal
          pthread_mutex_unlock(&dashboardLock);
          break;

        case 'Z':
          // TODO: hazards
          pthread_mutex_lock(&dashboardLock);
          hazard_state = hazard_state == 1 ? 0 : 1; // toggle hazards on/off, if its currently on set to off, if its currently off set to on
          pthread_mutex_unlock(&dashboardLock);
          break;

        case 'H':
          // TODO: headlight
          pthread_mutex_lock(&dashboardLock);
          headlight_state = headlight_state == 1 ? 0 : 1; // toggle headlight on/off, if its currently on set to off, if its currently off set to on
          pthread_mutex_unlock(&dashboardLock);
          break;

        case 'F':
        {
          pthread_mutex_lock(&engineStateLock);
          int eng = engine_state;
          pthread_mutex_unlock(&engineStateLock);

          pthread_mutex_lock(&motionLock);
          int spd = speed;
          pthread_mutex_unlock(&motionLock);

          if (eng == 0 && spd == 0)
          {
            pthread_mutex_lock(&fuelLock);
            refueling = 1;
            pthread_mutex_unlock(&fuelLock);
            sleep(5);
            pthread_mutex_lock(&fuelLock);
            fuel = 4.7f;
            low_fuel_warning = 0;
            last_fuel_amount_logged = -1;
            refueling = 0;
            pthread_mutex_unlock(&fuelLock);

            notify_ecu();
          }
          break;
        }

        case 'I':
          pthread_mutex_lock(&fuelLock);
          float current_fuel = fuel;
          int is_refueling = refueling;
          pthread_mutex_unlock(&fuelLock);
          pthread_mutex_lock(&engineStateLock);
          if (engine_state == 0 && current_fuel > 0 && is_refueling == 0)
          {
            pthread_mutex_lock(&engineLock);
            engine_state = 1;
            rpm = 1200;
            pthread_mutex_unlock(&engineLock);
          }
          pthread_mutex_unlock(&engineStateLock);
          notify_ecu();
          break;

        case 'K':
          // kill switch
          // lock the engine mutex
          pthread_mutex_lock(&engineStateLock);

          // only change engine state and rpm if engine is on
          if (engine_state == 1)
          {
            pthead_mutex_lock(&engineLock);
            engine_state = 0;
            rpm = 0;
            rpm_zone = -1;
            pthread_mutex_unlock(&engineLock);
          }

          pthread_mutex_unlock(&engineLock);

          // lock motion thread
          pthread_mutex_lock(&motionLock);

          // stop motion
          speed = 0;
          direction = 0;
          // reset current trip distance
          distance_trip = 0.0f;
          // since speed is 0, total distance will naturally pause
          engine_off_decelerate = 0;

          pthread_mutex_unlock(&motionLock);

          // lock the dashboard mutex
          pthread_mutex_lock(&dashboardLock);

          // reset current trip time
          time_elapsed_trip.hours = 0;
          time_elapsed_trip.minutes = 0;
          time_elapsed_trip.seconds = 0;

          pthread_mutex_unlock(&dashboardLock);

          // lock hybrid assist mutext
          pthread_mutex_lock(&hybridAssistLock);

          // turn off battery mode / hybrid assist when kill switch is pressed
          battery_mode_on = 0;
          electric_assist_state = 0;
          charging_state = 0;
          hybrid_mode = 0;

          pthread_mutex_unlock(&hybridAssistLock);

          // wake hybrid thread so dashboard updates hybrid status
          pthread_mutex_lock(&hybridAssistConditionalLock);
          hybrid_update = 1;
          pthread_cond_signal(&speedChangeConditional);
          pthread_mutex_unlock(&hybridAssistConditionalLock);

          // notify ecu
          notify_ecu();
          break;

        case 'B':
          // lock hybrid assist mutex to safely update the battery mode variable that is used in the hybrid assist thread
          pthread_mutex_lock(&hybridAssistLock);
          // allow toggling battery mode on and off if the battery level is above 0
          if (battery_level > 0)
          {
            battery_mode_on = !battery_mode_on;
          }
          // if empty then we cant use battery
          else
          {
            battery_mode_on = 0;
          }
          // unlock mutext
          pthread_mutex_unlock(&hybridAssistLock);

          // alert hybrid assist thread that there was a change
          pthread_mutex_lock(&hybridAssistConditionalLock);
          hybrid_update = 1;
          pthread_cond_signal(&speedChangeConditional);
          pthread_mutex_unlock(&hybridAssistConditionalLock);

          // notify ecu
          notify_ecu();
          break;

        case 'Q':
          // set running to 0 to kill all threads
          running = 0;

          // destroy every mutex
          pthread_mutex_destroy(&engineLock);
          pthread_mutex_destroy(&motionLock);
          pthread_mutex_destroy(&fuelLock);
          pthread_mutex_destroy(&ecuLock);
          pthread_mutex_destroy(&hybridAssistLock);
          pthread_mutex_destroy(&eventQueueLock);
          pthread_mutex_destroy(&dashboardLock);
          pthread_mutex_destroy(&ecuConditionalLock);
          pthread_mutex_destroy(&hybridAssistConditionalLock);
          pthread_mutex_destroy(&engineStateLock);

          // destroy all conditionals
          pthread_cond_destroy(&ecuConditional);
          pthread_cond_destroy(&eventQueueConditional);
          pthread_cond_destroy(&eventQueueNotFullConditional);
          pthread_cond_destroy(&engineOnConditional);
          pthread_cond_destroy(&speedChangeConditional);
          pthread_cond_destroy(&fuelEngineOnConditional);

          restore_terminal(); // restore terminal settings before exiting

          // break out of loop
          break;
        }
      }
    }
  }

  return NULL;
}

// Engine Subsystem
// engine subsystem - updates RPM, temps, simulates idle to high
void *engine_thread(void *arg)
{
  int rpm_direction = 1; // 1 = rpms increase, -1 = rpms decrease

  while (running)
  {
    // critical
    pthread_mutex_lock(&engineStateLock);
    pthread_mutex_lock(&engineLock);

    // check if the battery mode is on
    // if it is set the rpm to 0
    if (battery_mode_on == 1)
    {
      rpm = 0;
      // cool engine while battery is being used since 0 rpm
      // 45 degrees is cold start so use 50 as a baseline
      if (engine_temp > 50)
      {
        engine_temp -= 1;
      }
    }
    else if (engine_state == 0)
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
      // RPM tied to speed
      int base_idle = 1100;
      int scale = 60;

      pthread_mutex_lock(&motionLock);
      int current_speed = speed;
      pthread_mutex_unlock(&motionLock);

      // only update RPM if speed changed from 0 OR RPM is out of sync
      int target_rpm = base_idle + (current_speed * scale);

      if (rpm < 1100)
      {
        rpm = 1200; // if rpm is below idle, set to idle rpm
      }
      else if (rpm > max_rpm)
      {
        rpm -= 150; // if rpm is above max rpm, decelerate it by 150 until it is at or below the max rpm set by the ECU
      }
      else
      {
        if (rpm > target_rpm)
        {
          rpm -= 150; // if rpm is above target rpm based on speed, decelerate it by 150 until it is at or below the target rpm
        }
        else
        {
          rpm += 150; // if rpm is below target rpm based on speed, accelerate it by 150 until it is at or above the target rpm
        }
      }

      // rpm zones control temps, the higher zone the faster engine increases
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
        else if (engine_temp > 92)
        {
          engine_temp -= 1;
        }
      }
      else if (rpm_zone == 0)
      {
        if (engine_temp > 75)
        {
          engine_temp -= 1;
        }
        else if (engine_temp < 75)
        {
          engine_temp += 1;
        }
      }
      // max of 130 degrees
      if (engine_temp > 130)
        engine_temp = 130;
    }

    pthread_mutex_unlock(&engineStateLock);
    pthread_mutex_unlock(&engineLock);

    // notify ECU that engine state/rpm/temp may have changed
    notify_ecu();

    // if engine is on, signal the fuel thread that it can consume fuel, and signal motion thread
    pthread_mutex_lock(&engineStateLock);
    if (engine_state == 1)
    {
      pthread_cond_signal(&fuelEngineOnConditional);
      pthread_cond_signal(&engineOnConditional);
    }
    pthread_mutex_unlock(&engineStateLock);

    sleep(1); // update every second
  }

  return NULL;
}

// Motion Subsystem
void *motion_thread(void *arg)
{
  // direction and initial speed are auto determined by command line
  while (running)
  {
    // Condition Variable
    // Wait until the engine is actually ON
    pthread_mutex_lock(&engineStateLock);

    while (engine_state == 0 && engine_off_decelerate == 0)
    {
      pthread_cond_wait(&engineOnConditional, &engineStateLock);
    }
    pthread_mutex_unlock(&engineStateLock);

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
      // might need to decelerate based on ECU changes to max speed
      if (speed > max_speed)
      {
        // slowly decelrate
        speed -= 2;
        // check if speed is less than max speed
        if (speed < max_speed)
        {
          // clamp speed
          speed = max_speed;
        }
      }
      else
      {
        // make sure speed isnt 0 or max speed
        if (direction == 1)
        {
          speed += (max_speed - speed) * 0.2 * 1;
        }
        else if (direction == -1 && speed > 0)
        {
          speed -= (speed - 0) * 0.1 * 1;
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

    // unlock motion mutex
    pthread_mutex_unlock(&motionLock);
    // notify ecu that speed has changed so it can handle its functionality
    notify_ecu();
    // signal hybrid assist thread that speed has changed
    pthread_mutex_lock(&hybridAssistConditionalLock);
    // update hybrid update variable to 1 to notify the hybrid assist thread that it needs to check
    // conditions and potentially update assist state based on the new speed
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
  while (running)
  {
    // crit sect. wait for engine to be on
    pthread_mutex_lock(&engineStateLock);

    // wait on the condition variable until signaled by the engine thread
    while (engine_state == 0)
    {
      pthread_cond_wait(&fuelEngineOnConditional, &engineStateLock);
    }

    pthread_mutex_unlock(&engineStateLock);

    pthread_mutex_lock(&engineLock); // also lock engine since we need to check rpm zones, maintain locking order present throughout all threads
    pthread_mutex_lock(&fuelLock);

    if (battery_mode_on == 1)
    {
      fuel -= 0.001; // if battery mode is on, reduce fuel by almost 0
    }
    else if (rpm_zone == 0)
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
      pthread_mutex_lock(&engineStateLock);
      engine_state = 0;
      pthread_mutex_unlock(&engineStateLock);
      fuel = 0;
    }

    pthread_mutex_unlock(&engineLock); // unlock engine since we no longer need to check engine state

    // check if low fuel threshold just crossed, enqueue an event if so
    if (fuel < 0.7 && (last_fuel_amount_logged > 0.7 || abs(fuel - last_fuel_amount_logged) > 0.1))
    {
      struct Event low_fuel_event;
      strncpy(low_fuel_event.title, "LOW FUEL", sizeof(low_fuel_event.title));
      snprintf(low_fuel_event.description, sizeof(low_fuel_event.description), "Fuel dropped below threshold: %.2f gal", fuel);
      low_fuel_event.timestamp = time_elapsed_total;
      enqueue_event(&low_fuel_event);
      last_fuel_amount_logged = fuel;
    }
    else if (fuel >= 0.7)
    {
      last_fuel_amount_logged = fuel; // update the last fuel amount logged when fuel goes back above the threshold so we can log again if it drops below again
    }

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

  while (running)
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

    // lock engine and motion to check rpm, ropm zones, speeds, etc.
    pthread_mutex_lock(&engineLock);
    pthread_mutex_lock(&motionLock);
    pthread_mutex_lock(&engineStateLock);

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

    pthread_mutex_unlock(&engineStateLock);

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
      if (computed_zone > old_zone)
      {
        struct Event rpm_zone_event;
        strncpy(rpm_zone_event.title, "RPM ZONE INCREASE", sizeof(rpm_zone_event.title));
        snprintf(rpm_zone_event.description, sizeof(rpm_zone_event.description), "RPM zone increased from %d to %d", old_zone, computed_zone);
        rpm_zone_event.timestamp = time_elapsed_total;
        enqueue_event(&rpm_zone_event);
      }
      else
      {
        struct Event rpm_zone_event;
        strncpy(rpm_zone_event.title, "RPM ZONE DECREASE", sizeof(rpm_zone_event.title));
        snprintf(rpm_zone_event.description, sizeof(rpm_zone_event.description), "RPM zone decreased from %d to %d", old_zone, computed_zone);
        rpm_zone_event.timestamp = time_elapsed_total;
        enqueue_event(&rpm_zone_event);
      }
      rpm_zone = computed_zone;
    }

    // this part updates the engine temperature zone based on the current engine
    // temperature
    int old_temp_zone = engine_temp_zone;
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

    if (old_temp_zone != engine_temp_zone)
    {
      if (engine_temp_zone == 3)
      {
        struct Event temp_zone_event;
        strncpy(temp_zone_event.title, "ENGINE OVERHEATING", sizeof(temp_zone_event.title));
        snprintf(temp_zone_event.description, sizeof(temp_zone_event.description), "Engine is overheating (zone %d to %d)", old_temp_zone, engine_temp_zone);
        temp_zone_event.timestamp = time_elapsed_total;
        enqueue_event(&temp_zone_event);
      }
      else if (engine_temp_zone == 2)
      {
        struct Event temp_zone_event;
        strncpy(temp_zone_event.title, "ENGINE HOT", sizeof(temp_zone_event.title));
        snprintf(temp_zone_event.description, sizeof(temp_zone_event.description), "Engine temperature is high (zone %d to %d)", old_temp_zone, engine_temp_zone);
        temp_zone_event.timestamp = time_elapsed_total;
        enqueue_event(&temp_zone_event);
      }
    }

    // lock fuel to check fuel levels and potentially log low fuel event
    pthread_mutex_lock(&fuelLock);
    // if engine is overheating or fuel is low, limit max speed & rpm
    if (engine_temp_zone == 3 || fuel < 0.7)
    {
      max_speed = 80;
      max_rpm = 8000;
      if (limiting_speed_rpm_event == 0)
      {
        struct Event limiting_event;
        strncpy(limiting_event.title, "LIMITING SPEED AND RPM", sizeof(limiting_event.title));
        snprintf(limiting_event.description, sizeof(limiting_event.description), "ECU is limiting speed and rpm due to %s", (engine_temp_zone == 3) ? "overheating" : "low fuel");
        limiting_event.timestamp = time_elapsed_total;
        enqueue_event(&limiting_event);
        limiting_speed_rpm_event = 1; // set variable to indicate we just limited speed and rpm so we can log an event in the dashboard thread
      }
    }
    else
    {
      max_speed = 200;
      max_rpm = 16500;
      limiting_speed_rpm_event = 0; // reset variable since we are no longer limiting speed and rpm
    }

    pthread_mutex_unlock(&motionLock); // unlock motion since we no longer need to check speed
    pthread_mutex_unlock(&engineLock); // unlock engine since we no longer need to checkrpm or temps

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

    pthread_mutex_unlock(&fuelLock); // unlock fuel since we no longer need to check fuel levels

    // have ECU model system state with enum
    // not sure if this is correct as we never do anything with the enum
    // maybe in future phase
    // lock engine, motion, and ecu to update system state since it relies on all of those variables, maintain locking order
    pthread_mutex_lock(&engineLock);
    pthread_mutex_lock(&motionLock);
    pthread_mutex_lock(&ecuLock);
    pthread_mutex_lock(&engineStateLock);
    if (engine_state == 0)
    {
      system_state = ENGINE_OFF_STATE;
    }
    else if (engine_temp_zone == 3)
    {
      system_state = CRITICAL_STATE;
    }
    else if (rpm_zone == 2 || rpm_zone == 3)
    {
      system_state = HIGH_LOAD_STATE;
    }
    else if (speed == 0)
    {
      system_state = IDLE_STATE;
    }
    else
    {
      system_state = NORMAL_STATE;
    }
    pthread_mutex_unlock(&engineLock);
    pthread_mutex_unlock(&motionLock);
    pthread_mutex_unlock(&ecuLock);

    pthread_mutex_lock(&hybridAssistLock); // lock hybrid assist to check battery levels and hybrid assist status
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

    pthread_mutex_unlock(&hybridAssistLock); // unlock hybrid assist since we no longer need to check rpm zones or temps
    pthread_mutex_unlock(&engineStateLock);
  }

  return NULL;
}

// Hybrid Assist System Subsystem
void *hybrid_assist_thread(void *arg)
{
  // keep track of previous mode for ECU
  int previous_mode = hybrid_mode;

  while (running)
  {
    // Condition Variable
    // Wait for speed change from motion thread before applying hybrid assist logic
    pthread_mutex_lock(&hybridAssistConditionalLock);
    // loop while hybrid update is 0 to avoid waiting on the conditional if the variable is already set to 1 from a previous signal
    while (hybrid_update == 0)
    {
      pthread_cond_wait(&speedChangeConditional, &hybridAssistConditionalLock);
    }
    // set hybrid update back to 0 after waking up to wait for the next signal
    hybrid_update = 0;
    pthread_mutex_unlock(&hybridAssistConditionalLock);

    pthread_mutex_lock(&motionLock); // for checking speed
    pthread_mutex_lock(&hybridAssistLock);

    // Battery mode logic from Phase 3
    // NEED TO CLARIFY WITH DR. K HOW THIS IS SUPPOSED TO WORK EXACTLY
    if (battery_mode_on == 1)
    {
      electric_assist_state = 1;
      charging_state = 0;
      hybrid_mode = 1; // using CRUISING as battery-drive mode

      // drain battery while battery mode is active
      battery_level -= 0.15;

      // once battery reaches zero, battery mode shuts off automatically
      if (battery_level <= 0)
      {
        battery_level = 0;
        battery_mode_on = 0;
        electric_assist_state = 0;
        charging_state = 0;
        hybrid_mode = 0;
      }
    }

    // normal hybrid assist logic
    else
    {
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
          else if (speed > 60 && speed <= max_speed)
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
    }

    pthread_mutex_unlock(&motionLock); // unlock motion since we no longer need to check speed

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
      if (hybrid_mode == 0)
      {
        struct Event hybrid_event;
        strncpy(hybrid_event.title, "HYBRID ASSIST OFF", sizeof(hybrid_event.title));
        snprintf(hybrid_event.description, sizeof(hybrid_event.description), "Hybrid assist turned off, previous mode was %d", previous_mode);
        hybrid_event.timestamp = time_elapsed_total;
        enqueue_event(&hybrid_event);
      }
      else
      {
        struct Event hybrid_event;
        strncpy(hybrid_event.title, "HYBRID MODE CHANGE", sizeof(hybrid_event.title));
        snprintf(hybrid_event.description, sizeof(hybrid_event.description), "Hybrid mode changed from %d to %d", previous_mode, hybrid_mode);
        hybrid_event.timestamp = time_elapsed_total;
        enqueue_event(&hybrid_event);
      }
      previous_mode = hybrid_mode;
    }

    // unlock hybrid assist mutex
    pthread_mutex_unlock(&hybridAssistLock);

    // notify ecu that hybrid assist status has changed so it can handle any necessary changes based on the new mode
    notify_ecu();

    sleep(1);
  }
  return NULL;
}

// Event Logging Subsystem
void *event_thread(void *arg)
{

  while (running)
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
  pthread_mutex_lock(&engineStateLock);
  char *eng_status = (engine_state == 1) ? "ON" : "OFF";
  pthread_mutex_unlock(&engineStateLock);

  pthread_mutex_lock(&dashboardLock);
  char *signal_label;
  if (hazard_state == 1)
    signal_label = "<  >";
  else if (left_signal_on == 1 && right_signal_on == 1)
    signal_label = "<  >";
  else if (left_signal_on == 1)
    signal_label = "<";
  else if (right_signal_on == 1)
    signal_label = ">";
  else
    signal_label = "";

  char *headlight_label = (headlight_state == 1) ? "ON" : "OFF";
  struct Time local_time_elapsed_total = time_elapsed_total; // copy total time elapsed to a local variable to avoid keeping the lock for too long while printing
  struct Time local_time_elapsed_trip = time_elapsed_trip;   // copy trip time elapsed to a local

  pthread_mutex_unlock(&dashboardLock);

  pthread_mutex_lock(&engineLock);
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

  int local_engine_temp = engine_temp; // copy engine temp to a local variable to avoid keeping the lock for too long while printing
  int local_rpm = rpm;                 // copy rpm to a local variable to avoid keeping the lock for too long while printing
  pthread_mutex_unlock(&engineLock);

  pthread_mutex_lock(&motionLock);
  int local_speed = speed;
  float local_distance_total = distance_total;
  float local_distance_trip = distance_trip;
  pthread_mutex_unlock(&motionLock);

  pthread_mutex_lock(&hybridAssistLock);
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

  char batt_bar[21];
  int batt_filled = (battery_level * 20) / 100;
  if (batt_filled < 0)
    batt_filled = 0;
  if (batt_filled > 20)
    batt_filled = 20;
  for (int i = 0; i < 20; i++)
    batt_bar[i] = (i < batt_filled) ? '#' : '-';
  batt_bar[20] = '\0';
  float local_battery_level = battery_level;
  pthread_mutex_unlock(&hybridAssistLock);

  pthread_mutex_lock(&fuelLock);
  char fuel_bar[21];
  int fuel_filled = (int)((fuel / 4.7f) * 20);
  if (fuel_filled < 0)
    fuel_filled = 0;
  if (fuel_filled > 20)
    fuel_filled = 20;
  for (int i = 0; i < 20; i++)
    fuel_bar[i] = (i < fuel_filled) ? '#' : '-';
  fuel_bar[20] = '\0';

  float local_fuel = fuel; // copy fuel to a local variable to avoid keeping the lock for too long while printing
  char *low_fuel_label = (low_fuel_warning == 1) ? "!! LOW FUEL !!" : "";
  pthread_mutex_unlock(&fuelLock);

  pthread_mutex_lock(&eventQueueLock);
  struct Event local_event_log[3] = {0};
  int local_event_count = event_count;
  for (int i = local_event_count - 1; i > local_event_count - 4 && i >= 0; i--)
  {
    local_event_log[local_event_count - 1 - i] = event_log[i];
  }
  pthread_mutex_unlock(&eventQueueLock);

  printf("╔════════════════════════════════════════════════════════════════════"
         "═════════╗\n");
  print_dash_row("                                GEEKERS OS");
  printf("║════════════════════════════════════════════════════════════════════"
         "═════════║\n");

  print_dash_row("  ENGINE: %s  |  TEMP: %3d C (%s)  |  RPM: %5d (%s)",
                 eng_status, local_engine_temp, temp_zone_label, local_rpm, rpm_zone_label);

  print_dash_row("  SPEED: %3d mph", local_speed);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  FUEL    [%-20s]  %4.2f gal  %s", fuel_bar, local_fuel,
                 low_fuel_label);

  print_dash_row("  DIST TOTAL: %8.1f mi        DIST TRIP: %6.1f mi",
                 local_distance_total, local_distance_trip);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  TIME ELAPSED (TOTAL): %02d:%02d:%02d",
                 local_time_elapsed_total.hours, local_time_elapsed_total.minutes,
                 local_time_elapsed_total.seconds);

  print_dash_row("  TIME ELAPSED (TRIP):  %02d:%02d:%02d",
                 local_time_elapsed_trip.hours, local_time_elapsed_trip.minutes,
                 local_time_elapsed_trip.seconds);

  printf("║────────────────────────────────────────────────────────────────────"
         "─────────║\n");

  print_dash_row("  SIGNAL: %s       HEADLIGHT: %s", signal_label,
                 headlight_label);

  printf("║═══════════════════════════ HYBRID ASSIST SYSTEM "
         "════════════════════════════║\n");

  print_dash_row("  BATTERY  [%-20s]  %3.2f%%", batt_bar, local_battery_level);

  print_dash_row("  ELECTRIC ASSIST: %s    CHARGING: %s    HYBRID MODE: %s",
                 elec_assist_label, charging_label, hybrid_mode_label);

  printf("║═══════════════════════════════ EVENT LOG "
         "═══════════════════════════════════║\n");

  int display_num = 1;

  for (int i = 0; i < 3; i++)
  {
    print_dash_row("  %d. [%02d:%02d:%02d] %s", display_num,
                   local_event_log[i].timestamp.hours, local_event_log[i].timestamp.minutes,
                   local_event_log[i].timestamp.seconds, local_event_log[i].description);
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
  while (running)
  {
    refresh_dashboard(print_dashboard);
    usleep(500000); // refreshes dash every 0.5 sec
  }
  return NULL;
}

void *time_thread(void *arg)
{
  while (running)
  {
    // check the engine state as time should only increment when engine is running
    // this is for Phase 3 kil switch
    pthread_mutex_lock(&engineStateLock);
    int local_engine_state = engine_state;
    pthread_mutex_unlock(&engineStateLock);

    if (local_engine_state == 1)
    {
      pthread_mutex_lock(&dashboardLock);
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
      pthread_mutex_unlock(&dashboardLock);
    }
    sleep(1);
  }
  return NULL;
}

void init_from_args(int argc, char *argv[])
{
  if (argc <= 6)
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

  // setup the terminal for inputs
  setup_terminal();
  // restore terminal settings on exit
  atexit(restore_terminal);

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

  left_signal_on = 0;
  right_signal_on = 0;
  hazard_state = 0;
  headlight_state = 1;
  electric_assist_state = 1;
  electric_assist_allowed = 0;
  charging_state = 0;
  hybrid_mode = 2; // hybrid assist
  battery_mode_on = 0;

  event_count = 0;
  event_queue_count = 0;
  wheelSlipDetected = 0;
  last_fuel_amount_logged = -1;
  ecu_update = 1; // set to 1 so ecu begins by checking initial conditions
  engine_off_decelerate = 0;
  max_speed = 200;
  max_rpm = 16500;
  hybrid_update = 1; // set to 1 so hybrid assist thread checks conditions on startup
  system_state = NORMAL_STATE;

  // initialize all mutex's
  pthread_mutex_init(&engineStateLock, NULL);
  pthread_mutex_init(&engineLock, NULL);
  pthread_mutex_init(&motionLock, NULL);
  pthread_mutex_init(&fuelLock, NULL);
  pthread_mutex_init(&ecuLock, NULL);
  pthread_mutex_init(&hybridAssistLock, NULL);
  pthread_mutex_init(&eventQueueLock, NULL);
  pthread_mutex_init(&dashboardLock, NULL);
  pthread_mutex_init(&ecuConditionalLock, NULL);
  pthread_mutex_init(&hybridAssistConditionalLock, NULL);

  // initialize conditionals
  pthread_cond_init(&ecuConditional, NULL);
  pthread_cond_init(&eventQueueConditional, NULL);
  pthread_cond_init(&eventQueueNotFullConditional, NULL);
  pthread_cond_init(&engineOnConditional, NULL);
  pthread_cond_init(&speedChangeConditional, NULL);
  pthread_cond_init(&fuelEngineOnConditional, NULL);

  // create threads
  pthread_t input_tid, engine_tid, motion_tid, fuel_tid, ecu_tid, hybrid_assist_tid,
      event_tid, dashboard_tid, time_tid;

  pthread_create(&input_tid, NULL, input_thread, NULL);
  pthread_create(&engine_tid, NULL, engine_thread, NULL);
  pthread_create(&motion_tid, NULL, motion_thread, NULL);
  pthread_create(&fuel_tid, NULL, fuel_thread, NULL);
  pthread_create(&ecu_tid, NULL, ecu_thread, NULL);
  pthread_create(&hybrid_assist_tid, NULL, hybrid_assist_thread, NULL);
  pthread_create(&event_tid, NULL, event_thread, NULL);
  pthread_create(&dashboard_tid, NULL, dashboard_thread, NULL);
  pthread_create(&time_tid, NULL, time_thread, NULL);
  // Wait (threads run indefinitely)
  pthread_join(input_tid, NULL);
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
