#ifndef PLAGUE1LP_H
#define PLAGUE1LP_H

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

#endif // PLAGUE1LP_H