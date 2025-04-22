
#include "plagueInc.h"

tw_peid mapping(tw_lpid gid)
{
	return (tw_peid)gid / g_tw_nlp;
}

void location_init(location_state *s, tw_lp *lp)
{
	tw_output(lp, "location_init: LP %lu | state ptr: %p | rng: %p\n", lp->gid, s, lp->rng);
	// printf("location_init: LP %lu | state ptr: %p | rng: %p\n", lp->gid, s, lp->rng);

	if (!lp->rng)
	{
		tw_error(TW_LOC, "location_init: RNG is NULL for LP %lu\n", lp->gid);
	}

	// Set LP grid coordinates
	s->x = lp->gid % GRID_WIDTH;
	s->y = lp->gid / GRID_WIDTH;
	s->max_people_held = MAX_PEOPLE_PER_LOCATION;
	printf("LP %lu mapped to grid (%d, %d)\n", lp->gid, s->x, s->y);

	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		printf("  Init person[%d] for LP %lu\n", i, lp->gid);
		s->people[i].x = s->x;
		s->people[i].y = s->y;
		s->people[i].alive = true;
		s->people[i].infected = false;
		s->people[i].immune = false;
		s->people[i].susceptible = true;

		double r = tw_rand_unif(lp->rng);
		printf("    person[%d] random init val: %f\n", i, r);

		if (r < INITIAL_INFECTED_RATE)
		{
			s->people[i].infected = true;
			s->people[i].infected_time = 0.0;
			s->people[i].susceptible = false;
			printf("    person[%d] initialized as infected\n", i);
		}
	}

	tw_event *e = tw_event_new(lp->gid, 1.0, lp);
	if (!e)
	{
		tw_error(TW_LOC, "location_init: Failed to allocate event for LP %lu\n", lp->gid);
	}


	for(i = 0; i < planes_per_airport; i++)
    {
      e = tw_event_new(lp->gid, tw_rand_exponential(lp->rng, 1.0), lp);
      m = tw_event_data(e);
      m->type = STATUS_UPDATE;
	  m->person_index = i;
      tw_event_send(e);
    }
}

void location_event(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	int person_index;
	person_state arriving_state;
	switch(msg->type){
		case ARRIVAL:
		{
			arriving_state = m->arriving_state;
			if (s->num_people == s->max_people_held)
			{
				s->max_people_held *= 2;
				s->people = (tw_lpid *)realloc(s->people, sizeof(tw_lpid) * s->max_people_held);
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
			return; //don't handle status updates for people who have left the location
		}
		case default:
		{
			person_index = m->person_index;
			break;
		}
	}

	person_state p = s->people[person_state];

	if (!p->alive)
		continue;

	if (p->infected){
		if (tw_rand_unif(lp->rng) < DEATH_RATE)
		{
			p->alive = false;
			printf("    person[%d] died\n", i);
			return;
		}
	}

	if (p->infected && (tw_now(lp) - p->infected_time > INFECTION_TIME))
	{
		double r = tw_rand_unif(lp->rng);
		printf("  person[%d] infected for long enough, rand: %f\n", i, r);
		// ADD DEATH FOR PEOPLE THAT JUST GOT SICK
		if (r < RECOVERY_RATE)
		{
			p->infected = false;
			p->immune = true;
			p->immune_start = tw_now(lp);
			printf("    person[%d] recovered\n", i);
		}
	}
	else if (p->susceptible)
	{
		for (int j = 0; j < PEOPLE_PER_LOCATION; j++)
		{
			if (i == j || !s->people[j].infected || !s->people[j].alive)
				continue;

			if (tw_rand_unif(lp->rng) < TRANSMISSION_RATE)
			{
				p->susceptible = false;
				p->infected = true;
				p->infected_time = tw_now(lp);
				printf("    person[%d] got infected by person[%d]\n", i, j);
				break;
			}
		}
	}

	if (p->immune && (tw_now(lp) - p->immune_start > IMMUNITY_TIME))
	{
		p->immune = false;
		p->susceptible = true;
		printf("    person[%d] lost immunity\n", i);
	}

	if(tw_rand_unif(lp->rng) < MOVE_PROBABILITY){
		//move
	}
	else{
		//status update to self
	}





	printf("location_event: LP %lu at time %lf | msg type: %d\n", lp->gid, tw_now(lp), m->type);

	for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
	{
		
		person_state *p = &s->people[i];

		if (!p->alive)
			continue;

		if (p->infected && (tw_now(lp) - p->infected_time > INFECTION_TIME))
		{
			double r = tw_rand_unif(lp->rng);
			printf("  person[%d] infected for long enough, rand: %f\n", i, r);
			// ADD DEATH FOR PEOPLE THAT JUST GOT SICK
			if (r < RECOVERY_RATE)
			{
				p->infected = false;
				p->immune = true;
				p->immune_start = tw_now(lp);
				printf("    person[%d] recovered\n", i);
			}
			else if (tw_rand_unif(lp->rng) < DEATH_RATE)
			{
				p->alive = false;
				printf("    person[%d] died\n", i);
			}
		}
		else if (p->susceptible)
		{
			for (int j = 0; j < PEOPLE_PER_LOCATION; j++)
			{
				if (i == j || !s->people[j].infected || !s->people[j].alive)
					continue;

				if (tw_rand_unif(lp->rng) < TRANSMISSION_RATE)
				{
					p->susceptible = false;
					p->infected = true;
					p->infected_time = tw_now(lp);
					printf("    person[%d] got infected by person[%d]\n", i, j);
					break;
				}
			}
		}

		if (p->immune && (tw_now(lp) - p->immune_start > IMMUNITY_TIME))
		{
			p->immune = false;
			p->susceptible = true;
			printf("    person[%d] lost immunity\n", i);
		}
	}

	tw_event *e = tw_event_new(lp->gid, 1.0, lp);
	if (!e)
	{
		tw_error(TW_LOC, "location_event: Failed to allocate next event for LP %lu\n", lp->gid);
	}

	event_msg *next = tw_event_data(e);
	next->type = STATUS_UPDATE;
	next->person_index = -1;
	printf("LP %lu rescheduling STATUS_UPDATE event for next cycle\n", lp->gid);
	tw_event_send(e);
}

void location_event_reverse(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
	// Not implemented
	printf("location_event_reverse: LP %lu (unimplemented)\n", lp->gid);
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

	printf("FINAL: LP %lu | Alive: %d | Dead: %d | Infected: %d\n", lp->gid, alive, dead, infected);
}

// tw_lpid map_location(tw_lpid gid)
// {
// 	printf("map_location: mapping gid %lu to itself\n", gid);
// 	return gid;
// }

tw_lptype my_lps[] =
	{
		{
			(init_f)location_init,
			(pre_run_f)NULL,
			(event_f)location_event,
			(revent_f)location_event_reverse,
			(commit_f)NULL,
			(final_f)location_final,
			(map_f)mapping,
			sizeof(location_state),
		},
		{0},
};

int main(int argc, char **argv, char **env)
{
	int i;
	printf("main: Starting ROSS plague simulation\n");
	g_tw_lookahead = 1.0;
	g_tw_events_per_pe = 8192;
	tw_opt_add(NULL);
	tw_init(&argc, &argv);

	int NUM_LOCATIONS = GRID_WIDTH * GRID_HEIGHT;
	// nlp_per_pe = NUM_LOCATIONS / (tw_nnodes() * g_tw_npe);
	long nlp_per_pe = NUM_LOCATIONS / 2;

	tw_define_lps(nlp_per_pe, sizeof(event_msg));
	printf("main: LP types registered\n");
	for (i = 0; i < g_tw_nlp; i++)
		tw_lp_settype(i, &my_lps[0]);

	printf("main: Running simulation...\n");
	tw_run();

	if (tw_ismaster())
	{
		printf("\nAModel Statistics:\n");
		printf("\t%-50s %11lld\n", "Number of locations",
			   nlp_per_pe * 2 * tw_nnodes());
		printf("\t%-50s %11lld\n", "Number of people",
			   PEOPLE_PER_LOCATION * nlp_per_pe * 2 * tw_nnodes());
	}

	tw_end();

	return 0;
}