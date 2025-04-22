/**

#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>
#include <mpi.h>
#include <stdio.h>
#include <stdbool.h>

// Tesing Nums
#define MOVES 2
#define INFECTION_TIME 10.0
#define IMMUNE_TIME 20
#define GRID_WIDTH 2
#define GRID_HEIGHT 2
#define STARTING_INFECTED_PERCENTAGE .2
#define TRANSMISSION_COEFFICIENT .8
#define LETHALITY 0.01
#define TRAVEL_LIKELIHOOD .5
#define AVERAGE_MOVE_TIME 1.0
#define INITIAL_MAX_PEOPLE_PER_SQUARE 50
#define NUM_LOCATIONS (GRID_WIDTH * GRID_HEIGHT)
#define NUM_PEOPLE 4


// Move causes status check, after status check is done move again
enum events
{
    ARRIVAL,
    DEPARTURE,
    NEW_INFECTION,
    NEW_RECOVERY,
    STATUS_CHECK
};
enum abs_directions
{
    NORTH = 0,
    NORTH_EAST,
    EAST,
    SOUTH_EAST,
    SOUTH,
    SOUTH_WEST,
    WEST,
    NORTH_WEST,
    NO_MOVE
};

typedef struct
{
    int x_spot;
    int y_spot;
    int moves_left;
    int infected_time;
    bool infected;
    bool susceptible;
    bool alive;
    bool immune;
    tw_stime infection_start;
    tw_stime immune_start;
} person;

typedef struct
{
    int x_spot;
    int y_spot;
    int infected_count;
    int total_people;
    tw_lpid *people_held;
    int max_people_held;
} location;

typedef struct
{
    enum events event_type;
    bool person_infected;
    tw_lpid person_id;
    int infected_count;
    person person_state;
} Msg_Data;

#endif

*/

#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>
#include <stdlib.h>
#include <stdbool.h>

// Simulation parameters
#define GRID_WIDTH 5
#define GRID_HEIGHT 5
#define PEOPLE_PER_LOCATION 2
#define TRANSMISSION_RATE 0.3
#define INITIAL_INFECTED_RATE 0.1
#define RECOVERY_RATE 0.2
#define MORTALITY_RATE 0.05
#define IMMUNITY_TIME 50
#define INFECTION_TIME 30
#define MOVE_PROBABILITY 0.3
#define MAX_PEOPLE 100
#define DEATH_RATE 0.0001

// Event types
typedef enum
{
    STATUS_UPDATE,
    MOVE_PERSON
} event_t;

// Person state structure
typedef struct
{
    bool alive;
    bool infected;
    bool immune;
    bool susceptible;
    int x, y; // coordinates of the current location
    tw_stime infected_time;
    tw_stime immune_start;
} person_state;

// LP state: each location has a list of people
typedef struct
{
    int x, y;
    int num_people;
    person_state people[PEOPLE_PER_LOCATION];
} location_state;

// Event payload
typedef struct
{
    event_t type;
    int person_index; // -1 for all people
} event_msg;

#endif