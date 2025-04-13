#include "plagueInc.h"

void
person_init(person * s, tw_lp * lp)
{
  int i;
  tw_event *e;
  Msg_Data *m;
  s->moves_left = 30;
  s->x_spot = tw_rand_integer(lp->rng, 0, GRID_WIDTH-1);
  s->y_spot = tw_rand_integer(lp->rng, 0, GRID_HEIGHT-1);
  s->alive = true;

  double r = tw_rand_unif(lp->rng);
  if (r < TRANSMISSION_COEFFICIENT) {
	s->infected_time = tw_now(lp);
	s->susceptible = false;
	s->infected = true;
  } else{
	s->susceptible = true;
	s->infected = false;
  }


  e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, AVERAGE_MOVE_TIME), lp); //Schedules event at some future point in time
  m = tw_event_data(e); //retrieves the data pointer for the message associated with scheduled event, can modify message from here
  m->type = STATUS_CHECK;
  m->infected_count = 0;
  tw_event_send(e); //schedules event in ROSS
}



enum abs_directions assign_move(tw_lp * lp, int x_spot, int y_spot) {
    enum abs_directions possible_moves[8]; 
    int count = 0;

    if (y_spot < GRID_HEIGHT - 1) {
        possible_moves[count++] = NORTH;
        if (x_spot < GRID_WIDTH - 1)
            possible_moves[count++] = NORTH_EAST;
        if (x_spot > 0)
            possible_moves[count++] = NORTH_WEST;
    }
    if (y_spot > 0) {
        possible_moves[count++] = SOUTH;
        if (x_spot < GRID_WIDTH - 1)
            possible_moves[count++] = SOUTH_EAST;
        if (x_spot > 0)
            possible_moves[count++] = SOUTH_WEST;
    }
    if (x_spot < GRID_WIDTH - 1)
        possible_moves[count++] = EAST;
    if (x_spot > 0)
        possible_moves[count++] = WEST;

    if (count == 0)
        return NO_MOVE;

    return possible_moves[tw_rand_integer(lp->rng, 0, count - 1)];
}



tw_lpid get_location_id(int x_spot, int y_spot){
	//TODO: FILL IN
}

void
person_event(person * s, tw_bf * bf, Msg_Data * msg, tw_lp * lp) {
	//WHEN NOT SICK & ALIVE
	bool new_infection;
	bool new_recovery;
	tw_stime now_time = tw_now(lp);
	int x_spot = s->x_spot;
	int y_spot = s->y_spot;
	bool previously_infected = s->infected;
	tw_event *e;
	Msg_Data *m;
	if (s->immune){
		{
			if(now_time - s->immune_start > IMMUNE_TIME) {
				s->immune = false;
				s->susceptible = true;
			}
		}
	}

	if (s->susceptible){
		for (i = 0; i < msg->infected_count; i++){
			if(tw_rand_unif(lp->rng) < TRANSMISSION_COEFFICIENT) {
				s->susceptible = false;
				s->infected_time = now_time;
				s->infected = true;
				new_infection = true;
			}
		}
	} 
	//WHEN SICK & ALIVE - DO WE EVEN WANT DEAD PEOPLE TO MOVE

	else
	{
		if(tw_rand_unif(lp->rng) < LETHALITY) {
			s->alive = false;
			tw_lpid location_id = get_location_id(x_spot, y_spot);
			e = tw_event_new(location_id, 1, lp);
			m = tw_event_data(e); 
		    m->type = DEPARTURE; //TREAT A DEATH AS A DEPARTURE FROM LOCATION AND ARRIVAL NOWHERE
			tw_event_send(e); 
		}
		if(now_time - s->infected_time > INFECTION_TIME){
			s->infected = false;
			s->immune = true;
			s->immune_start = now_time;
			new_recovery = true;
		}
	}

	//LET THEM MOVE AND SEND MESSAGES TO THEIR LOCATION LPs

	if (s->alive) {
		tw_lpid prev_location_id = get_location_id(x_spot, y_spot);
		enum abs_directions next_move = assign_move(lp, x_spot, y_spot);
		switch (next_move)
		{
		case NORTH: 
			y_spot++;
			break;
		case NORTH_EAST:
			y_spot++
			x_spot++;
		case EAST:
			x_spot++;
		case SOUTH_EAST:
			y_spot--;
			x_spot++;
		case SOUTH:
			y_spot--;
		case SOUTH_WEST:
			y_spot--;
			x_spot--;
		case WEST:
			x_spot--;
		case NORTH_WEST:
			y_spot++;
			x_spot--;

		case NO_MOVE:
			if(new_infection){
				e = tw_event_new(prev_location_id, 1, lp);
				m = tw_event_data(e); 
				m->type = NEW_INFECTION //JUST INCREMENT THE AMOUNT OF SICK PEOPLE AT THE SPOT;
				tw_event_send(e); 
			}
			else if(new_recovery){
				e = tw_event_new(prev_location_id, 1, lp);
				m = tw_event_data(e); 
				m->type = NEW_INFECTION //JUST DECREASE THE AMOUNT OF SICK PEOPLE AT THE SPOT;
				tw_event_send(e); 
			}
			return;
		}
		tw_event *e_leave;
		Msg_Data *m_leave;

		tw_event *e_arrive;
		Msg_Data *m_arrive;

		tw_lpid new_location_id = get_location_id(x_spot, y_spot);


		e_leave = tw_event_new(prev_location_id, 1, lp);
		m_leave = tw_event_data(e_leave); 
		m_leave->type = DEPARTURE //ADJUST THE AMOUNT OF SICK PEOPLE AND TOTAL PEOPLE AT THE SPOT & REMOVE THE PERSON FROM THE ARRAY OF PEOPLE IN THE SPOT;
		m_leave->person_infected = previously_infected;
		m_leave->person_id = lp->gid;
		tw_event_send(e_leave); 

		e_arrive = tw_event_new(new_location_id, 1, lp);
		m_arrive = tw_event_data(e_arrive); 
		m_arrive->type = ARRIVAL //ADJUST THE AMOUNT OF SICK PEOPLE AND TOTAL PEOPLE AT THE SPOT & ADD THE PERSON TO THE ARRAY OF PEOPLE IN THE SPOT;
		m_arrive->person_infected = s->infected;
		m_arrive->person_id = lp->gid;
		tw_event_send(e_arrive); 
	}
}

void 



tw_lptype my_lps[] = {
    {
        .init = person_init,
        .pre_run = NULL,
        .event = person_event,
        .revent = person_event_reverse,
        .final = person_final,
        .map = NULL,
        .state_sz = sizeof(person),
    },
    {
        .init = location_int,
        .pre_run = NULL,
        .event = location_event,
        .revent = location_event_reverse,
        .final = location_final,
        .map = NULL,
        .state_sz = sizeof(location),
    },
    { 0 }, // End marker
};


main(int argc, char **argv, char **env)
{


	tw_define_lps(nlp, sizeof(Msg_Data), 0);
	g_tw_lp_types = my_lps;

}