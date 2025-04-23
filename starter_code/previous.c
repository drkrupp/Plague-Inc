#include "plagueInc.h"
#include <ross.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

MPI_File mpi_file;

// Helper: find a person’s index by their stable ID
static int find_person_idx(location_state *s, int pid)
{
	for (int i = 0; i < s->num_people; i++)
	{
		if (s->people[i].id == pid)
			return i;
	}
	return -1;
}

void location_init(location_state *s, tw_lp *lp)
{
	// Set grid coords and allocate array
	s->x = lp->gid % GRID_WIDTH;
	s->y = lp->gid / GRID_HEIGHT;
	s->max_people_held = MAX_PEOPLE_PER_LOCATION;
	s->people = malloc(sizeof(person_state) * s->max_people_held);
	s->num_people = PEOPLE_PER_LOCATION;

	// Initialize each person
	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		person_state *p = &s->people[i];
		p->x = s->x;
		p->y = s->y;
		p->alive = true;
		p->infected = false;
		p->susceptible = true;
		p->id = lp->gid * PEOPLE_PER_LOCATION + i;
		if (tw_rand_unif(lp->rng) < INITIAL_INFECTED_RATE)
		{
			p->infected = true;
			p->infected_time = tw_now(lp);
			p->susceptible = false;
		}
	}

	// Schedule initial status updates
	for (int i = 0; i < s->num_people; i++)
	{
		tw_event *e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
		event_msg *mm = tw_event_data(e);
		mm->type = STATUS_UPDATE;
		mm->person_id = s->people[i].id;
		tw_event_send(e);
	}
}

void location_event(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	const int total_lps = GRID_WIDTH * GRID_HEIGHT;
	int pid = m->person_id;

	// ARRIVAL: append and schedule next status
	if (m->type == ARRIVAL)
	{
		if (s->num_people == s->max_people_held)
		{
			s->max_people_held *= 2;
			s->people = realloc(s->people, sizeof(person_state) * s->max_people_held);
		}
		s->people[s->num_people++] = m->arriving_state;
		tw_event *e = tw_event_new(lp->gid, tw_now(lp) + 1.0, lp);
		event_msg *mm = tw_event_data(e);
		mm->type = STATUS_UPDATE;
		mm->person_id = pid;
		tw_event_send(e);
		return;
	}

	// DEPARTURE: remove and forward as ARRIVAL to dest_gid
	if (m->type == DEPARTURE)
	{
		int idx = find_person_idx(s, pid);
		if (idx >= 0)
		{
			person_state p = s->people[idx];
			s->people[idx] = s->people[--s->num_people];
			int dst = m->dest_gid;
			dst = (dst + total_lps) % total_lps;
			tw_event *ea = tw_event_new(dst, tw_now(lp) + 1.0, lp);
			event_msg *ma = tw_event_data(ea);
			ma->type = ARRIVAL;
			ma->person_id = pid;
			ma->arriving_state = p;
			tw_event_send(ea);
		}
		return;
	}

	// STATUS_UPDATE: update disease and movement
	int idx = find_person_idx(s, pid);
	if (idx < 0)
		return;
	person_state p = s->people[idx];

	// Disease: death, recovery, immunity loss, new infection
	if (p.infected)
	{
		if (tw_rand_unif(lp->rng) < DEATH_RATE)
		{
			p.alive = false;
		}
		else if (tw_now(lp) - p.infected_time > INFECTION_TIME && tw_rand_unif(lp->rng) < RECOVERY_RATE)
		{
			p.infected = false;
			p.immune_start = tw_now(lp);
		}
	}
	else if (!p.susceptible && tw_now(lp) - p.immune_start > IMMUNITY_TIME)
	{
		p.susceptible = true;
	}
	else if (p.susceptible)
	{
		for (int j = 0; j < s->num_people; j++)
		{
			if (j == idx)
				continue;
			person_state *o = &s->people[j];
			if (o->alive && o->infected && tw_rand_unif(lp->rng) < TRANSMISSION_RATE)
			{
				p.infected = true;
				p.infected_time = tw_now(lp);
				p.susceptible = false;
				break;
			}
		}
	}
	s->people[idx] = p;

	// Movement or re-schedule
	tw_stime next_t = tw_now(lp) + 1.0;
	if (tw_rand_unif(lp->rng) < MOVE_PROBABILITY)
	{
		int dir = tw_rand_integer(lp->rng, 0, 3), dst;
		switch (dir)
		{
		case 0:
			dst = (p.y == GRID_HEIGHT - 1) ? p.x : lp->gid + GRID_WIDTH;
			break;
		case 1:
			dst = (p.y == 0) ? (GRID_HEIGHT - 1) * GRID_WIDTH + p.x : lp->gid - GRID_WIDTH;
			break;
		case 2:
			dst = (p.x == GRID_WIDTH - 1) ? lp->gid - (GRID_WIDTH - 1) : lp->gid + 1;
			break;
		default:
			dst = (p.x == 0) ? lp->gid + (GRID_WIDTH - 1) : lp->gid - 1;
			break;
		}
		tw_event *e = tw_event_new(lp->gid, next_t, lp);
		event_msg *mm = tw_event_data(e);
		mm->type = DEPARTURE;
		mm->person_id = pid;
		mm->dest_gid = dst;
		tw_event_send(e);
	}
	else
	{
		tw_event *e = tw_event_new(lp->gid, next_t, lp);
		event_msg *mm = tw_event_data(e);
		mm->type = STATUS_UPDATE;
		mm->person_id = pid;
		tw_event_send(e);
	}
}

void location_event_reverse(location_state *s, tw_bf *bf,
							event_msg *m, tw_lp *lp)
{
	switch (m->type)
	{
	case ARRIVAL:
		if (s->num_people > 0)
			s->num_people--;
		break;
	case DEPARTURE:
		s->num_people++;
		break;
	default:
		break;
	}
}

void location_final(location_state *s, tw_lp *lp)
{
	free(s->people);
}

void location_commit(location_state *s, tw_bf *bf,
					 event_msg *m, tw_lp *lp)
{
	/* no-op */
}

static tw_lptype my_lps[] = {
	{.init = (init_f)location_init,
	 .pre_run = NULL,
	 .event = (event_f)location_event,
	 .revent = (revent_f)location_event_reverse,
	 .commit = (commit_f)location_commit,
	 .final = (final_f)location_final,
	 .map = NULL,
	 .state_sz = sizeof(location_state)},
	{0}};

tw_lpid lp_typemap(tw_lpid gid)
{
	return 0; // You only have one LP type
}

int main(int argc, char **argv, char **env)
{
	tw_opt_add(NULL);
	tw_init(&argc, &argv);

	int total_lps = GRID_WIDTH * GRID_HEIGHT;
	tw_define_lps(total_lps / NUM_PROCESSES, sizeof(event_msg));

	g_tw_lp_types = my_lps;
	g_tw_lp_typemap = lp_typemap;
	tw_lp_setup_types();

	tw_run();
	tw_end();
	return 0;
}
