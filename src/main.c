/* main.c — T20 WC Cricket Simulator (CSC-204 Assignment-1)
 *
 * Single binary that demonstrates ALL THREE scheduling algorithms
 * in one match (TWO INNINGS):
 *   A) Round-Robin    → Bowler over transitions (quantum = 6)
 *   B) SJF            → Incoming batsman selection on wicket
 *   C) Priority       → Death over specialist gets CPU priority
 *
 * Thread count: 6 bowlers + 11 batsmen + 10 fielders = 27 threads
 */
#include "../include/cricket.h"

Match M;   /* global match state */

static int  bowl_args[NUM_BOWLERS];
static int  bat_args[NUM_BATSMEN];
static int  field_args[NUM_FIELDERS];

static void broadcast_end(void)
{
    pthread_mutex_lock(&M.pitch_mtx);
    M.innings_over = true;
    pthread_cond_broadcast(&M.cv_delivery);
    pthread_cond_broadcast(&M.cv_stroke);
    pthread_cond_broadcast(&M.cv_air);
    for (int i = 0; i < NUM_BATSMEN + 2; i++)
        sem_post(&M.crease_sem);
    pthread_mutex_unlock(&M.pitch_mtx);
}

static void run_innings(int inn_num)
{
    mlog("\n");
    mlog("╔══════════════════════════════════════════════════════════╗\n");
    mlog("║              INNINGS %d BEGINS                           ║\n", inn_num);
    mlog("╚══════════════════════════════════════════════════════════╝\n\n");

    mlog("=== INNINGS %d BEGINS ===\n", inn_num);
    mlog("  Striker     : %s\n", M.bat[M.striker].name);
    mlog("  Non-Striker : %s\n", M.bat[M.nonstriker].name);
    mlog("  Bowler (RR) : %s\n\n", M.bowl[M.cur_bowler].name);

    /* Spawn fielder threads (10) */
    for (int i = 0; i < NUM_FIELDERS; i++) {
        field_args[i] = i;
        pthread_create(&M.field[i].tid, NULL, fielder_fn, &field_args[i]);
    }

    /* Spawn bowler threads (6) */
    for (int i = 0; i < NUM_BOWLERS; i++) {
        bowl_args[i] = i;
        pthread_create(&M.bowl[i].tid, NULL, bowler_fn, &bowl_args[i]);
    }

    /* Spawn batsman threads (11) — stagger slightly for ordered init */
    for (int i = 0; i < NUM_BATSMEN; i++) {
        bat_args[i] = i;
        pthread_create(&M.bat[i].tid, NULL, batsman_fn, &bat_args[i]);
        usleep(300);
    }

    mlog("[MAIN] 27 threads spawned "
         "(6 bowlers + 11 batsmen + 10 fielders)\n\n");

    /* Monitor loop */
    while (!M.innings_over) {
        usleep(8000);
        if (M.total_balls >= TOTAL_OVERS * BALLS_PER_OVER ||
            M.wickets     >= MAX_WICKETS)
            M.innings_over = true;
    }

    broadcast_end();

    /* Join all threads */
    for (int i = 0; i < NUM_BOWLERS; i++)
        pthread_join(M.bowl[i].tid,  NULL);
    for (int i = 0; i < NUM_BATSMEN; i++)
        pthread_join(M.bat[i].tid,   NULL);
    for (int i = 0; i < NUM_FIELDERS; i++)
        pthread_join(M.field[i].tid, NULL);

    print_scorecard();
    print_analysis();
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║      T20 WORLD CUP 2026 — CRICKET SIMULATOR             ║\n");
    printf("║      CSC-204: Operating Systems — Assignment 1          ║\n");
    printf("║  Scheduling: RR (bowlers) + SJF (batsmen) + Priority    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    /* ── INNINGS 1 (India batting) ── */
    match_init();
    M.innings_num = 1;
    run_innings(1);

    /* ── INNINGS 2 (Australia batting) ── */
    innings2_init();
    run_innings(2);

    /* ── Match Result ── */
    mlog("\n╔══════════════════════════════════════════════════════════╗\n");
    mlog("║                    MATCH RESULT                         ║\n");
    mlog("╚══════════════════════════════════════════════════════════╝\n");
    mlog("  Innings 1 (India)     : %d/%d\n",
         M.innings1_runs, M.innings1_wickets);
    mlog("  Innings 2 (Australia) : %d/%d\n",
         M.total_runs, M.wickets);
    if (M.total_runs > M.innings1_runs)
        mlog("  Result: Australia won by %d wickets (chased %d)\n",
             MAX_WICKETS - M.wickets, M.innings1_runs + 1);
    else if (M.total_runs < M.innings1_runs)
        mlog("  Result: India won by %d runs\n",
             M.innings1_runs - M.total_runs);
    else
        mlog("  Result: Match TIED!\n");
    mlog("\n");

    write_chart_script();

    mlog("=== SIMULATION COMPLETE ===\n");
    mlog("  Log  : logs/match_log.txt\n");
    mlog("  Data : logs/gantt_data.csv\n");
    mlog("  Run  : python3 charts/make_charts.py\n\n");

    match_cleanup();
    return 0;
}

