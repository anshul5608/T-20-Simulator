#include "../include/cricket.h"

/* ── Player rosters ─────────────────────────────────── */
static const char *BAT_NAMES[11] = {
    "Rohit Sharma",      /* 0  opener   exp_balls=42 */
    "Virat Kohli",       /* 1  opener   exp_balls=38 */
    "Shubman Gill",      /* 2  no.3     exp_balls=30 */
    "Suryakumar Yadav",  /* 3  no.4     exp_balls=24 */
    "Hardik Pandya",     /* 4  no.5     exp_balls=18 */
    "Rinku Singh",       /* 5  no.6     exp_balls=14 */
    "Dinesh Karthik",    /* 6  no.7     exp_balls=10 */
    "Axar Patel",        /* 7  no.8     exp_balls=7  */
    "Kuldeep Yadav",     /* 8  no.9     exp_balls=5  */
    "Jasprit Bumrah",    /* 9  no.10    exp_balls=3  */
    "Arshdeep Singh"     /* 10 no.11    exp_balls=2  */
};
static const double EXP_BALLS[11] = {42,38,30,24,18,14,10,7,5,3,2};

static const char *BOWL_NAMES[6] = {
    "Jasprit Bumrah",    /* death specialist */
    "Mohammed Siraj",
    "Hardik Pandya",
    "Axar Patel",
    "Kuldeep Yadav",
    "Arshdeep Singh"     /* death specialist */
};
static const bool DEATH_SPEC[6] = {true,false,false,false,false,true};

static const char *FIELD_NAMES[10] = {
    "DK (WK)", "Gill (Slip)", "Kohli (Gully)", "Surya (Point)",
    "Rohit (Cover)", "Pandya (Mid-off)", "Rinku (Long-on)",
    "Axar (Long-off)", "Siraj (Fine-leg)", "Bumrah (3rd-man)"
};

/* ── match_init ─────────────────────────────────────── */
void match_init(void)
{
    memset(&M, 0, sizeof(M));
    srand((unsigned)time(NULL));

    M.striker    = 0;
    M.nonstriker = 1;
    M.next_bat   = 2;
    M.cur_bowler = 0;
    M.end1_holder = 1;   /* non-striker at end 1 */
    M.end2_holder = 0;   /* striker at end 2     */

    /* Batsmen */
    for (int i = 0; i < NUM_BATSMEN; i++) {
        M.bat[i].id             = i;
        strncpy(M.bat[i].name, BAT_NAMES[i], 63);
        M.bat[i].order          = i;
        M.bat[i].expected_balls = EXP_BALLS[i];
        M.bat[i].at_crease      = (i < 2);
    }

    /* Bowlers */
    for (int i = 0; i < NUM_BOWLERS; i++) {
        M.bowl[i].id              = i;
        strncpy(M.bowl[i].name, BOWL_NAMES[i], 63);
        M.bowl[i].death_specialist = DEATH_SPEC[i];
        M.bowl[i].priority        = DEATH_SPEC[i] ? 10 : 5;
    }

    /* Fielders */
    for (int i = 0; i < NUM_FIELDERS; i++) {
        M.field[i].id = i;
        strncpy(M.field[i].name, FIELD_NAMES[i], 63);
    }

    /* Sync primitives */
    pthread_mutex_init(&M.score_mtx,  NULL);
    pthread_mutex_init(&M.pitch_mtx,  NULL);
    pthread_mutex_init(&M.log_mtx,    NULL);
    pthread_mutex_init(&M.runout_mtx, NULL);
    sem_init(&M.crease_sem, 0, CREASE_CAPACITY);
    pthread_cond_init(&M.cv_delivery, NULL);
    pthread_cond_init(&M.cv_stroke,   NULL);
    pthread_cond_init(&M.cv_air,      NULL);

    /* Log files */
    M.log_fp = fopen("logs/match_log.txt", "w");
    M.csv_fp = fopen("logs/gantt_data.csv", "w");
    if (!M.log_fp || !M.csv_fp) { perror("log open"); exit(1); }

    fprintf(M.csv_fp,
        "Ball,Over,BallInOver,Bowler,Striker,NonStriker,"
        "Outcome,Runs,TotalRuns,Wickets\n");
}

/* ── match_cleanup ──────────────────────────────────── */
void match_cleanup(void)
{
    pthread_mutex_destroy(&M.score_mtx);
    pthread_mutex_destroy(&M.pitch_mtx);
    pthread_mutex_destroy(&M.log_mtx);
    pthread_mutex_destroy(&M.runout_mtx);
    sem_destroy(&M.crease_sem);
    pthread_cond_destroy(&M.cv_delivery);
    pthread_cond_destroy(&M.cv_stroke);
    pthread_cond_destroy(&M.cv_air);
    if (M.log_fp) fclose(M.log_fp);
    if (M.csv_fp) fclose(M.csv_fp);
}

/* ── innings2_init ───────────────────────────────────
 * Re-uses same sync primitives (files stay open).
 * Team 2: Australia batting, India bowling.
 * ─────────────────────────────────────────────────── */
static const char *BAT2_NAMES[11] = {
    "David Warner",       /* 0  opener   exp_balls=40 */
    "Travis Head",        /* 1  opener   exp_balls=36 */
    "Mitchell Marsh",     /* 2  no.3     exp_balls=28 */
    "Steven Smith",       /* 3  no.4     exp_balls=22 */
    "Glenn Maxwell",      /* 4  no.5     exp_balls=20 */
    "Tim David",          /* 5  no.6     exp_balls=15 */
    "Matthew Wade",       /* 6  no.7     exp_balls=10 */
    "Pat Cummins",        /* 7  no.8     exp_balls=7  */
    "Mitchell Starc",     /* 8  no.9     exp_balls=5  */
    "Adam Zampa",         /* 9  no.10    exp_balls=3  */
    "Josh Hazlewood"      /* 10 no.11    exp_balls=2  */
};
static const double EXP2_BALLS[11] = {40,36,28,22,20,15,10,7,5,3,2};

static const char *BOWL2_NAMES[6] = {
    "Jasprit Bumrah",    /* death specialist */
    "Arshdeep Singh",    /* death specialist */
    "Hardik Pandya",
    "Axar Patel",
    "Kuldeep Yadav",
    "Mohammed Siraj"
};
static const bool DEATH2_SPEC[6] = {true,true,false,false,false,false};

static const char *FIELD2_NAMES[10] = {
    "Wade (WK)", "Head (Slip)", "Smith (Gully)", "Maxwell (Point)",
    "Warner (Cover)", "Marsh (Mid-off)", "T.David (Long-on)",
    "Cummins (Long-off)", "Starc (Fine-leg)", "Zampa (3rd-man)"
};

void innings2_init(void)
{
    /* Save innings 1 result */
    M.innings1_runs    = M.total_runs;
    M.innings1_wickets = M.wickets;

    /* Reset score & state (keep log/csv open, keep sync primitives) */
    M.total_runs   = 0;
    M.wickets      = 0;
    M.over         = 0;
    M.ball_in_over = 0;
    M.total_balls  = 0;
    M.innings_over = false;
    M.high_intensity = false;
    M.innings_num  = 2;

    M.striker    = 0;
    M.nonstriker = 1;
    M.next_bat   = 2;
    M.cur_bowler = 0;
    M.end1_holder = 1;
    M.end2_holder = 0;

    M.pitch.delivery_ready = false;
    M.pitch.stroke_done    = false;
    M.pitch.ball_in_air    = false;
    M.pitch.runs           = 0;

    /* Batsmen (Australia) */
    for (int i = 0; i < NUM_BATSMEN; i++) {
        M.bat[i].id             = i;
        strncpy(M.bat[i].name, BAT2_NAMES[i], 63);
        M.bat[i].order          = i;
        M.bat[i].expected_balls = EXP2_BALLS[i];
        M.bat[i].runs           = 0;
        M.bat[i].balls_faced    = 0;
        M.bat[i].is_out         = false;
        M.bat[i].at_crease      = (i < 2);
        M.bat[i].entry_ball     = 0;
        M.bat[i].wait_balls_fcfs= 0;
        M.bat[i].wait_balls_sjf = 0;
    }

    /* Bowlers (India) */
    for (int i = 0; i < NUM_BOWLERS; i++) {
        M.bowl[i].id               = i;
        strncpy(M.bowl[i].name, BOWL2_NAMES[i], 63);
        M.bowl[i].death_specialist = DEATH2_SPEC[i];
        M.bowl[i].priority         = DEATH2_SPEC[i] ? 10 : 5;
        M.bowl[i].overs_done       = 0;
        M.bowl[i].balls_this_over  = 0;
        M.bowl[i].runs_given       = 0;
        M.bowl[i].wickets          = 0;
    }

    /* Fielders (Australia) */
    for (int i = 0; i < NUM_FIELDERS; i++) {
        M.field[i].id = i;
        strncpy(M.field[i].name, FIELD2_NAMES[i], 63);
    }

    /* Re-init semaphore and conditions */
    sem_destroy(&M.crease_sem);
    pthread_cond_destroy(&M.cv_delivery);
    pthread_cond_destroy(&M.cv_stroke);
    pthread_cond_destroy(&M.cv_air);
    sem_init(&M.crease_sem, 0, CREASE_CAPACITY);
    pthread_cond_init(&M.cv_delivery, NULL);
    pthread_cond_init(&M.cv_stroke,   NULL);
    pthread_cond_init(&M.cv_air,      NULL);

    /* Write innings 2 CSV header section */
    fprintf(M.csv_fp, "\n# INNINGS 2\n");
    fprintf(M.csv_fp,
        "Ball,Over,BallInOver,Bowler,Striker,NonStriker,"
        "Outcome,Runs,TotalRuns,Wickets\n");
}
