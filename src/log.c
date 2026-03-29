#include "../include/cricket.h"

static const char *outcome_label(Outcome o)
{
    switch(o){
        case OUT_DOT:    return "DOT";
        case OUT_1:      return "1";
        case OUT_2:      return "2";
        case OUT_3:      return "3";
        case OUT_FOUR:   return "4";
        case OUT_SIX:    return "6";
        case OUT_WIDE:   return "WD";
        case OUT_NOBALL: return "NB";
        case OUT_WICKET: return "W";
        case OUT_RUNOUT: return "RO";
        default:         return "?";
    }
}

void mlog(const char *fmt, ...)
{
    pthread_mutex_lock(&M.log_mtx);
    va_list ap;
    va_start(ap, fmt);  vprintf(fmt, ap);          va_end(ap);
    va_start(ap, fmt);  vfprintf(M.log_fp, fmt, ap); va_end(ap);
    fflush(M.log_fp);
    pthread_mutex_unlock(&M.log_mtx);
}

void csv_log(int ball, int ov, int bin,
             const char *bowler, const char *striker,
             Outcome o, int runs)
{
    if (!M.csv_fp) return;
    pthread_mutex_lock(&M.log_mtx);
    fprintf(M.csv_fp, "%d,%d,%d,\"%s\",\"%s\",\"%s\",%s,%d,%d,%d\n",
            ball, ov, bin,
            bowler, striker,
            M.bat[M.nonstriker].name,
            outcome_label(o), runs,
            M.total_runs, M.wickets);
    fflush(M.csv_fp);
    pthread_mutex_unlock(&M.log_mtx);
}
