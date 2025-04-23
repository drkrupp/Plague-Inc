

#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>
#include <stdlib.h>
#include <stdbool.h>

// Simulation parameters
#define GRID_WIDTH 2
#define GRID_HEIGHT 2
#define PEOPLE_PER_LOCATION 2
#define NUM_PROCESSES 2
#define STEPS 300
#define MAX_PEOPLE_PER_LOCATION 25
#define TRANSMISSION_RATE 0.1
#define INITIAL_INFECTED_RATE 0.0
#define RECOVERY_RATE 1.0
#define IMMUNITY_TIME 30.0
#define INFECTION_TIME 50.0
#define MOVE_PROBABILITY 0.4
#define MAX_PEOPLE 100
#define DEATH_RATE 0.0
// #define lookahead 0
// #define opt_mem 0

// Event types
typedef enum
{
    STATUS_UPDATE,
    ARRIVAL,
    DEPARTURE
} event_t;

// Person state structure
typedef struct
{
    bool alive;
    bool infected;
    bool susceptible;
    int x, y; // coordinates of the current location
    int id;   // unique identifier for person
    tw_stime infected_time;
    tw_stime immune_start;
} person_state;

// LP state: each location has a list of people
typedef struct
{
    int x, y;
    int num_people;
    int max_people_held;
    person_state *people;
} location_state;

// Event payload
typedef struct
{
    event_t type;
    person_state arriving_state;
    int max_people_count;
    person_state *people;
    int person_index; // -1 for all people
} event_msg;

#endif