#include "../include/cricket.h"

/* ════════════════════════════════════════════════════════
 * BOWLER THREAD
 *   Owns pitch_mtx for one delivery.
 *   Delivers, then signals batsman (cv_delivery).
 *   Waits for batsman to complete stroke (cv_stroke).
 *   Over rotation handled by apply_outcome() via context_switch().
 * ════════════════════════════════════════════════════════ */
void *bowler_fn(void *arg)
{
    int idx = *(int *)arg;

    while (!M.innings_over) {
        pthread_mutex_lock(&M.pitch_mtx);

        /* Only act when I am the scheduled bowler */
        if (M.cur_bowler != idx || M.innings_over) {
            pthread_mutex_unlock(&M.pitch_mtx);
            usleep(500);
            continue;
        }

        /* ── Roll the ball ── */
        Outcome o = roll_delivery();
        M.pitch.outcome       = o;
        M.pitch.runs          = runs_of(o);
        M.pitch.stroke_done   = false;
        M.pitch.delivery_ready = true;
        M.pitch.ball_in_air   = true;

        /* Wake batsman + fielders */
        pthread_cond_broadcast(&M.cv_delivery);
        pthread_cond_broadcast(&M.cv_air);

        /* Wait for stroke completion */
        while (!M.pitch.stroke_done && !M.innings_over)
            pthread_cond_wait(&M.cv_stroke, &M.pitch_mtx);

        M.pitch.ball_in_air    = false;
        M.pitch.delivery_ready = false;

        pthread_mutex_unlock(&M.pitch_mtx);
        usleep(2000);   /* 2 ms gap between balls */
    }
    return NULL;
}

/* ════════════════════════════════════════════════════════
 * BATSMAN THREAD
 *   Waits on crease semaphore (max 2 at crease).
 *   Waits for delivery_ready signal.
 *   Plays stroke, calls apply_outcome(), signals bowler.
 * ════════════════════════════════════════════════════════ */
void *batsman_fn(void *arg)
{
    int idx = *(int *)arg;

    /* Acquire crease slot — 3rd batsman blocks here */
    sem_wait(&M.crease_sem);
    if (M.innings_over) { sem_post(&M.crease_sem); return NULL; }

    mlog("[CREASE] %s entered crease (sem acquired)\n", M.bat[idx].name);

    while (!M.innings_over && !M.bat[idx].is_out) {
        pthread_mutex_lock(&M.pitch_mtx);

        /* Non-striker: just hold crease */
        if (M.striker != idx) {
            pthread_mutex_unlock(&M.pitch_mtx);
            usleep(1000);
            continue;
        }

        /* Wait for bowler to deliver */
        while (!M.pitch.delivery_ready && !M.innings_over && !M.bat[idx].is_out)
            pthread_cond_wait(&M.cv_delivery, &M.pitch_mtx);

        if (M.innings_over || M.bat[idx].is_out) {
            pthread_mutex_unlock(&M.pitch_mtx);
            break;
        }

        Outcome o = M.pitch.outcome;
        int     r = M.pitch.runs;

        /* ── Run-out: deadlock detection ── */
        if (o == OUT_RUNOUT) {
            bool safe = run_attempt(idx, 2, 1);
            if (!safe) {
                M.pitch.stroke_done = true;
                pthread_cond_signal(&M.cv_stroke);
                pthread_mutex_unlock(&M.pitch_mtx);
                break;
            }
        }

        /* ── Apply outcome (score, wicket, over logic) ── */
        apply_outcome(o);

        /* ── Ball-by-ball log ── */
        mlog("  Over %2d.%d | %-20s → %-20s | %3s | %d/%d\n",
             M.over,
             M.ball_in_over,
             M.bowl[M.cur_bowler].name,
             M.bat[M.striker == idx ? idx : M.striker].name,
             o == OUT_DOT    ? "DOT" :
             o == OUT_1      ? "1"   :
             o == OUT_2      ? "2"   :
             o == OUT_3      ? "3"   :
             o == OUT_FOUR   ? "FOUR":
             o == OUT_SIX    ? "SIX" :
             o == OUT_WIDE   ? "WD"  :
             o == OUT_NOBALL ? "NB"  :
             o == OUT_WICKET ? "OUT" : "RO",
             M.total_runs, M.wickets);

        csv_log(M.total_balls, M.over + 1, M.ball_in_over,
                M.bowl[M.cur_bowler].name,
                M.bat[idx].name, o, r);

        /* Signal bowler */
        M.pitch.stroke_done = true;
        pthread_cond_signal(&M.cv_stroke);
        pthread_mutex_unlock(&M.pitch_mtx);

        usleep(1500);
    }

    M.bat[idx].at_crease = false;
    sem_post(&M.crease_sem);
    return NULL;
}

/* ════════════════════════════════════════════════════════
 * FIELDER THREAD
 *   Sleeps on pthread_cond_wait(cv_air).
 *   Wakes ONLY when ball_in_air == true (batsman hit signal).
 * ════════════════════════════════════════════════════════ */
void *fielder_fn(void *arg)
{
    int idx = *(int *)arg;

    while (!M.innings_over) {
        pthread_mutex_lock(&M.pitch_mtx);
        while (!M.pitch.ball_in_air && !M.innings_over)
            pthread_cond_wait(&M.cv_air, &M.pitch_mtx);

        if (M.innings_over) { pthread_mutex_unlock(&M.pitch_mtx); break; }

        Outcome o = M.pitch.outcome;
        if (o == OUT_FOUR)
            mlog("    [F%d %-16s] Dives at the rope — boundary!\n",
                 idx, M.field[idx].name);
        else if (o == OUT_SIX)
            mlog("    [F%d %-16s] Ball sails over the rope — SIX!\n",
                 idx, M.field[idx].name);
        else if (o == OUT_WICKET)
            mlog("    [F%d %-16s] Takes the catch / effects stumping!\n",
                 idx, M.field[idx].name);
        else if (o == OUT_RUNOUT)
            mlog("    [F%d %-16s] Direct hit! Run out attempt!\n",
                 idx, M.field[idx].name);

        pthread_mutex_unlock(&M.pitch_mtx);
        usleep(800);
    }
    return NULL;
}
