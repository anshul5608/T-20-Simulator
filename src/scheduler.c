/* scheduler.c
 *
 * All three scheduling algorithms required by the assignment:
 *
 * A) Round-Robin (RR)
 *    - Manages "Over" transitions for Bowlers
 *    - Time Quantum = 6 balls (one over)
 *    - After 6 legal balls → context switch to next bowler
 *    - Bowler's CPU state (stats) saved; next bowler loaded
 *
 * B) Shortest Job First (SJF)
 *    - Manages incoming Batsman order when a wicket falls
 *    - Tail-enders (short expected stay) are promoted ahead
 *      of middle-order to minimise total match time
 *    - Burst estimate = expected_balls (pre-assigned per position)
 *
 * C) Priority Scheduling
 *    - Manages which Bowler gets the last 6 balls (death over)
 *    - When Match_Intensity is HIGH (over >= DEATH_OVER_START)
 *      the Death Over Specialist thread gets highest CPU priority
 *    - Overrides RR selection for those overs
 */

#include "../include/cricket.h"

/* ═══════════════════════════════════════════════════════
 * A) ROUND-ROBIN — next bowler in circular order
 *    Rule: no bowler bowls more than 4 overs (T20 rule)
 * ═══════════════════════════════════════════════════════ */
int rr_next_bowler(void)
{
    int idx = (M.cur_bowler + 1) % NUM_BOWLERS;
    for (int i = 0; i < NUM_BOWLERS; i++) {
        if (M.bowl[idx].overs_done < 4)
            return idx;
        idx = (idx + 1) % NUM_BOWLERS;
    }
    /* All hit quota — return least-used */
    int best = 0, least = 999;
    for (int i = 0; i < NUM_BOWLERS; i++) {
        if (M.bowl[i].overs_done < least) {
            least = M.bowl[i].overs_done;
            best  = i;
        }
    }
    return best;
}

/* ═══════════════════════════════════════════════════════
 * B) SJF — next batsman with shortest expected stay
 *    Called when a wicket falls; scans from next_bat onward
 *    Swaps the shortest-burst batsman into the next slot
 * ═══════════════════════════════════════════════════════ */
int sjf_next_batsman(void)
{
    int    best     = M.next_bat;
    double shortest = 1e9;

    for (int i = M.next_bat; i < NUM_BATSMEN; i++) {
        if (!M.bat[i].is_out && !M.bat[i].at_crease) {
            if (M.bat[i].expected_balls < shortest) {
                shortest = M.bat[i].expected_balls;
                best     = i;
            }
        }
    }

    /* Swap best into next_bat slot */
    if (best != M.next_bat) {
        Batsman tmp        = M.bat[M.next_bat];
        M.bat[M.next_bat]  = M.bat[best];
        M.bat[best]        = tmp;
        /* fix ids after swap */
        M.bat[M.next_bat].id = M.next_bat;
        M.bat[best].id       = best;
        mlog("[SJF] Promoted %s (exp %.0f balls) ahead of batting order\n",
             M.bat[M.next_bat].name, M.bat[M.next_bat].expected_balls);
    }
    return M.next_bat;
}

/* ═══════════════════════════════════════════════════════
 * C) PRIORITY — pick death specialist when high intensity
 *    Falls back to RR if no specialist available
 * ═══════════════════════════════════════════════════════ */
int priority_next_bowler(void)
{
    if (M.high_intensity) {
        int best_pri = -1, best_idx = -1;
        for (int i = 0; i < NUM_BOWLERS; i++) {
            if (M.bowl[i].overs_done >= 4)   continue;
            if (!M.bowl[i].death_specialist)  continue;
            if (M.bowl[i].priority > best_pri) {
                best_pri = M.bowl[i].priority;
                best_idx = i;
            }
        }
        if (best_idx != -1) {
            mlog("[PRIORITY] High intensity! Death specialist %s given CPU priority\n",
                 M.bowl[best_idx].name);
            return best_idx;
        }
    }
    return rr_next_bowler();
}

/* ═══════════════════════════════════════════════════════
 * Context Switch (RR / Priority)
 *   Save current bowler's stats → Load next bowler
 * ═══════════════════════════════════════════════════════ */
void context_switch(int next_idx)
{
    Bowler *cur  = &M.bowl[M.cur_bowler];
    Bowler *next = &M.bowl[next_idx];

    mlog("\n[CONTEXT SWITCH] Saving  : %s | %d overs | %d runs | %d wkts\n",
         cur->name, cur->overs_done, cur->runs_given, cur->wickets);
    mlog("[CONTEXT SWITCH] Loading : %s | %d overs bowled so far\n",
         next->name, next->overs_done);

    M.cur_bowler = next_idx;
    next->balls_this_over = 0;
}
