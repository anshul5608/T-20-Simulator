#ifndef CRICKET_H
#define CRICKET_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

/* ── Match constants ─────────────────────────────────── */
#define TOTAL_OVERS        20
#define BALLS_PER_OVER      6
#define MAX_WICKETS        10
#define NUM_BATSMEN        11
#define NUM_BOWLERS         6
#define NUM_FIELDERS       10   /* includes wicket-keeper */
#define CREASE_CAPACITY     2   /* semaphore value        */
#define DEATH_OVER_START   14   /* overs 15-20 = death    */

/* ── Ball outcomes ───────────────────────────────────── */
typedef enum {
    OUT_DOT = 0,
    OUT_1,
    OUT_2,
    OUT_3,
    OUT_FOUR,
    OUT_SIX,
    OUT_WIDE,
    OUT_NOBALL,
    OUT_WICKET,
    OUT_RUNOUT
} Outcome;

/* ── Batsman ─────────────────────────────────────────── */
typedef struct {
    int    id;
    char   name[64];
    int    runs;
    int    balls_faced;
    bool   is_out;
    bool   at_crease;
    int    order;                 /* 0=opener … 10=last     */
    double expected_balls;        /* SJF burst estimate     */
    int    entry_ball;            /* total ball when entered */
    int    wait_balls_fcfs;       /* wait under FCFS        */
    int    wait_balls_sjf;        /* wait under SJF         */
    pthread_t tid;
} Batsman;

/* ── Bowler ──────────────────────────────────────────── */
typedef struct {
    int   id;
    char  name[64];
    int   overs_done;
    int   balls_this_over;
    int   runs_given;
    int   wickets;
    bool  death_specialist;       /* Priority scheduling    */
    int   priority;               /* higher = more urgent   */
    pthread_t tid;
} Bowler;

/* ── Fielder ─────────────────────────────────────────── */
typedef struct {
    int  id;
    char name[64];
    pthread_t tid;
} Fielder;

/* ── Shared Pitch (Critical Section) ────────────────── */
typedef struct {
    Outcome outcome;
    int     runs;
    bool    ball_in_air;     /* fielder wake condition    */
    bool    stroke_done;     /* bowler wait condition     */
    bool    delivery_ready;  /* batsman wait condition    */
} Pitch;

/* ── Match-wide state ────────────────────────────────── */
typedef struct {
    /* Score */
    int   total_runs;
    int   wickets;
    int   over;              /* completed overs           */
    int   ball_in_over;      /* 0-5 within current over   */
    int   total_balls;       /* all legal deliveries      */
    bool  innings_over;

    /* Innings tracking */
    int   innings_num;       /* 1 or 2                    */
    int   innings1_runs;     /* result of innings 1       */
    int   innings1_wickets;

    /* Active indices */
    int   striker;
    int   nonstriker;
    int   next_bat;          /* next batsman to come in   */
    int   cur_bowler;

    /* Scheduling state */
    bool  high_intensity;    /* TRUE when over >= DEATH_OVER_START */

    /* Deadlock / run-out */
    int   end1_holder;       /* batsman id, -1 if free    */
    int   end2_holder;

    /* Players */
    Batsman  bat[NUM_BATSMEN];
    Bowler   bowl[NUM_BOWLERS];
    Fielder  field[NUM_FIELDERS];

    Pitch pitch;

    /* Synchronisation */
    pthread_mutex_t score_mtx;   /* protects total_runs, wickets */
    pthread_mutex_t pitch_mtx;   /* Critical Section lock        */
    pthread_mutex_t log_mtx;
    pthread_mutex_t runout_mtx;
    sem_t           crease_sem;  /* capacity = 2                 */
    pthread_cond_t  cv_delivery; /* batsman waits for ball       */
    pthread_cond_t  cv_stroke;   /* bowler waits for stroke done */
    pthread_cond_t  cv_air;      /* fielders wait for ball_in_air*/

    /* Logs */
    FILE *log_fp;
    FILE *csv_fp;

    /* Gantt data: per-ball record */
    /* stored in csv_fp            */
} Match;

extern Match M;

/* ── Function declarations ───────────────────────────── */

/* init.c */
void match_init(void);
void innings2_init(void);
void match_cleanup(void);

/* log.c */
void mlog(const char *fmt, ...);
void csv_log(int ball, int over, int bin,
             const char *bowler, const char *striker,
             Outcome o, int runs);

/* scheduler.c  — ALL THREE algorithms live here */
int  rr_next_bowler(void);          /* Round-Robin               */
int  sjf_next_batsman(void);        /* SJF on tail-ender burst   */
int  priority_next_bowler(void);    /* Priority (death specialist)*/
void context_switch(int next_idx);  /* save old, load new bowler */

/* pitch.c */
Outcome roll_delivery(void);
int     runs_of(Outcome o);
void    apply_outcome(Outcome o);

/* threads.c */
void *bowler_fn(void *arg);
void *batsman_fn(void *arg);
void *fielder_fn(void *arg);

/* deadlock.c */
bool run_attempt(int bat_id, int from_end, int to_end);

/* analysis.c */
void print_scorecard(void);
void print_analysis(void);
void write_chart_script(void);

#endif
