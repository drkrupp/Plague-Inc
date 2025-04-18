#include "plagueInc.h"

tw_lpid lp_type_mapper(tw_lpid gid)
{
	if (gid < NUM_LOCATIONS)
		return 0; // LP type 0 = Location
	else
		return 1; // LP type 1 = Person
}

int get_x_spot(tw_lpid gid)
{
	return gid % GRID_WIDTH;
}

int get_y_spot(tw_lpid gid)
{
	int offset = get_x_spot(gid) * GRID_WIDTH;
	return gid - offset;
}

tw_lpid get_location_id(int x_spot, int y_spot)
{
	return x_spot * GRID_WIDTH + y_spot;
}

void person_init(person *s, tw_lp *lp)
{
	tw_event *e;
	Msg_Data *m;
	s->moves_left = 30;
	s->x_spot = tw_rand_integer(lp->rng, 0, GRID_WIDTH - 1);
	s->y_spot = tw_rand_integer(lp->rng, 0, GRID_HEIGHT - 1);
	s->alive = true;

	double r = tw_rand_unif(lp->rng);
	if (r < STARTING_INFECTED_PERCENTAGE)
	{
		s->infected_time = tw_now(lp);
		s->susceptible = false;
		s->infected = true;
	}
	else
	{
		s->susceptible = true;
		s->infected = false;
	}

	// TELL INITIAL LOCATION THAT PERSON HAS "ARRIVED" -> SPAWN PEOPLE
	tw_lpid location_id = get_location_id(s->x_spot, s->y_spot);
	tw_event *e_arrival;
	Msg_Data *m_arrival;

	e_arrive = tw_event_new(location_id, 1, lp);
	m_arrive->type = ARRIVAL;
	m_arrive->person_infected = s->infected;
	m_arrive->person_id = lp->gid;
	n_arrive->person_state = *s;
	tw_event_send(e_arrive);
}

void location_init(location *s, tw_lp *lp)
{
	tw_event *e;
	Msg_Data *m;
	tw_lpid gid = lp->gid;
	s->x_spot = get_x_spot(gid);
	s->y_spot = get_y_spot(gid);
	s->infected_count = 0;
	s->total_people = 0;
	s->max_people_held = INITIAL_MAX_PEOPLE_PER_SQUARE;
	s->people_held = (tw_lpid *)malloc(sizeof(tw_lpid) * INITIAL_MAX_PEOPLE_PER_SQUARE);
}

enum abs_directions assign_move(tw_lp *lp, int x_spot, int y_spot)
{
	enum abs_directions possible_moves[8];
	int count = 0;

	if (y_spot < GRID_HEIGHT - 1)
	{
		possible_moves[count++] = NORTH;
		if (x_spot < GRID_WIDTH - 1)
			possible_moves[count++] = NORTH_EAST;
		if (x_spot > 0)
			possible_moves[count++] = NORTH_WEST;
	}
	if (y_spot > 0)
	{
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

void person_event(person *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	// WHEN NOT SICK & ALIVE
	bool new_infection;
	bool new_recovery;
	tw_stime now_time = tw_now(lp);
	int x_spot = s->x_spot;
	int y_spot = s->y_spot;
	bool previously_infected = s->infected;
	tw_event *e;
	Msg_Data *m;
	if (s->immune)
	{
		{
			if (now_time - s->immune_start > IMMUNE_TIME)
			{
				s->immune = false;
				s->susceptible = true;
			}
		}
	}

	if (s->susceptible)
	{
		for (i = 0; i < msg->infected_count; i++)
		{
			if (tw_rand_unif(lp->rng) < TRANSMISSION_COEFFICIENT)
			{
				s->susceptible = false;
				s->infected_time = now_time;
				s->infected = true;
				new_infection = true;
			}
		}
	}
	// WHEN SICK & ALIVE - DO WE EVEN WANT DEAD PEOPLE TO MOVE

	else
	{
		if (tw_rand_unif(lp->rng) < LETHALITY)
		{
			s->alive = false;
			tw_lpid location_id = get_location_id(x_spot, y_spot);
			e = tw_event_new(location_id, 1, lp);
			m = tw_event_data(e);
			m->type = DEPARTURE; // TREAT A DEATH AS A DEPARTURE FROM LOCATION AND ARRIVAL NOWHERE
			tw_event_send(e);
		}
		if (now_time - s->infected_time > INFECTION_TIME)
		{
			s->infected = false;
			s->immune = true;
			s->immune_start = now_time;
			new_recovery = true;
		}
	}

	// LET THEM MOVE AND SEND MESSAGES TO THEIR LOCATION LPs

	if (s->alive)
	{
		tw_lpid prev_location_id = get_location_id(x_spot, y_spot);
		enum abs_directions next_move = assign_move(lp, x_spot, y_spot);
		switch (next_move)
		{
		case NORTH:
			y_spot++;
			break;
		case NORTH_EAST:
			y_spot++ x_spot++;
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
			tw_event *e;
			Msg_Data *m;
			if (new_infection)
			{
				e = tw_event_new(prev_location_id, 1, lp);
				m = tw_event_data(e);
				m->type = NEW_INFECTION // JUST INCREMENT THE AMOUNT OF SICK PEOPLE AT THE SPOT;
					tw_event_send(e);
			}
			else if (new_recovery)
			{
				e = tw_event_new(prev_location_id, 1, lp);
				m = tw_event_data(e);
				m->type = NEW_INFECTION // JUST DECREASE THE AMOUNT OF SICK PEOPLE AT THE SPOT;
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
		m_leave->type = DEPARTURE; // ADJUST THE AMOUNT OF SICK PEOPLE AND TOTAL PEOPLE AT THE SPOT & REMOVE THE PERSON FROM THE ARRAY OF PEOPLE IN THE SPOT
		m_leave->person_infected = previously_infected;
		m_leave->person_id = lp->gid;
		tw_event_send(e_leave);

		e_arrive = tw_event_new(new_location_id, 1, lp);
		m_arrive = tw_event_data(e_arrive);
		m_arrive->type = ARRIVAL; // ADJUST THE AMOUNT OF SICK PEOPLE AND TOTAL PEOPLE AT THE SPOT & ADD THE PERSON TO THE ARRAY OF PEOPLE IN THE SPOT
		m_arrive->person_infected = s->infected;
		m_arrive->person_id = lp->gid;
		m_arrive->person_state = *s;
		tw_event_send(e_arrive);
	}
}

person_event_reverse(person *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	*s = msg->person_state;
}

void location_event(location *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	// ARRIVAL, DEPARTURE, NEW_INFECTION, NEW_RECOVERY
	enum events event = msg->event_type;
	tw_lpid person_id = msg->person_id;
	switch (event)
	{
	case ARRIVAL:
		bool is_infected = msg->person_infected;
		// If array is at max capacity and needs to be resized
		int max_people_held = s->max_people_held;
		if (s->total_people == max_people_held)
		{
			s->max_people_held *= 2;
			s->people_held = (tw_lpid *)realloc(s->people_held, sizeof(tw_lpid) * s->max_people_held);
		}

		if (is_infected)
		{
			s->infected_count++;
		}
		s->people_held[s->total_people] = person_id;
		s->total_people++;
		break;

	case DEPARTURE:
		bool was_infected = msg->person_infected;
		for (int i = 0; i < s->total_people; i++)
		{
			if (s->people_held[i] == person_id)
			{
				s->people_held[i] == s->people_held[s->total_people - 1];
			}
		}
		if (was_infected)
		{
			s->infected_count--;
		}
		s->total_people--;
		return;

	case NEW_INFECTION:
		s->infected_count++;
	case NEW_RECOVERY:
		s->infected_count--;
	default:
		// no work to be done if the person is just staying there, only needed in order to generate their next move
		break;
	}
	// Only reach this point if the message was an ARRIVAL, NEW_INFECTION, or NEW_RECOVERY. The lp that receives depaerture doesn't need to worry about the person
	// If a person doesn't move (New infection or new recovery) then the person sends a stay message after to receive next instructions
	tw_event *e;
	Msg_Data *m;
	e = tw_event_new(person_id, tw_rand_exponential(lp->rng, AVERAGE_MOVE_TIME), lp);
	m = tw_event_data(e);
	m->type = STATUS_CHECK;
	m->infected_count = s->infected_count;
	m->person_state = msg->person_state;
	tw_event_send(e);
}

void location_event_reverse(location *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	enum events event = msg->event_type;
	switch (event)
	{
	case ARRIVAL:
		// effects of a departure, not worth worrying about resizing array
		bool is_infected = msg->person_infected;
		tw_lpid person_id = msg->person_id;
		bool was_infected = msg->person_infected;
		for (int i = 0; i < s->total_people; i++)
		{
			if (s->people_held[i] == person_id)
			{
				s->people_held[i] == s->people_held[s->total_people - 1];
			}
		}
		if (was_infected)
		{
			s->infected_count--;
		}
		s->total_people--;
		break;
	case DEPARTURE:
		// Effects of an arrival, don't have to worry about resizing array
		bool was_infected = msg->person_infected;
		tw_lpid person_id = msg->person_id;
		s->people_held[s->total_people] = person_id;
		s->total_people++;
		if (was_infected)
		{
			s->infected_count++;
		}
		break;
	case NEW_INFECTION:
		s->infected_count--;
		break;
	case NEW_RECOVERY:
		s->infected_count++;
		break;
	}
}

void person_final(person *s, tw_lp *lp)
{
	// print stats if we want, maybe count up the number of dead people and the number of alive infected people
}

void location_final(location *s, tw_lp *lp)
{
	// print stats if we want
	free(s->people_lpids);
}

void person_commit(person *s, tw_bf *bf, Msg_Data *in_msg, tw_lp *lp)
{
	person prev_info = in_msg->person_state;
	int prev_x = prev_info.x_spot;
	int prev_y = prev_info.y_spot;
	bool prev_infected = prev_info.infected;
	bool prev_alive = prev_info.alive;
	bool became_infected = !prev_infected && s->infected;
	bool died = prev_alive && !s->alive;
	bool recovered = prev_infected && !s->infected;
	char buf[128];
	int len = snprintf(buf, sizeof(buf), "Time: %lf, Person %lu, Infected: %s, Became Infected: %s, Recovered: %s, Died: %s\n Moved from (%d, %d) to (%d, %d)\n",
					   tw_now(lp), lp->gid, s->infected ? "true" : "false", became_infected ? "true" : "false" recovered ? "true"
																								   : "false" died		 ? "true"
																														 : "false",
					   prev_x, prev_y, s->x_spot, s->y_spot);

	// Might need to adjust offset depending on how output looks/if this gives enough space for each message
	MPI_Offset offset = (MPI_Offset)(lp->gid * 1000 + (long)(tw_now(lp) * 10));
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

void location_commit(location *s, tw_bf *bf, Msg_Data *in_msg, tw_lp *lp)
{
	char buf[128];
	int len = snprintf(buf, sizeof(buf), "Time: %lf, Location %lu, Coordinates: (%d, %d), Infected People: %d, Total People: %d\n",
					   tw_now(lp), lp->gid, s->x_spot, s->y_spot, s->infected, s->total_people);

	// Might need to adjust offset depending on how output looks/if this gives enough space for each message
	MPI_Offset offset = (MPI_Offset)(lp->gid * 1000 + (long)(tw_now(lp) * 10));
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

tw_lptype my_lps[] = {
	{(init_f)person_init,
	 (pre_run_f)NULL,
	 (event_f)person_event,
	 (revent_f)person_event_reverse,
	 (commit_f)person_commit,
	 (final_f)person_final,
	 (map_f)lp_type_mapper,
	 sizeof(person)},
	{

		(init_f)location_init,
		(pre_run_f)NULL,
		(event_f)location_event,
		(revent_f)location_event_reverse,
		(commit_f)location_commit,
		(final_f)location_final,
		(map_f)lp_type_mapper,
		sizeof(location)},
	{0}, // End marker
};

main(int argc, char **argv, char **env)
{

	int i;

	tw_opt_add(app_opt);
	tw_init(&argc, &argv);
	tw_define_lps(NUM_LOCATIONS + NUM_PEOPLE, sizeof(Msg_Data));

	MPI_File mpi_file;
	MPI_Comm comm = MPI_COMM_WORLD;
	MPI_File_open(comm, "output.dat", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &mpi_file);

	for (i = 0; i < NUM_LOCATIONS + NUM_PEOPLE; i++)
	{
		if (i < NUM_LOCATIONS)
		{
			tw_lp_settype(i, &my_lps[1]);
		}
		else
		{
			tw_lp_settype(i, &my_lps[0]);
		}
	}

	tw_run();

	/*

	MAYBE WE COME UP WITH OUR OWN STATS TO ADD HERE, would want to probs output to the file and not just print tho

	if(tw_ismaster())
	{
		printf("\nAirport Model Statistics:\n");
		printf("\t%-50s %11.4lf\n", "Average Waiting Time", wait_time_avg);
		printf("\t%-50s %11lld\n", "Number of airports",
			nlp_per_pe * g_tw_npe * tw_nnodes());
		printf("\t%-50s %11lld\n", "Number of planes",
			planes_per_airport * nlp_per_pe * g_tw_npe * tw_nnodes());
	}

	*/

	tw_end();

	return 0;
}