#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>

#define MOVES 30
#define INFECTION_TIME 50.0
#define IMMUNE_TIME 100
#define GRID_WIDTH 10
#define GRID_HEIGHT 10
#define STARTING_INFECTED_PERCENTAGE .1
#define TRANSMISSION_COEFFICIENT .2
#define LETHALITY .00001
#define TRAVEL_LIKELIHOOD .8
#define AVERAGE_MOVE_TIME 10.0
#define INITIAL_MAX_PEOPLE_PER_SQUARE 500
#define NUM_LOCATIONS = GRID_WIDTH * GRID_HEIGHT
#define NUM_PEOPLE = 50

//Move causes status check, after status check is done move again
enum events { ARRIVAL, DEPARTURE, NEW_INFECTION, NEW_RECOVERY, STATUS_CHECK};
enum abs_directions { NORTH = 0, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST, NO_MOVE};


typedef struct {
	x_spot;
	y_spot;
    moves_left;
	infected_time;
    infected;
    susceptible;
	alive;
    immune;
    infection_start;
    immune_start;

    int prev_x_spot;
    int prev_y_spot;
    int prev_moves_left;
    int prev_infected_time;
    bool prev_infected;
    bool prev_susceptible;
    bool prev_alive;
    bool prev_immune;
    tw_stime prev_infection_start;
    tw_stime prev_immune_start;


} person;

typedef struct {
	int x_spot;
	int y_spot;
    int infected_count;
    int total_people;
	tw_lpid* people_held;
    int max_people_held;
} location;

typedef struct {
	enum events event_type;
    bool person_infected;
    tw_lpid person_id;
    int infected_count;
} Msg_Data;

#endif
