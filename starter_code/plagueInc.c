#include "plagueInc.h"

// Global counters for summary statistics
int total_infected = 0;
int total_dead = 0;
int total_recovered = 0;
MPI_File mpi_file;
// Mapping functions for custom mapping

tw_peid custom_mapping_lp_to_pe(tw_lpid gid)
{
	return gid % tw_nnodes();
}

tw_lp *custom_mapping_lpgid_to_local(tw_lpid gid)
{
	tw_lpid local_index = gid / tw_nnodes();
	return g_tw_lp[local_index];
}

void custom_mapping_setup(void)
{
	tw_kp_onpe(0, g_tw_pe);

	int total_lps = NUM_LOCATIONS + NUM_PEOPLE;
	int min_num_lps_per_pe = total_lps / tw_nnodes();
	int remainder = total_lps % tw_nnodes();
	int lps_on_this_pe = min_num_lps_per_pe + (g_tw_mynode < remainder ? 1 : 0);

	for (int i = 0; i < lps_on_this_pe; i++)
	{
		tw_lp_onkp(g_tw_lp[i], g_tw_kp[0]);
		tw_lpid gid = g_tw_mynode + (i * tw_nnodes());
		tw_lp_onpe(i, g_tw_pe, gid);
	}
}

// Map LP to type: 0 = person, 1 = location
tw_lpid lp_type_mapper(tw_lpid gid)
{
	if (gid < NUM_LOCATIONS)
		return 1; // Location
	else
		return 0; // Person
}

int get_x_spot(tw_lpid gid)
{
	return gid % GRID_WIDTH;
}

int get_y_spot(tw_lpid gid)
{
	return gid / GRID_WIDTH;
}

tw_lpid get_location_id(int x, int y)
{
	tw_lpid id = y * GRID_WIDTH + x;
	printf("MAPPING COORD (%d, %d) TO LOCATION ID %lu\n", x, y, (unsigned long)id);
	return id;
}

// Initialization function for each person
void person_init(person *s, tw_lp *lp)
{
	printf("[Init] Person LP %lu on PE %lu\n", (unsigned long)lp->gid, (unsigned long)g_tw_mynode);
	s->moves_left = MOVES;
	s->x_spot = tw_rand_integer(lp->rng, 0, GRID_WIDTH - 1);
	s->y_spot = tw_rand_integer(lp->rng, 0, GRID_HEIGHT - 1);
	s->alive = true;

	double r = tw_rand_unif(lp->rng);
	if (r < STARTING_INFECTED_PERCENTAGE)
	{
		s->infected_time = tw_now(lp);
		s->susceptible = false;
		s->infected = true;
		s->immune = false;
	}
	else
	{
		s->susceptible = true;
		s->infected = false;
		s->immune = false;
	}

	printf("INIT PERSON LP %lu at (%d,%d). Infected: %d\n", (unsigned long)lp->gid, s->x_spot, s->y_spot, s->infected);
	fflush(stdout);
	tw_lpid location_id = get_location_id(s->x_spot, s->y_spot);
	if (location_id >= NUM_LOCATIONS)
	{
		printf("ERROR: Person LP %lu got invalid location ID %lu\n", (unsigned long)lp->gid, (unsigned long)location_id);
		fflush(stdout);
	}
	tw_event *e_arrive = tw_event_new(location_id, 1, lp);
	Msg_Data *m_arrive = tw_event_data(e_arrive);
	if (!m_arrive)
	{
		printf("Null Msg_Data pointer in person_init LP %lu\n", (unsigned long)lp->gid);
		fflush(stdout);
		return;
	}
	m_arrive->event_type = ARRIVAL;
	m_arrive->person_infected = s->infected;
	m_arrive->person_id = lp->gid;
	m_arrive->person_state = *s;
	tw_event_send(e_arrive);
}

// Initialization function for each location
void location_init(location *s, tw_lp *lp)
{
	tw_lpid gid = lp->gid;
	s->x_spot = get_x_spot(gid);
	s->y_spot = get_y_spot(gid);
	s->infected_count = 0;
	s->total_people = 0;
	s->max_people_held = INITIAL_MAX_PEOPLE_PER_SQUARE;
	s->people_held = (tw_lpid *)malloc(sizeof(tw_lpid) * INITIAL_MAX_PEOPLE_PER_SQUARE);

	printf("INIT LOCATION LP %lu at (%d,%d) on PE %lu\n", (unsigned long)lp->gid, s->x_spot, s->y_spot, (unsigned long)g_tw_mynode);
	fflush(stdout);
}

// Determine next move direction for a person
enum abs_directions assign_move(tw_lp *lp, int x_spot, int y_spot)
{
	enum abs_directions possible_moves[8];
	int count = 0;

	// Populate possible directions based on boundaries
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

	// Choose a random direction from valid options
	return possible_moves[tw_rand_integer(lp->rng, 0, count - 1)];
}

// Event handler for person LPs
void person_event(person *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{

	// Stop sending events if out of moves
	if (s->moves_left <= 0)
		return;
	s->moves_left--;

	tw_stime now_time = tw_now(lp);
	// Debugging
	printf("PERSON EVENT: LP %lu at time %f. Alive: %d, Infected: %d, Susceptible: %d, Immune: %d\n",
		   lp->gid, now_time, s->alive, s->infected, s->susceptible, s->immune);
	fflush(stdout);

	bool new_infection = false;
	bool new_recovery = false;
	int x_spot = s->x_spot;
	int y_spot = s->y_spot;
	bool previously_infected = s->infected;

	// Handle immunity expiration
	if (s->immune && (now_time - s->immune_start > IMMUNE_TIME))
	{
		s->immune = false;
		s->susceptible = true;
	}

	// Check for new infection
	if (s->susceptible)
	{
		for (int i = 0; i < msg->infected_count; i++)
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
	// Handle death or recovery if already infected
	else
	{
		if (tw_rand_unif(lp->rng) < LETHALITY)
		{
			s->alive = false;
			tw_lpid location_id = get_location_id(x_spot, y_spot);
			tw_event *e = tw_event_new(location_id, 1, lp);
			Msg_Data *m = tw_event_data(e);
			if (!m)
			{
				printf("tw_event_data returned NULL!\n");
				return;
			}
			m->event_type = DEPARTURE;
			tw_event_send(e);
			return;
		}
		if (now_time - s->infected_time > INFECTION_TIME)
		{
			s->infected = false;
			s->immune = true;
			s->immune_start = now_time;
			new_recovery = true;
		}
	}

	// Move the person to a new location or stay
	if (s->alive)
	{
		tw_lpid prev_location_id = get_location_id(x_spot, y_spot);
		enum abs_directions next_move = assign_move(lp, x_spot, y_spot);
		// Update coordinates based on move
		switch (next_move)
		{
		case NORTH:
			y_spot++;
			break;
		case NORTH_EAST:
			y_spot++;
			x_spot++;
			break;
		case EAST:
			x_spot++;
			break;
		case SOUTH_EAST:
			y_spot--;
			x_spot++;
			break;
		case SOUTH:
			y_spot--;
			break;
		case SOUTH_WEST:
			y_spot--;
			x_spot--;
			break;
		case WEST:
			x_spot--;
			break;
		case NORTH_WEST:
			y_spot++;
			x_spot--;
			break;
		case NO_MOVE:
			if (new_infection || new_recovery)
			{
				tw_event *e = tw_event_new(prev_location_id, 1, lp);
				Msg_Data *m = tw_event_data(e);
				if (!m)
				{
					printf("tw_event_data returned NULL!\n");
					return;
				}
				m->event_type = new_infection ? NEW_INFECTION : NEW_RECOVERY;
				tw_event_send(e);
			}
			return;
		}

		s->x_spot = x_spot;
		s->y_spot = y_spot;
		tw_lpid new_location_id = get_location_id(x_spot, y_spot);

		// Send DEPARTURE event to old location
		tw_event *e_leave = tw_event_new(prev_location_id, 1, lp);
		Msg_Data *m_leave = tw_event_data(e_leave);
		if (!m_leave)
		{
			printf("tw_event_data returned NULL!\n");
			return;
		}
		m_leave->event_type = DEPARTURE;
		m_leave->person_infected = previously_infected;
		m_leave->person_id = lp->gid;
		tw_event_send(e_leave);

		// Send ARRIVAL event to new location
		tw_event *e_arrive = tw_event_new(new_location_id, 1, lp);
		Msg_Data *m_arrive = tw_event_data(e_arrive);
		if (!m_arrive)
		{
			printf("tw_event_data returned NULL!\n");
			return;
		}
		m_arrive->event_type = ARRIVAL;
		m_arrive->person_infected = s->infected;
		m_arrive->person_id = lp->gid;
		m_arrive->person_state = *s;
		tw_event_send(e_arrive);
	}
}

// Rollback for person events
void person_event_reverse(person *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	*s = msg->person_state;
}

// Location LP event handler: update people count and infections
void location_event(location *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{

	enum events event = msg->event_type;
	tw_lpid person_id = msg->person_id;

	// Debugging
	printf("LOCATION EVENT: LP %lu Type %d for Person %lu. Infected Count: %d, Total People: %d\n",
		   lp->gid, msg->event_type, person_id, s->infected_count, s->total_people);
	fflush(stdout);
	switch (event)
	{
	case ARRIVAL:
		// Add person to location
		if (s->total_people == s->max_people_held)
		{
			s->max_people_held *= 2;
			s->people_held = (tw_lpid *)realloc(s->people_held, sizeof(tw_lpid) * s->max_people_held);
		}
		if (msg->person_infected)
			s->infected_count++;
		s->people_held[s->total_people++] = person_id;
		break;

	case DEPARTURE:
		// Remove person from location
		for (int i = 0; i < s->total_people; i++)
		{
			if (s->people_held[i] == person_id)
			{
				s->people_held[i] = s->people_held[s->total_people - 1];
				break;
			}
		}
		if (msg->person_infected)
			s->infected_count--;
		s->total_people--;
		return;

	case NEW_INFECTION:
		s->infected_count++;
		break;

	case NEW_RECOVERY:
		s->infected_count--;
		break;

	default:
		break;
	}

	// Trigger a STATUS_CHECK message for the person
	tw_event *e = tw_event_new(person_id, tw_rand_exponential(lp->rng, AVERAGE_MOVE_TIME), lp);
	Msg_Data *m = tw_event_data(e);
	if (!m)
	{
		printf("tw_event_data returned NULL!\n");
		return;
	}
	m->event_type = STATUS_CHECK;
	m->infected_count = s->infected_count;
	m->person_state = msg->person_state;
	tw_event_send(e);
}

// Reverse event handler for locations
void location_event_reverse(location *s, tw_bf *bf, Msg_Data *msg, tw_lp *lp)
{
	enum events event = msg->event_type;
	tw_lpid person_id = msg->person_id;

	switch (event)
	{
	case ARRIVAL:
		// Reverse ARRIVAL: remove person
		for (int i = 0; i < s->total_people; i++)
		{
			if (s->people_held[i] == person_id)
			{
				s->people_held[i] = s->people_held[s->total_people - 1];
				break;
			}
		}
		s->total_people--;
		if (msg->person_infected)
			s->infected_count--;
		break;

	case DEPARTURE:
		// Reverse DEPARTURE: add person back
		s->people_held[s->total_people++] = person_id;
		if (msg->person_infected)
			s->infected_count++;
		break;

	case NEW_INFECTION:
		s->infected_count--;
		break;

	case NEW_RECOVERY:
		s->infected_count++;
		break;
	case STATUS_CHECK:
		break;
	}
}

// Finalization hook for person LPs (optional stats)
void person_final(person *s, tw_lp *lp)
{
	// Could output final infection state or death counts here
}

// Finalization hook for location LPs
void location_final(location *s, tw_lp *lp)
{
	free(s->people_held); // Clean up dynamic array
}

// Logging and commit for person LPs
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

	// Update global stats
	if (became_infected)
		total_infected++;
	if (died)
		total_dead++;
	if (recovered)
		total_recovered++;

	char buf[128];
	int len = snprintf(buf, sizeof(buf), "Time: %lf, Person %lu, Infected: %s, Became Infected: %s, Recovered: %s, Died: %s\n Moved from (%d, %d) to (%d, %d)\n",
					   tw_now(lp), lp->gid, s->infected ? "true" : "false",
					   became_infected ? "true" : "false",
					   recovered ? "true" : "false",
					   died ? "true" : "false",
					   prev_x, prev_y, s->x_spot, s->y_spot);

	MPI_Offset offset = (MPI_Offset)(lp->gid * 1000 + (long)(tw_now(lp) * 10));
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

// Logging and commit for location LPs
void location_commit(location *s, tw_bf *bf, Msg_Data *in_msg, tw_lp *lp)
{
	char buf[128];
	int len = snprintf(buf, sizeof(buf),
					   "Time: %lf, Location %lu, Coordinates: (%d, %d), Infected Count: %d, Total People: %d\n",
					   tw_now(lp), lp->gid, s->x_spot, s->y_spot, s->infected_count, s->total_people);

	MPI_Offset offset = (MPI_Offset)(lp->gid * 1000 + (long)(tw_now(lp) * 10));
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

// Define logical process types: person and location
// LP type 0 = person, LP type 1 = location
tw_lptype my_lps[] = {
	{(init_f)person_init,
	 (pre_run_f)NULL,
	 (event_f)person_event,
	 (revent_f)person_event_reverse,
	 (commit_f)person_commit,
	 (final_f)person_final,
	 (map_f)custom_mapping_lp_to_pe,
	 sizeof(person)},
	{

		(init_f)location_init,
		(pre_run_f)NULL,
		(event_f)location_event,
		(revent_f)location_event_reverse,
		(commit_f)location_commit,
		(final_f)location_final,
		(map_f)custom_mapping_lp_to_pe,
		sizeof(location)},
	{0}, // End marker
};

// tw_lptype my_lps[] = {
// 	{(init_f)person_init,
// 	 (pre_run_f)NULL,
// 	 (event_f)person_event,
// 	 (revent_f)person_event_reverse,
// 	 (commit_f)person_commit,
// 	 (final_f)person_final,
// 	 (map_f)lp_type_mapper,
// 	 sizeof(person)},
// 	{

// 		(init_f)location_init,
// 		(pre_run_f)NULL,
// 		(event_f)location_event,
// 		(revent_f)location_event_reverse,
// 		(commit_f)location_commit,
// 		(final_f)location_final,
// 		(map_f)lp_type_mapper,
// 		sizeof(location)},
// 	{0}, // End marker
// };

int main(int argc, char **argv, char **env)
{
	tw_opt_add(NULL);
	tw_init(&argc, &argv);

	int n_lps = NUM_LOCATIONS + NUM_PEOPLE;

	if (tw_ismaster())
	{
		printf("Grid: %d x %d, Locations: %d, People: %d, Total LPs: %d\n",
			   GRID_WIDTH, GRID_HEIGHT, NUM_LOCATIONS, NUM_PEOPLE, n_lps);
	}

	g_tw_mapping = CUSTOM;
	g_tw_custom_initial_mapping = &custom_mapping_setup;
	g_tw_custom_lp_global_to_local_map = &custom_mapping_lpgid_to_local;
	// g_tw_custom_lp_global_to_pe_map = &custom_mapping_lp_to_pe;
	g_tw_lp_types = lp_types;

	tw_define_lps(n_lps, sizeof(Msg_Data));

	if (tw_ismaster())
	{
		printf("After tw_define_lps: g_tw_nlp = %d, g_tw_mynode = %lu\n",
			   g_tw_nlp, (unsigned long)g_tw_mynode);
		fflush(stdout);
	}

	for (int i = 0; i < g_tw_nlp; i++)
	{
		tw_lp *l = g_tw_lp[i];
		tw_lpid gid = l->gid;

		if (gid < NUM_LOCATIONS)
		{
			tw_lp_settype(gid, &my_lps[1]); // Location
			printf("LP %lu (local idx %d) assigned type Location on PE %lu\n",
				   (unsigned long)gid, i, (unsigned long)g_tw_mynode);
		}
		else
		{
			tw_lp_settype(gid, &my_lps[0]); // Person
			printf("LP %lu (local idx %d) assigned type Person on PE %lu\n",
				   (unsigned long)gid, i, (unsigned long)g_tw_mynode);
		}
		fflush(stdout);
	}

	MPI_Comm comm = MPI_COMM_WORLD;
	if (MPI_File_open(comm, "output.dat", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &mpi_file) != MPI_SUCCESS)
	{
		printf("MPI_File_open failed\n");
		fflush(stdout);
	}

	tw_run();

	if (tw_ismaster())
	{
		MPI_File summary_file;
		if (MPI_File_open(comm, "summary_stats.dat", MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &summary_file) != MPI_SUCCESS)
		{
			printf("MPI_File_open for summary failed\n");
			fflush(stdout);
		}
		char buf[256];
		int len = snprintf(buf, sizeof(buf), "Total People: %d\nTotal Infected: %d\nTotal Dead: %d\nTotal Recovered: %d\n",
						   NUM_PEOPLE, total_infected, total_dead, total_recovered);
		MPI_File_write_at(summary_file, 0, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
		MPI_File_close(&summary_file);

		printf("SIM COMPLETE: Infected: %d, Dead: %d, Recovered: %d\n",
			   total_infected, total_dead, total_recovered);
		fflush(stdout);
	}

	tw_end();
	return 0;
}
