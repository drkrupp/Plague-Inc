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

//Move causes status check, after status check is done move again
enum events { ARRIVAL, DEPARTURE, NEW_INFECTION, NEW_RECOVERY, STATUS_CHECK};
enum abs_directions { NORTH = 0, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST, NO_MOVE};


typedef struct {
	int moves_left;
	int x_spot;
	int y_spot;
	int infected_time;
    bool infected;
    bool susceptible;
	bool alive;
    bool immune;
	enum abs_directions previous_move;

    tw_stime infection_start;
    tw_stime immune_start;
} person;

typedef struct {
	int x_spot;
	int y_spot;
    int infected_count;
    int total_people;
	tw_lpid* people_held;
} location;

typedef struct {
	enum events event_type;
    bool person_infected;
    tw_lpid person_id;
    int infected_count;
} Msg_Data;

#endif
