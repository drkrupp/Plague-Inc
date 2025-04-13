#ifndef INC_plagueInc_h
#define INC_plagueInc_h

#include <ross.h>

#define MOVES 30
#define INFECTION_TIME 5
#define GRID_WIDTH 10
#define GRID_HEIGHT 10

//Move causes status check, after status check is done move again
enum events { MOVE, STATUS_CHECK };
enum abs_directions { NORTH = 0, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST, NO_MOVE};


typedef struct {
	int moves_left;
	int x_spot;
	int y_spot;
	int infected_time;
    int immune_time;
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
