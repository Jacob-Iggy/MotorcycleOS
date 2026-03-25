Phase 1 Reflection
Logan Pinel, Jacob Igielski, Nick Dobrzycki

 List all threads implemented in your system and briefly describe their responsibilities.

Engine Thread - Creates a simulation of increasing rpms and slowing them back down, while also controlling engine temperatures based on rpm zones.
Motion Thread - Controls vehicle speeds by starting at 50 and increasing to 70 and decreasing  back to 50 while rpms rise, while also counting trip distance.
Fuel Thread - Controls fuel and slowly decreases fuel based on rpm zone and eventually shuts off the engine when it is empty.
ECU Thread - The ECU determines what rpm zone you are in based on your current rpms, along with engine temperature, low fuel warnings and signal changes. 
Hybrid Assist Thread - Controls hybrid assist system by switching between cruising, assist, and regenerative mode based on speed and direction. This also updates the battery level.
Event Thread - Monitors and logs events whenever significant ones occur such as RPM zone changes, hybrid mode changes, low fuel etc and displays them into the event log display. 
Dashboard Thread - Reads info and clears dashboard and reloads it every 0.5 seconds.
Time Thread - Increments trip and total elapsed time counters every second

 Describe how your system state is organized. Did you use a single struct? Multiple structs? Global variables? Nested structures? Explain your reasoning.

We primarily use global variables to store the key data for our motorcycle operating system. We did use a couple structs aswell but only when absolutely needed for things like Time with hours, minutes, seconds, and Events that have multiple parts to the data. We found that using global variables made managing state and logic easy when working across numerous threads since they were always accessible to all the threads. We will have to be cautious moving forward and make sure to accurately prevent race conditions from occuring due to the shared global variables.

Explain how you ensured that:
Simulation logic is not mixed with dashboard printing.

Each thread is separate and the dashboard display, print_dashboard(), is only called in the dashboard thread. This thread also doesn’t update any info and allows simulation logic to not mix in with dashboard printing.

The ECU does not simulate physical quantities.

The ECU doesn’t increment anything, but rather only checks things such as rpms and assigns a rpm zone or engine temperatures and assigns the engine temperature zones. Each thread updates its information separately.

The dashboard does not enforce system rules.

The dashboard display only reads information and displays it. It does not modify anything or enforce rules.


Describe how information flows in your system.
How does RPM move from Engine − > ECU − > Dashboard?

The engine thread updates rpms every second which the ECU reads and determines which zone to classify the rpms as. In the dashboard thread, it reads rpm and rpm zone from the ECU and displays them properly formatted.

How does fuel consumption affect what is displayed?

The fuel thread decreases fuel every second by a rate determined by the rpm zone. The ECU thread then consistently checks fuel and updates with a low fuel warning when it drops under 0.7 gallons. The dashboard thread then reads the fuel state from the ECU and updates the display based on if there is low fuel and also the fuel bar.

Attach a screenshot of your dashboard output. Why did you choose this layout? Did you prioritize readability, realism, minimalism, compactness, or visual appeal?

When determining the layout for our dashboard we were heavily influenced by the example shown in the project instructions. We really like the clean, text based design and the use of ASCII characters to make the terminal a little more interesting. We designing this layout we wanted to prioritize readability, minimalism and appropriate groupings of information. We chose to keep engine and speed related information in one section, fuel and distance in the next, then time, followed by indicators, hybrid assist, and event log. We felt that these groupings were a strong way to organize the data on our dashboard while also following a sequential order from the assignment instructions.

Explain how you represented the following: RPM, Fuel level, Speed, Engine state. Why did you choose numeric values, bars, symbols, or a hybrid approach?

We represented RPMs as just a number by a “RPM:” label because it shows the most accurate and easily readable data. For fuel levels we used a bar made out of ASCII that also has a warning when fuel is low. It also shows the actual amount of fuel left for accuracy. For speed we used just a simple label and number as it would show the most accuracy especially for something like speed. Lastly, the engine state is simply just “ON” or “OFF” with a label “Engine:” as anything else seems unnecessary.

Describe at least two relationships between subsystems, note that the relationship logic will be implemented in future phases.

One relationship between subsystems is seen between the engine and ECU subsystems. The engine thread continuously updates the rpm and engine_temp global variables and the ECU uses those variables to then determine the values of rpm_zone and engine_temp_zone. Another example of a relationship between subsystems is between the motion and hybrid assist subsystems. The motion thread controls the speed and direction global variables while the hybrid assist utilizes those variables to determine if the car is accelerating or decelerating and at what rate to influence how the hybrid assist impacts the motorcycle, whether it regenerates battery on decelerations or uses battery to assist the vehicle at certain speeds.

What responsibilities does your ECU handle in Phase 1?

Some of the primary responsibilities of our ECU subsystem include, making sure that rpm, speed, and rpm_zone reflect the engine being off, and also determining the rpm_zone (idle, normal, high, redline, or off). Another major responsibility is that the ECU determines the engine temperature zone based on the current engine temp (cold, normal, hot, overheat). Lastly, the ECU helps alert the dashboard when fuel has reached a low level and displays the warning message.

Even though synchronization is not implemented in Phase 1: Identify at least two shared variables that could cause race conditions in your program. Explain why concurrent access might be problematic.

Two shared variables that could cause race conditions in our program are the rpm and battery level variables. The rpm variable is accessed and modified by the engine and ECU threads. Without semaphores or synchronization the ECU could try reading the rpm to determine the rpm_zone while the rpm variable is being updated by the engine thread, providing an inaccurate reading of the value. The battery level variable is also shared between the hybrid assist and event logging subsystems prompting a similar risk to the rpm variable as the hybrid assist subsystem edits the value and the event logging subsystem is reading it. In summary, concurrent access to these shared variables could be problematic as it could lead to inaccurate or unexpected values for the variables when being read or written to the dashboard.

What is one design weakness in your system that you would improve in the next phase?
	
One design weakness in our system that we might want to improve upon in the next phase is the lack of modularity and SoC in our code. We have our entire operating system in a single c file. We heard in class that we could separate each thread into its own respective file which we might include as we progress this project, our only worry is how to maintain global variables across the files.
