
#include "plagueInc.h"

MPI_File mpi_file;

// global file pointer
// FILE *node_out_file;

tw_peid mapping(tw_lpid gid)
{
	return (tw_peid)gid / g_tw_nlp;
}

void location_init(location_state *s, tw_lp *lp)
{

	// Set LP grid coordinates
	s->x = lp->gid % GRID_WIDTH;
	s->y = lp->gid / GRID_WIDTH;
	s->max_people_held = MAX_PEOPLE_PER_LOCATION;
	s->people = (person_state *)malloc(sizeof(person_state) * s->max_people_held);
	s->num_people = PEOPLE_PER_LOCATION;
	// printf("LP %lu mapped to grid (%d, %d)\n", lp->gid, s->x, s->y);

	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		int id = (lp->gid) * PEOPLE_PER_LOCATION + i;
		s->people[i].x = s->x;
		s->people[i].y = s->y;
		s->people[i].alive = true;
		s->people[i].infected = false;
		s->people[i].susceptible = true;
		s->people[i].id = id;
		double r = tw_rand_unif(lp->rng);
		if (r < INITIAL_INFECTED_RATE)
		{
			s->people[i].infected = true;
			s->people[i].infected_time = 0.0;
			s->people[i].susceptible = false;
		}
	}
	tw_event *e;
	event_msg *m;
	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		// e = tw_event_new(lp->gid, 50, lp);
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
	int steps = STEPS + 1;
	int space_needed = buf_length * steps;
	MPI_Offset offset = (MPI_Offset)(lp->gid * space_needed);
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
}

void location_event(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	int person_index;

	// 1) Take a full snapshot of the array for rollback
	m->before_num_people = s->num_people;
	memcpy(
		m->before_people,
		s->people,
		sizeof(person_state) * (size_t)s->num_people);

	switch (m->type)
	{
	case ARRIVAL:
	{
		person_state arriving_state = m->arriving_state;
		if (s->num_people == s->max_people_held)
		{
			s->max_people_held *= 2;
			s->people = (person_state *)realloc(s->people, sizeof(person_state) * s->max_people_held);
		}
		s->people[s->num_people] = arriving_state;
		person_index = s->num_people;
		s->num_people++;
		tw_event *e_status;
		event_msg *m_status;
		e_status = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		m_status = tw_event_data(e_status);
		m_status->type = STATUS_UPDATE;
		m_status->person_index = person_index;
		tw_event_send(e_status);
		break;
	}
	case DEPARTURE:
	{
		person_index = m->person_index;
		if (person_index != s->num_people - 1)
		{
			s->people[person_index] = s->people[s->num_people - 1];
		}

		s->num_people--;
		break; // don't handle status updates for people who have left the location
	}
	case STATUS_UPDATE:
	{
		person_index = m->person_index;
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

				return;
			}
		}
		else if (!p.susceptible && (tw_now(lp) - p.immune_start > IMMUNITY_TIME))
		{
			p.susceptible = true;
		}

		// Calculating recovery from infected
		if (p.infected && (tw_now(lp) - p.infected_time > INFECTION_TIME))
		{
			double r = tw_rand_unif(lp->rng);

			if (r < RECOVERY_RATE)
			{
				p.infected = false;
				p.susceptible = false;
				p.immune_start = tw_now(lp);
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
					break;
				}
			}
		}

		s->people[person_index] = p;

		if (tw_rand_unif(lp->rng) < MOVE_PROBABILITY)
		{
			int rand_result = tw_rand_integer(lp->rng, 0, 3);
			tw_lpid dst_lp = 0;
			int width = GRID_WIDTH;
			int one_less = GRID_WIDTH - 1;
			switch (rand_result)
			{
			case 0: // NORTH
			{
				if (s->y == one_less)
					// Wrap around
					dst_lp = s->x;
				else
					dst_lp = lp->gid + width;

				break;
			}
			case 1: // SOUTH
			{
				if (s->y == 0)
					// Wrap around
					dst_lp = width * one_less + s->x;
				else
					dst_lp = lp->gid - width;
				break;
			}
			case 2:
			{
				// Fly east
				if (s->x == one_less)
					// Wrap around
					dst_lp = lp->gid - one_less;
				else
					dst_lp = lp->gid + 1;
				break;
			}
			case 3:
			{
				// Fly west
				if (s->x == 0)
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
			tw_event_send(e_leave);

			tw_event *e_arrive;
			event_msg *m_arrive;
			e_arrive = tw_event_new(dst_lp, tw_rand_exponential(lp->rng, 1.0), lp);
			m_arrive = tw_event_data(e_arrive);
			m_arrive->type = ARRIVAL;
			m_arrive->arriving_state = p;
			tw_event_send(e_arrive);
		}
		else
		{
			tw_event *e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
			event_msg *mm = tw_event_data(e);
			mm->type = STATUS_UPDATE;
			mm->person_index = person_index;
			tw_event_send(e);
		}
		break;
	}
	}
}

void location_event_reverse(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	s->num_people = m->before_num_people;
	memcpy(s->people,
		   m->before_people,
		   sizeof(person_state) * m->before_num_people);
}

void location_final(location_state *s, tw_lp *lp)
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
	int len = snprintf(buf, sizeof(buf), "FINAL: LP %lu |Coords: (%d, %d)| Alive: %d | Dead: %d | Infected: %d | Time: %f\n", lp->gid, s->x, s->y, alive, dead, infected, tw_now(lp));
	int steps = STEPS + 1;
	int space_needed_per_lp = buf_length * steps;
	int num_lps = GRID_HEIGHT * GRID_WIDTH;
	int end_spot = num_lps * space_needed_per_lp;
	MPI_Offset offset = (MPI_Offset)(end_spot + lp->gid * buf_length);
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);

	tw_output(lp, "FINAL: LP %lu | Alive: %d | Dead: %d | Infected: %d\n", lp->gid, alive, dead, infected);
	free(s->people);
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

	ticks start_print = getticks();
	int buf_length = 256;
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "LP %lu |Coords: (%d, %d)| Alive: %d | Dead: %d | Infected: %d | Time: %f\n", lp->gid, s->x, s->y, alive, dead, infected, tw_now(lp));
	int steps = STEPS + 1;
	int space_needed = buf_length * steps;
	MPI_Offset offset = (MPI_Offset)(lp->gid * space_needed + (long)tw_now(lp) * buf_length);
	MPI_File_write_at(mpi_file, offset, buf, len, MPI_CHAR, MPI_STATUS_IGNORE);
	ticks end_print = getticks();
	s->io_print_time += (end_print - start_print);
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

	ticks start = getticks();
	printf("main: Starting ROSS plague simulation\n");
	g_tw_lookahead = 0.00000001;

	g_tw_events_per_pe = (GRID_WIDTH * GRID_HEIGHT * PEOPLE_PER_LOCATION * 2) / NUM_PROCESSES;
	tw_opt_add(NULL);
	tw_init(&argc, &argv);

	int NUM_LOCATIONS = GRID_WIDTH * GRID_HEIGHT;
	// nlp_per_pe = NUM_LOCATIONS / (tw_nnodes() * g_tw_npe);
	// NUM_LOCATIONS / NUM_PROCESSES (-np)
	long nlp_per_pe = NUM_LOCATIONS / NUM_PROCESSES;

	tw_define_lps(nlp_per_pe, sizeof(event_msg));

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

	ticks finish = getticks();
	if (tw_ismaster())
	{
		printf("\nModel Statistics:\n");
		// 2 processes
		printf("\t%-50s %11ld\n", "Number of locations", nlp_per_pe * NUM_PROCESSES);
		printf("\t%-50s %11ld\n", "Number of people", PEOPLE_PER_LOCATION * nlp_per_pe * NUM_PROCESSES);
		printf("Computed in %llu ticks\n", (finish - start));
	}

	tw_end();

	return 0;
}