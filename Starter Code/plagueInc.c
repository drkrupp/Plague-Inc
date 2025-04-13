#include "plagueInc.h"


void
person_init(person * s, tw_lp * lp)
{
  int i;
  tw_event *e;
  Msg_Data *m;

  s->moves_left = 30;
  s->x_spot = rand(GRID_WIDTH);
  s->y_spot = rand(GRID_HEIGHT);
  s->infected_time = 0;
  s->immune_time = 0;
  s->susceptible = true;
  s->alive = true;
  s->previous_move = NULL;
  s->next_move = assign_move()

  e = tw_event_new(lp->gid, 0, lp); //0 is the offset - no clue what that is
  m = tw_event_data(e); //tried looking at the documentation, wasn't sure, just followed from the examples
  m->type = MOVE;
  tw_event_send(e);
}



abs_directions assign_move(tw_lp * lp, int x_spot, int y_spot) {
    abs_directions possible_moves[8]; 
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





tw_lptype my_lps[] = {
    {
        .init = person_init,
        .pre_run = NULL,
        .event = person_event,
        .revent = person_event_reverse,
        .final = person_final,
        .map = NULL,
        .state_sz = sizeof(Person),
    },
    {
        .init = grid_init,
        .pre_run = NULL,
        .event = grid_event,
        .revent = grid_event_reverse,
        .final = grid_final,
        .map = NULL,
        .state_sz = sizeof(Grid),
    },
    { 0 }, // End marker
};


main(int argc, char **argv, char **env)
{


	tw_define_lps(nlp, sizeof(Msg_Data), 0);
	g_tw_lp_types = my_lps;

}


void
event_handler(person * s, tw_bf * bf, Msg_Data * msg, tw_lp * lp)
{
  int rand_result;
  abs_directions;
  tw_event *e;
  airport_message *m;


  switch(msg->type)
  {
	case MOVE:
		{
			
			break;
		}
	case STATUS_CHECK:
		{

		}
  }


  switch(msg->type)
    {

    case MOVE:
      {
	// Schedule a landing in the future
	msg->saved_furthest_flight_landing = s->furthest_flight_landing;
	s->furthest_flight_landing = ROSS_MAX(s->furthest_flight_landing, tw_now(lp));
	ts = tw_rand_exponential(lp->rng, MEAN_LAND);
	e = tw_event_new(lp->gid, ts + s->furthest_flight_landing - tw_now(lp), lp);
	m = tw_event_data(e);
	m->type = LAND;
	m->waiting_time = s->furthest_flight_landing - tw_now(lp);
	s->furthest_flight_landing += ts;
	tw_event_send(e);
	break;
      }

    case DEPARTURE:
      {
	s->planes_on_the_ground--;
	ts = tw_rand_exponential(lp->rng, mean_flight_time);
	rand_result = tw_rand_integer(lp->rng, 0, 3);
	dst_lp = 0;
	switch(rand_result)
	  {
	  case 0:
	      // Fly north
	      if(lp->gid < 32)
		  // Wrap around
		  dst_lp = lp->gid + 31 * 32;
	      else
		  dst_lp = lp->gid - 31;
	      break;
	  case 1:
	      // Fly south
	      if(lp->gid >= 31 * 32)
		  // Wrap around
		  dst_lp = lp->gid - 31 * 32;
	      else
		  dst_lp = lp->gid + 31;
	      break;
	  case 2:
	      // Fly east
	      if((lp->gid % 32) == 31)
		  // Wrap around
		  dst_lp = lp->gid - 31;
	      else
		  dst_lp = lp->gid + 1;
	      break;
	  case 3:
	      // Fly west
	      if((lp->gid % 32) == 0)
		  // Wrap around
		  dst_lp = lp->gid + 31;
	      else
		  dst_lp = lp->gid - 1;
	      break;
	  }

	e = tw_event_new(dst_lp, ts, lp);
	m = tw_event_data(e);
	m->type = ARRIVAL;
	tw_event_send(e);
	break;
      }

    case LAND:
      {
	s->landings++;
	s->waiting_time += msg->waiting_time;

	e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, MEAN_DEPARTURE), lp);
	m = tw_event_data(e);
	m->type = DEPARTURE;
	tw_event_send(e);
	break;
      }

    }
}

void
rc_event_handler(airport_state * s, tw_bf * bf, airport_message * msg, tw_lp * lp)
{
  switch(msg->type)
  {
    case ARRIVAL:
	s->furthest_flight_landing = msg->saved_furthest_flight_landing;
	tw_rand_reverse_unif(lp->rng);
	break;
    case DEPARTURE:
	tw_rand_reverse_unif(lp->rng);
	tw_rand_reverse_unif(lp->rng);
	break;
    case LAND:
	s->landings--;
	s->waiting_time -= msg->waiting_time;
	tw_rand_reverse_unif(lp->rng);
  }
}

void
final(airport_state * s, tw_lp * lp)
{
	wait_time_avg += ((s->waiting_time / (double) s->landings) / nlp_per_pe);
}

tw_lptype airport_lps[] =
{
	{
		(init_f) init,
        (pre_run_f) NULL,
		(event_f) event_handler,
		(revent_f) rc_event_handler,
		(commit_f) NULL,
		(final_f) final,
		(map_f) mapping,
		sizeof(airport_state),
	},
	{0},
};

const tw_optdef app_opt [] =
{
	TWOPT_GROUP("Airport Model"),
    TWOPT_STIME("lookahead", lookahead, "lookahead for events"),
	//TWOPT_UINT("nairports", nlp_per_pe, "initial # of airports(LPs)"),
	TWOPT_UINT("nplanes", planes_per_airport, "initial # of planes per airport(events)"),
	TWOPT_STIME("mean", mean_flight_time, "mean flight time for planes"),
	TWOPT_UINT("memory", opt_mem, "optimistic memory"),
	TWOPT_END()
};

int
main(int argc, char **argv, char **env)
{
	int i;

	tw_opt_add(app_opt);
	tw_init(&argc, &argv);

	nlp_per_pe /= (tw_nnodes() * g_tw_npe);
	g_tw_events_per_pe =(planes_per_airport * nlp_per_pe / g_tw_npe) + opt_mem;

    g_tw_lookahead = lookahead;

	tw_define_lps(nlp_per_pe, sizeof(airport_message));

	for(i = 0; i < g_tw_nlp; i++)
		tw_lp_settype(i, &airport_lps[0]);

	tw_run();

	if(tw_ismaster())
	{
		printf("\nAirport Model Statistics:\n");
		printf("\t%-50s %11.4lf\n", "Average Waiting Time", wait_time_avg);
		printf("\t%-50s %11lld\n", "Number of airports",
			nlp_per_pe * g_tw_npe * tw_nnodes());
		printf("\t%-50s %11lld\n", "Number of planes",
			planes_per_airport * nlp_per_pe * g_tw_npe * tw_nnodes());
	}

	tw_end();

	return 0;
}