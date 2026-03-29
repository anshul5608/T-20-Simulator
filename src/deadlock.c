/* deadlock.c — Run-Out Deadlock Detection
 *
 * Scenario:
 *   Batsman A (striker) at End-2, running to End-1.
 *   Batsman B (nonstriker) at End-1, running to End-2.
 *   → Hold-and-Wait + Circular-Wait → Deadlock
 *
 * Detection: Resource Allocation Graph cycle check.
 * Resolution: Umpire (kernel) kills the slower batsman (striker).
 */
#include "../include/cricket.h"

bool run_attempt(int bat_id, int from_end, int to_end)
{
    pthread_mutex_lock(&M.runout_mtx);

    int *dest = (to_end == 1) ? &M.end1_holder : &M.end2_holder;
    int *src  = (from_end == 1) ? &M.end1_holder : &M.end2_holder;

    if (*dest == -1) {
        /* Safe — destination free */
        *dest = bat_id;
        *src  = -1;
        pthread_mutex_unlock(&M.runout_mtx);
        return true;
    }

    /* Destination occupied — check for cycle */
    int other = *dest;
    int *other_src = (to_end == 1) ? &M.end2_holder : &M.end1_holder;

    if (*other_src == bat_id) {
        /* ── CYCLE DETECTED ── */
        mlog("\n[RAG] Deadlock! %s holds End-%d wants End-%d\n",
             M.bat[bat_id].name, from_end, to_end);
        mlog("[RAG]           %s holds End-%d wants End-%d\n",
             M.bat[other].name, to_end, from_end);
        mlog("[RAG] Cycle confirmed → Umpire kills %s (RUN OUT)\n",
             M.bat[M.striker].name);

        /* Kill striker */
        pthread_mutex_lock(&M.score_mtx);
        M.bat[M.striker].is_out    = true;
        M.bat[M.striker].at_crease = false;
        M.wickets++;
        pthread_mutex_unlock(&M.score_mtx);

        sem_post(&M.crease_sem);

        if (M.wickets < MAX_WICKETS && M.next_bat < NUM_BATSMEN) {
            int nb = sjf_next_batsman();
            M.bat[nb].at_crease  = true;
            M.bat[nb].entry_ball = M.total_balls;
            M.striker            = nb;
            M.next_bat++;
        }
        if (M.wickets >= MAX_WICKETS) M.innings_over = true;

        /* Reset ends */
        M.end1_holder = M.bat[M.nonstriker].id;
        M.end2_holder = -1;

        pthread_mutex_unlock(&M.runout_mtx);
        return false;
    }

    /* No cycle — safe run */
    *dest = bat_id;
    *src  = -1;
    pthread_mutex_unlock(&M.runout_mtx);
    return true;
}
