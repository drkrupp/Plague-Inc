#include "plague1lp.h"

void location_init(location_state *s, tw_lp *lp)
{
    for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
    {
        s->people[i].x = lp->gid % GRID_WIDTH;
        s->people[i].y = lp->gid / GRID_WIDTH;
        s->people[i].alive = true;
        s->people[i].infected = false;
        s->people[i].immune = false;
        s->people[i].susceptible = true;

        double r = tw_rand_unif(lp->rng);
        if (r < INITIAL_INFECTED_RATE)
        {
            s->people[i].infected = true;
            s->people[i].infected_time = 0.0;
            s->people[i].susceptible = false;
        }
    }

    tw_event *e = tw_event_new(lp->gid, 1.0, lp);
    event_msg *m = tw_event_data(e);
    m->type = STATUS_UPDATE;
    m->person_index = -1; // applies to all people
    tw_event_send(e);
}

void location_event(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
    for (int i = 0; i < PEOPLE_PER_LOCATION; i++)
    {
        person_state *p = &s->people[i];
        if (!p->alive)
            continue;

        if (p->infected && (tw_now(lp) - p->infected_time > INFECTION_TIME))
        {
            if (tw_rand_unif(lp->rng) < RECOVERY_RATE)
            {
                p->infected = false;
                p->immune = true;
                p->immune_start = tw_now(lp);
            }
            else if (tw_rand_unif(lp->rng) < DEATH_RATE)
            {
                p->alive = false;
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
                    break;
                }
            }
        }

        if (p->immune && (tw_now(lp) - p->immune_start > IMMUNITY_TIME))
        {
            p->immune = false;
            p->susceptible = true;
        }
    }

    tw_event *e = tw_event_new(lp->gid, 1.0, lp);
    event_msg *next = tw_event_data(e);
    next->type = STATUS_UPDATE;
    next->person_index = -1;
    tw_event_send(e);
}

void location_event_reverse(location_state *s, tw_bf *bf, event_msg *m, tw_lp *lp)
{
    // Not implemented for simplicity
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
    printf("Location LP %lu final: Alive: %d, Dead: %d, Infected: %d\n",
           lp->gid, alive, dead, infected);
}

tw_lptype my_lps[] = {
    {.init = (init_f)location_init,
     .pre_run = NULL,
     .event = (event_f)location_event,
     .revent = (revent_f)location_event_reverse,
     .commit = NULL,
     .final = (final_f)location_final,
     .map = NULL,
     .state_sz = sizeof(location_state)},
    {0},
};

int main(int argc, char **argv, char **env)
{
    tw_opt_add(NULL);
    tw_init(&argc, &argv);

    int num_locations = GRID_WIDTH * GRID_HEIGHT;
    tw_define_lps(num_locations, sizeof(event_msg));

    tw_run();
    tw_end();
    return 0;
}
