#ifndef INC_PLAGUEINC_H
#define INC_PLAGUEINC_H

#include <ross.h>
#include <stdlib.h>
#include <stdbool.h>

// Simulation parameters
#define GRID_WIDTH 2
#define GRID_HEIGHT 2
#define NUM_PROCESSES 2
#define PEOPLE_PER_LOCATION 2
#define STEPS 300
#define MAX_PEOPLE_PER_LOCATION 25
#define TRANSMISSION_RATE 0.1
#define INITIAL_INFECTED_RATE 0.0
#define RECOVERY_RATE 1.0
#define IMMUNITY_TIME 30.0
#define INFECTION_TIME 50.0
#define MOVE_PROBABILITY 0.4
#define DEATH_RATE 0.0

// Event types
typedef enum
{
    STATUS_UPDATE,
    ARRIVAL,
    DEPARTURE
} event_t;

// State of each person
typedef struct
{
    bool alive;
    bool infected;
    bool susceptible;
    int x, y; // Location coordinates
    int id;   // Unique person identifier
    tw_stime infected_time;
    tw_stime immune_start;
} person_state;

// State of each location LP
typedef struct
{
    int x, y;
    int num_people;
    int max_people_held;
    person_state *people;
} location_state;

// Event payload: minimal, carries only one person's info
typedef struct
{
    event_t type;                // STATUS_UPDATE, ARRIVAL, or DEPARTURE
    int person_id;               // stable unique person ID
    person_state arriving_state; // used only when type==ARRIVAL
    int dest_gid;                // used only when type==DEPARTURE
} event_msg;

#endif // INC_PLAGUEINC_H
