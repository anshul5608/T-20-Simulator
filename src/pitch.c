#include "../include/cricket.h"

/* Weighted random ball outcome (approximate T20 probabilities) */
Outcome roll_delivery(void)
{
    int r = rand() % 100;
    if      (r < 28) return OUT_DOT;
    else if (r < 52) return OUT_1;
    else if (r < 62) return OUT_2;
    else if (r < 65) return OUT_3;
    else if (r < 77) return OUT_FOUR;
    else if (r < 85) return OUT_SIX;
    else if (r < 90) return OUT_WIDE;
    else if (r < 93) return OUT_NOBALL;
    else if (r < 99) return OUT_WICKET;
    else             return OUT_RUNOUT;
}

int runs_of(Outcome o)
{
    switch(o){
        case OUT_1:      return 1;
        case OUT_2:      return 2;
        case OUT_3:      return 3;
        case OUT_FOUR:   return 4;
        case OUT_SIX:    return 6;
        case OUT_WIDE:   return 1;
        case OUT_NOBALL: return 1;
        default:         return 0;
    }
}

/* apply_outcome — called by batsman thread while holding pitch_mtx */
void apply_outcome(Outcome o)
{
    int r = runs_of(o);

    /* ── Atomic score update (mutex) ── */
    pthread_mutex_lock(&M.score_mtx);
    M.total_runs += r;
    pthread_mutex_unlock(&M.score_mtx);

    M.bowl[M.cur_bowler].runs_given += r;
    M.bat[M.striker].runs           += r;

    bool legal = (o != OUT_WIDE && o != OUT_NOBALL);
    if (legal) {
        M.bat[M.striker].balls_faced++;
        M.bowl[M.cur_bowler].balls_this_over++;
        M.ball_in_over++;
        M.total_balls++;
    }

    bool rotate = (r % 2 == 1);

    /* ── Wicket ── */
    if (o == OUT_WICKET) {
        mlog("  *** WICKET! %s OUT (bowled/caught off %s) ***\n",
             M.bat[M.striker].name, M.bowl[M.cur_bowler].name);

        M.bowl[M.cur_bowler].wickets++;
        M.bat[M.striker].is_out    = true;
        M.bat[M.striker].at_crease = false;
        M.wickets++;
        sem_post(&M.crease_sem);

        if (M.wickets < MAX_WICKETS && M.next_bat < NUM_BATSMEN) {
            /* ── SJF: choose incoming batsman ── */
            int nb = sjf_next_batsman();
            M.bat[nb].at_crease  = true;
            M.bat[nb].entry_ball = M.total_balls;
            M.striker            = nb;
            M.next_bat++;
            mlog("  [SJF] %s walks in (exp %.0f balls)\n",
                 M.bat[nb].name, M.bat[nb].expected_balls);
        }
        rotate = false;
    }

    /* ── Run-out handled in deadlock.c ── */
    if (o == OUT_RUNOUT) rotate = false;

    /* ── Rotate strike for odd runs ── */
    if (rotate) {
        int tmp      = M.striker;
        M.striker    = M.nonstriker;
        M.nonstriker = tmp;
    }

    if (M.wickets >= MAX_WICKETS) M.innings_over = true;

    /* ── Over boundary ──
     *   ball_in_over is incremented above only for legal deliveries.
     *   Check happens here so batsman thread triggers the over logic.
     */
    if (M.ball_in_over >= BALLS_PER_OVER) {
        M.over++;
        M.ball_in_over = 0;
        M.bowl[M.cur_bowler].overs_done++;
        M.bowl[M.cur_bowler].balls_this_over = 0;

        /* Rotate strike at end of over */
        int tmp      = M.striker;
        M.striker    = M.nonstriker;
        M.nonstriker = tmp;

        if (M.over >= TOTAL_OVERS) {
            M.innings_over = true;
            mlog("\n=== 20 OVERS COMPLETED ===\n");
        } else {
            /* Death-over check */
            if (M.over >= DEATH_OVER_START)
                M.high_intensity = true;

            /* ── Select next bowler ──
             *   Priority when high intensity, else RR
             */
            int next_b;
            if (M.high_intensity)
                next_b = priority_next_bowler();
            else
                next_b = rr_next_bowler();

            context_switch(next_b);
        }
    }
}
