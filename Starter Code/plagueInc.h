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

// Realistic Nums
/**
#define MOVES 30                        // Simulate several days of mobility
#define INFECTION_TIME 50.0            // ~5 days
#define IMMUNE_TIME 300                // ~1 month
#define GRID_WIDTH 20
#define GRID_HEIGHT 20
#define STARTING_INFECTED_PERCENTAGE 0.01   // 1% of population initially infected
#define TRANSMISSION_COEFFICIENT 0.03       // Basic R0 of ~1.5–2 depending on contact rate
#define LETHALITY 0.005                     // 0.5% mortality rate
#define TRAVEL_LIKELIHOOD 0.7               // People move frequently, but not every time step
#define AVERAGE_MOVE_TIME 6.0               // One move every few "hours"
#define INITIAL_MAX_PEOPLE_PER_SQUARE 1000
#define NUM_LOCATIONS (GRID_WIDTH * GRID_HEIGHT)
#define NUM_PEOPLE 1000                     // 2.5 people per square on average
*/

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