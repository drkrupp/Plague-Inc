
#include "plagueInc.h"

MPI_File mpi_file;

tw_peid mapping(tw_lpid gid)
{
	return (tw_peid)gid / g_tw_nlp;
}

void location_init(location_state *s, tw_lp *lp)
{

	printf("location_init: LP %lu | state ptr: %p | rng: %p\n", lp->gid, s, lp->rng);

	if (!lp->rng)
	{
		tw_error(TW_LOC, "location_init: RNG is NULL for LP %lu\n", lp->gid);
	}

	// Set LP grid coordinates
	s->x = lp->gid % GRID_WIDTH;
	s->y = lp->gid / GRID_WIDTH;
	s->max_people_held = MAX_PEOPLE_PER_LOCATION;
	s->people = (person_state *)malloc(sizeof(person_state) * s->max_people_held);
	s->num_people = PEOPLE_PER_LOCATION;

	printf("LP %lu mapped to grid (%d, %d)\n", lp->gid, s->x, s->y);

	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		// printf("  Init person[%d] for LP %lu\n", i, lp->gid);
		int id = (lp->gid) * PEOPLE_PER_LOCATION + i;
		s->people[i].x = s->x;
		s->people[i].y = s->y;
		s->people[i].alive = true;
		s->people[i].infected = false;
		s->people[i].susceptible = true;
		s->people[i].id = id;

		double r = tw_rand_unif(lp->rng);
		printf("    person[%d] random init val: %f\n", id, r);

		if (r < INITIAL_INFECTED_RATE)
		{
			s->people[i].infected = true;
			s->people[i].infected_time = 0.0;
			s->people[i].susceptible = false;
			printf("    person[%d] initialized as infected\n", id);
		}
	}

	tw_event *e;
	event_msg *m;
	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		m = tw_event_data(e);
		m->type = STATUS_UPDATE;
		m->person_index = i;
		tw_event_send(e);
	}

	int alive = 0, dead = 0, infected = 0;

	for (int i = 0; i < s->num_people; i++)
	{
		if (!s->people[i].alive)
			dead++;
		else
			alive++;
		if (s->people[i].infected)
			infected++;
	}

	int buf_length = 256;
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "INIT-LP %lu |Coords: (%d, %d)| Alive: %d | Dead: %d | Infected: %d | Time: %f\n", lp->gid, s->x, s->y, alive, dead, infected, tw_now(lp));
	int steps = 1001;
	int space_needed = buf_length * steps;
	MPI_Offset offset = (MPI_Offset)(lp->gid * space_needed);
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

person_state *deep_copy_person_states(const person_state *original, size_t length)
{
	if (original == NULL || length == 0)
		return NULL;

	person_state *copy = (person_state *)malloc(length * sizeof(person_state));
	if (copy == NULL)
	{
		perror("Failed to allocate memory for array copy");
		exit(EXIT_FAILURE);
	}

	memcpy(copy, original, length * sizeof(person_state));
	return copy;
}

void location_event(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	person_state *initial_people = deep_copy_person_states(m->people, s->max_people_held);
	int initial_max_people = s->max_people_held;
	int person_index;
	person_state arriving_state;
	switch (m->type)
	{
	case ARRIVAL:
	{
		arriving_state = m->arriving_state;
		if (s->num_people == s->max_people_held)
		{
			s->max_people_held *= 2;
			s->people = (person_state *)realloc(s->people, sizeof(person_state) * s->max_people_held);
		}
		s->people[s->num_people] = arriving_state;
		person_index = s->num_people;
		s->num_people++;
		break;
	}
	case DEPARTURE:
	{
		person_index = m->person_index;
		s->people[person_index] = s->people[s->num_people - 1];
		s->num_people--;
		return; // don't handle status updates for people who have left the location
	}
	default:
	{
		person_index = m->person_index;
		break;
	}
	}

	person_state p = s->people[person_index];

	if (!p.alive)
		return;

	// Calculating infections death
	if (p.infected)
	{
		if (tw_rand_unif(lp->rng) < DEATH_RATE)
		{
			s->people[person_index].alive = false;
			s->people[person_index].infected = false;
			s->people[person_index].susceptible = false;
			tw_output(lp, "    person[%d] died\n", p.id);

			return;
		}
	}
	else if (!p.susceptible && (tw_now(lp) - p.immune_start > IMMUNITY_TIME))
	{
		p.susceptible = true;
		tw_output(lp, "    person[%d] lost immunity\n", person_index);
	}

	// Calculating recovery from infected
	if (p.infected && (tw_now(lp) - p.infected_time > INFECTION_TIME))
	{
		double r = tw_rand_unif(lp->rng);
		tw_output(lp, "  person[%d] infected for long enough, rand: %f\n", p.id, r);

		if (r < RECOVERY_RATE)
		{
			p.infected = false;
			p.susceptible = false;
			p.immune_start = tw_now(lp);
			tw_output(lp, "    person[%d] recovered\n", p.id);
		}
	}
	else if (p.susceptible)
	{
		for (int j = 0; j < s->num_people; j++)
		{
			if (person_index == j || !s->people[j].infected || !s->people[j].alive)
				continue;

			if (tw_rand_unif(lp->rng) < TRANSMISSION_RATE)
			{
				p.susceptible = false;
				p.infected = true;
				p.infected_time = tw_now(lp);
				tw_output(lp, "    person[%d] got infected by person[%d]\n", p.id, s->people[j].id);
				break;
			}
		}
	}


	//update state
	s->people[person_index].alive = p->alive;
	s->people[person_index].infected = p->infected;
	s->people[person_index].susceptible = p->susceptible;


	//move
	if (tw_rand_unif(lp->rng) < MOVE_PROBABILITY)
	{
		int rand_result = tw_rand_integer(lp->rng, 0, 3);
		tw_lpid dst_lp = 0;
		int width = GRID_WIDTH;
		int one_less = GRID_WIDTH - 1;
		tw_output(lp, "person %d moving ", p.id);
		switch (rand_result)
		{
		// LOOK INTO HOW THIS MOVEMENT WORKS - DONT FORGET IT
		case 0:
		{
			// Fly north
			// nlp_per_pe is 1024 in airplane version (32*32)
			// could be keeping each section of the grid independant
			// Should try two different things,
			// Could have 32 = GRID_WIDTH, 31 = GRID_WIDTH -1
			// Could have 32 = GRID_WIDTH/tw_nnodes, 31 = (GRID_WIDTH/tw_nnodes) - 1
			// Could have another value involving grid width divided by other factor
			// Like GRID_WIDTH and GRID_WIDTH -1
			tw_output(lp, "north\n");
			if (lp->gid < width)
				// Wrap around
				dst_lp = lp->gid + one_less * width;
			else
				dst_lp = lp->gid - one_less;
			break;
		}
		case 1:
		{
			tw_output(lp, "south\n");
			// Fly south
			if (lp->gid >= one_less * width)
				// Wrap around
				dst_lp = lp->gid - one_less * width;
			else
				dst_lp = lp->gid + one_less;
			break;
		}
		case 2:
		{
			tw_output(lp, "east\n");
			// Fly east
			if ((lp->gid % width) == one_less)
				// Wrap around
				dst_lp = lp->gid - one_less;
			else
				dst_lp = lp->gid + 1;
			break;
		}
		case 3:
		{
			tw_output(lp, "west\n");
			// Fly west
			if ((lp->gid % width) == 0)
				// Wrap around
				dst_lp = lp->gid + one_less;
			else
				dst_lp = lp->gid - 1;
			break;
		}
		}

		tw_event *e_leave;
		event_msg *m_leave;
		e_leave = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		m_leave = tw_event_data(e_leave);
		m_leave->type = DEPARTURE;
		m_leave->person_index = person_index;
		m_leave->max_people_count = initial_max_people;
		m_leave->people = initial_people;
		tw_event_send(e_leave);

		tw_event *e_arrive;
		event_msg *m_arrive;
		e_arrive = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		m_arrive = tw_event_data(e_arrive);
		m_arrive->type = ARRIVAL;
		m_arrive->arriving_state = p;
		m_arrive->max_people_count = initial_max_people;
		m_arrive->people = initial_people;
		tw_event_send(e_arrive);
	}
	else
	{
		tw_event *e_stay;
		event_msg *m_stay;
		e_stay = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		m_stay = tw_event_data(e_stay);
		m_stay->type = STATUS_UPDATE;
		m_stay->person_index = person_index;
		m_stay->max_people_count = initial_max_people;
		m_stay->people = initial_people;
		tw_event_send(e_stay);
	}
}
void location_event_reverse(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	tw_output(lp, "BIG LOCATION REVERSE YA BOMBOCLAAT");
	switch (m->type)
	{
	case ARRIVAL:
	{
		s->num_people--;
		break;
	}
	case DEPARTURE:
	{
		s->num_people++;
		break;
	}
	default:
	{
		break;
	}
	}
	s->people = deep_copy_person_states(m->people, m->max_people_count);
}

void location_final(location_state *s, tw_lp *lp)
{
	int alive = 0, dead = 0, infected = 0;

	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		if (!s->people[i].alive)
			dead++;
		else
			alive++;
		if (s->people[i].infected)
			infected++;
	}

	tw_output(lp, "FINAL: LP %lu | Alive: %d | Dead: %d | Infected: %d\n", lp->gid, alive, dead, infected);
}

void location_commit(location_state *s, tw_bf *bf, event_msg *in_msg, tw_lp *lp)
{
	int alive = 0, dead = 0, infected = 0;

	for (int i = 0; i < s->num_people; i++)
	{
		if (!s->people[i].alive)
			dead++;
		else
			alive++;
		if (s->people[i].infected)
			infected++;
	}

	int buf_length = 256;
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "LP %lu |Coords: (%d, %d)| Alive: %d | Dead: %d | Infected: %d | Time: %f\n", lp->gid, s->x, s->y, alive, dead, infected, tw_now(lp));
	int steps = 1001;
	int space_needed = buf_length * steps;
	MPI_Offset offset = (MPI_Offset)(lp->gid * space_needed + (long)tw_now(lp) * buf_length);
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

tw_lptype my_lps[] =
	{
		{
			(init_f)location_init,
			(pre_run_f)NULL,
			(event_f)location_event,
			(revent_f)location_event_reverse,
			(commit_f)location_commit,
			(final_f)location_final,
			(map_f)mapping,
			sizeof(location_state),
		},
		{0},
};

tw_lpid lp_typemap(tw_lpid gid)
{
	return 0; // You only have one LP type
}

int main(int argc, char **argv, char **env)
{
	printf("main: Starting ROSS plague simulation\n");
	g_tw_lookahead = 1.0;
	g_tw_events_per_pe = 8192;
	tw_opt_add(NULL);
	tw_init(&argc, &argv);

	int NUM_LOCATIONS = GRID_WIDTH * GRID_HEIGHT;
	// nlp_per_pe = NUM_LOCATIONS / (tw_nnodes() * g_tw_npe);
	// NUM_LOCATIONS / NUM_PROCESSES (-np)
	long nlp_per_pe = NUM_LOCATIONS / 2;

	tw_define_lps(nlp_per_pe, sizeof(event_msg));
	printf("main: LP types registered\n");

	g_tw_lp_types = my_lps;
	g_tw_lp_typemap = lp_typemap;
	tw_lp_setup_types();

	printf("main: Running simulation...\n");

	MPI_Comm comm = MPI_COMM_WORLD;
	int rc = MPI_File_open(comm, "output.dat",
						   MPI_MODE_CREATE | MPI_MODE_WRONLY,
						   MPI_INFO_NULL, &mpi_file);
	if (rc != MPI_SUCCESS)
	{
		printf("MPI_File_open failed\n");
		fflush(stdout);
	}
	else
	{
		// Explicitly truncate the file to clear old content
		MPI_File_set_size(mpi_file, 0);
	}

	tw_run();

	if (tw_ismaster())
	{
		printf("\nAModel Statistics:\n");
		// 2 processes
		printf("\t%-50s %11ld\n", "Number of locations", nlp_per_pe * 2);
		printf("\t%-50s %11ld\n", "Number of people", PEOPLE_PER_LOCATION * nlp_per_pe * 2);
	}

	tw_end();

	return 0;
}