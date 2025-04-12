#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>

#define MOVES 30
#define INFECTION_TIME 5

//Events - movements & infections
enum events { ARIVAL, DEPARTURE, DIRECTION_SELECT, INFECTION };
enum abs_directions { NORTH = 0, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST };


typedef struct {
	int moves_left;
	int x_spot;
	int y_spot;
	int infected_time;
	bool alive;
	enum abs_directions previous_move;
} person;

typedef struct {
	int x_spot;
	int y_spot;
	int people_held;
	//Maybe an array which holds the people in the spot
} grid_spot;

typedef struct {
	enum events event_type;
	Person person;
} Msg_Data;

#endif