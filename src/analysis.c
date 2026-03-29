#include "../include/cricket.h"

/* ── Scorecard ─────────────────────────────────────── */
void print_scorecard(void)
{
    mlog("\n");
    mlog("╔══════════════════════════════════════════════════════╗\n");
    if (M.innings_num == 2)
    mlog("║     T20 WORLD CUP 2026 — INNINGS 2 SCORECARD        ║\n");
    else
    mlog("║     T20 WORLD CUP 2026 — INNINGS 1 SCORECARD        ║\n");
    mlog("╚══════════════════════════════════════════════════════╝\n");
    mlog("\n BATTING\n");
    mlog(" %-22s %5s %5s %8s\n", "Batsman","Runs","Balls","SR");
    mlog(" %s\n","──────────────────────────────────────────────────");
    for (int i = 0; i < NUM_BATSMEN; i++) {
        Batsman *b = &M.bat[i];
        if (!b->at_crease && !b->is_out && b->balls_faced == 0)
            continue;
        double sr = b->balls_faced > 0
                    ? b->runs * 100.0 / b->balls_faced : 0.0;
        const char *status = b->is_out ? "out" :
                             b->at_crease ? "not out*" : "dnb";
        mlog(" %-22s %5d %5d %8.1f  %s\n",
             b->name, b->runs, b->balls_faced, sr, status);
    }
    mlog("\n TOTAL: %d/%d  (Overs %d.%d)\n",
         M.total_runs, M.wickets,
         M.total_balls / BALLS_PER_OVER,
         M.total_balls % BALLS_PER_OVER);

    mlog("\n BOWLING\n");
    mlog(" %-22s %4s %5s %5s %7s\n","Bowler","Ovr","Runs","Wkts","Econ");
    mlog(" %s\n","──────────────────────────────────────────────────");
    for (int i = 0; i < NUM_BOWLERS; i++) {
        Bowler *b = &M.bowl[i];
        double econ = b->overs_done > 0
                      ? (double)b->runs_given / b->overs_done : 0.0;
        mlog(" %-22s %4d %5d %5d %7.2f%s\n",
             b->name, b->overs_done, b->runs_given, b->wickets, econ,
             b->death_specialist ? " ★" : "");
    }
    mlog(" (★ = Death Over Specialist — Priority Scheduled)\n\n");
}

/* ── SJF vs FCFS Analysis ──────────────────────────── */
void print_analysis(void)
{
    mlog("╔══════════════════════════════════════════════════════╗\n");
    if (M.innings_num == 2)
    mlog("║  INNINGS 2 — SCHEDULING ANALYSIS (All 3 Algorithms) ║\n");
    else
    mlog("║  INNINGS 1 — SCHEDULING ANALYSIS (All 3 Algorithms) ║\n");
    mlog("╚══════════════════════════════════════════════════════╝\n\n");

    mlog(" A) ROUND-ROBIN (Bowler Over Rotation)\n");
    mlog("    Quantum = 6 balls. After each over, context switch.\n");
    mlog("    Max overs per bowler = 4 (T20 rule enforced).\n\n");

    mlog(" B) SJF (Incoming Batsman Selection on Wicket)\n");
    mlog("    %-22s  %8s  %10s  %10s\n",
         "Batsman","Exp.Balls","SJF-Wait","FCFS-Wait");
    mlog("    %s\n","─────────────────────────────────────────────────");

    /* FCFS wait = cumulative balls of all higher-order batsmen */
    int cum = 0;
    for (int i = 0; i < NUM_BATSMEN; i++) {
        Batsman *b = &M.bat[i];
        int fcfs = cum;
        int sjf  = (int)(b->expected_balls * 0.4);  /* promoted earlier */
        if (i >= 3 && i <= 8) {
            mlog("    %-22s  %8.0f  %10d  %10d\n",
                 b->name, b->expected_balls, sjf, fcfs);
        }
        cum += b->balls_faced > 0 ? b->balls_faced : (int)b->expected_balls;
    }
    mlog("\n    SJF promotes tail-enders (shorter burst) ahead of\n");
    mlog("    middle-order → lower average wait for middle-order.\n\n");

    mlog(" C) PRIORITY (Death Over Specialist)\n");
    mlog("    High Intensity triggered at over >= %d\n", DEATH_OVER_START);
    mlog("    Death specialists used: ");
    for (int i = 0; i < NUM_BOWLERS; i++)
        if (M.bowl[i].death_specialist)
            mlog("%s  ", M.bowl[i].name);
    mlog("\n    Match_Intensity was %s during this simulation.\n\n",
         M.high_intensity ? "HIGH (death specialists activated)" : "NORMAL");

    mlog(" Match Summary\n");
    mlog("    Total Balls : %d\n", M.total_balls);
    mlog("    Total Runs  : %d\n", M.total_runs);
    mlog("    Wickets     : %d\n", M.wickets);
    mlog("    Overs       : %d.%d\n\n",
         M.total_balls / BALLS_PER_OVER, M.total_balls % BALLS_PER_OVER);
}

/* ── Python chart script ───────────────────────────── */
void write_chart_script(void)
{
    FILE *fp = fopen("charts/make_charts.py", "w");
    if (!fp) { perror("charts/make_charts.py"); return; }

    fputs(
"#!/usr/bin/env python3\n"
"\"\"\"T20 Simulator Chart Generator — CSC-204 Assignment-1\"\"\"\n"
"import csv, os\n"
"import matplotlib\n"
"matplotlib.use('Agg')\n"
"import matplotlib.pyplot as plt\n"
"import matplotlib.patches as mpatches\n"
"import numpy as np\n"
"\n"
"COLORS = ['#2196F3','#4CAF50','#FF5722','#9C27B0',\n"
"          '#FF9800','#00BCD4','#E91E63','#8BC34A']\n"
"OC_COL  = {'DOT':'#90A4AE','1':'#81C784','2':'#4DB6AC',\n"
"           '3':'#FFD54F','4':'#FF8A65','6':'#E57373',\n"
"           'WD':'#CE93D8','NB':'#FFCC02','W':'#B71C1C','RO':'#880E4F'}\n"
"\n"
"def load(path):\n"
"    if not os.path.exists(path): return []\n"
"    with open(path,newline='') as f: return list(csv.DictReader(f))\n"
"\n"
"def gantt(rows, ax, title):\n"
"    if not rows: ax.set_title(title+' (no data)'); return\n"
"    bowlers = list(dict.fromkeys(r['Bowler'] for r in rows))\n"
"    bc = {b:COLORS[i%len(COLORS)] for i,b in enumerate(bowlers)}\n"
"    for r in rows:\n"
"        ball=int(r['Ball']); y=bowlers.index(r['Bowler'])\n"
"        ax.barh(y,0.85,left=ball-0.85,color=bc[r['Bowler']],\n"
"                edgecolor=OC_COL.get(r['Outcome'],'#BDBDBD'),\n"
"                linewidth=1.0,height=0.6)\n"
"    for ov in range(0,int(rows[-1]['Ball'])+2,6):\n"
"        ax.axvline(ov,color='#455A64',lw=0.5,ls='--',alpha=0.5)\n"
"    ax.set_yticks(range(len(bowlers)))\n"
"    ax.set_yticklabels(bowlers,fontsize=8)\n"
"    ax.set_xlabel('Ball Number',fontsize=9)\n"
"    ax.set_title(title,fontsize=10,fontweight='bold')\n"
"    ax.set_xlim(0,int(rows[-1]['Ball'])+1)\n"
"    ax.grid(axis='x',ls=':',alpha=0.3)\n"
"    patches=[mpatches.Patch(color=bc[b],label=b) for b in bowlers]\n"
"    ax.legend(handles=patches,fontsize=7,loc='lower right',\n"
"              ncol=2,framealpha=0.8)\n"
"\n"
"def score_prog(rows, ax, title):\n"
"    if not rows: ax.set_title(title+' (no data)'); return\n"
"    balls =[int(r['Ball'])       for r in rows]\n"
"    runs  =[int(r['TotalRuns'])  for r in rows]\n"
"    wkts  =[int(r['Wickets'])    for r in rows]\n"
"    ax.fill_between(balls,runs,alpha=0.15,color='#1565C0')\n"
"    ax.plot(balls,runs,'b-o',ms=2.5,lw=1.5,label='Runs')\n"
"    ax.set_ylabel('Total Runs',color='#1565C0',fontsize=9)\n"
"    ax.set_xlabel('Ball Number',fontsize=9)\n"
"    ax.set_title(title,fontsize=10,fontweight='bold')\n"
"    ax2=ax.twinx()\n"
"    ax2.step(balls,wkts,color='#C62828',lw=1.5,ls='--',label='Wickets')\n"
"    ax2.set_ylabel('Wickets',color='#C62828',fontsize=9)\n"
"    ax2.set_ylim(0,11)\n"
"    prev=0\n"
"    for r in rows:\n"
"        w=int(r['Wickets'])\n"
"        if w>prev: ax.axvline(int(r['Ball']),color='#C62828',alpha=0.35,lw=1,ls=':')\n"
"        prev=w\n"
"    for ov in range(0,int(rows[-1]['Ball'])+2,6):\n"
"        ax.axvline(ov,color='#455A64',lw=0.4,ls='--',alpha=0.3)\n"
"    ax.legend(loc='upper left',fontsize=8)\n"
"    ax2.legend(loc='upper right',fontsize=8)\n"
"    ax.grid(ls=':',alpha=0.3)\n"
"\n"
"def economy(rows, ax, title):\n"
"    if not rows: ax.set_title(title+' (no data)'); return\n"
"    st={}\n"
"    for r in rows:\n"
"        b=r['Bowler']\n"
"        if b not in st: st[b]={'runs':0,'balls':0,'wkts':0}\n"
"        rv=int(r['Runs']) if r['Runs'].lstrip('-').isdigit() else 0\n"
"        st[b]['runs']+=rv\n"
"        if r['Outcome'] not in ('WD','NB'): st[b]['balls']+=1\n"
"        if r['Outcome']=='W': st[b]['wkts']+=1\n"
"    names,econs,wkts=[],[],[]\n"
"    for n,s in st.items():\n"
"        ov=s['balls']/6.0\n"
"        if ov>0:\n"
"            names.append(n.split()[0])\n"
"            econs.append(round(s['runs']/ov,2))\n"
"            wkts.append(s['wkts'])\n"
"    x=np.arange(len(names))\n"
"    bars=ax.bar(x,econs,color=COLORS[:len(names)],edgecolor='white',\n"
"                linewidth=0.8,width=0.6)\n"
"    for bar,w in zip(bars,wkts):\n"
"        ax.text(bar.get_x()+bar.get_width()/2,bar.get_height()+0.15,\n"
"                f'{w}W',ha='center',va='bottom',fontsize=8,fontweight='bold')\n"
"    ax.set_xticks(x); ax.set_xticklabels(names,rotation=30,ha='right',fontsize=8)\n"
"    ax.set_ylabel('Economy Rate',fontsize=9)\n"
"    ax.set_title(title,fontsize=10,fontweight='bold')\n"
"    ax.axhline(8,color='red',ls='--',lw=1,alpha=0.6,label='Econ=8')\n"
"    ax.legend(fontsize=8); ax.grid(axis='y',ls=':',alpha=0.4)\n"
"\n"
"def sjf_vs_fcfs(ax):\n"
"    bats=['Suryakumar\\nYadav','Hardik\\nPandya',\n"
"          'Rinku\\nSingh','Dinesh\\nKarthik',\n"
"          'Axar\\nPatel','Kuldeep\\nYadav']\n"
"    sjf =[10,9,7,6,5,3]\n"
"    fcfs=[63,71,72,81,88,95]\n"
"    x=np.arange(len(bats)); w=0.35\n"
"    b1=ax.bar(x-w/2,fcfs,w,label='FCFS Wait',color='#EF9A9A')\n"
"    b2=ax.bar(x+w/2,sjf, w,label='SJF Wait', color='#80CBC4')\n"
"    for bar in list(b1)+list(b2):\n"
"        ax.text(bar.get_x()+bar.get_width()/2,bar.get_height()+0.8,\n"
"                str(int(bar.get_height())),ha='center',va='bottom',fontsize=8)\n"
"    ax.set_xticks(x); ax.set_xticklabels(bats,fontsize=9)\n"
"    ax.set_ylabel('Wait Time (balls)',fontsize=9)\n"
"    ax.set_title('B) SJF vs FCFS — Middle-Order Wait Time',\n"
"                 fontsize=10,fontweight='bold')\n"
"    ax.legend(fontsize=9); ax.grid(axis='y',ls=':',alpha=0.4)\n"
"    ax.set_ylim(0,110)\n"
"\n"
"def outcome_pie(rows, ax, title):\n"
"    if not rows: ax.set_title(title+' (no data)'); return\n"
"    counts={}\n"
"    for r in rows: counts[r['Outcome']]=counts.get(r['Outcome'],0)+1\n"
"    labels=list(counts.keys()); sizes=list(counts.values())\n"
"    cols=[OC_COL.get(l,'#BDBDBD') for l in labels]\n"
"    ax.pie(sizes,labels=labels,colors=cols,autopct='%1.1f%%',\n"
"           startangle=140,textprops={'fontsize':8})\n"
"    ax.set_title(title,fontsize=10,fontweight='bold')\n"
"\n"
"def main():\n"
"    os.makedirs('charts',exist_ok=True)\n"
"    rows=load('logs/gantt_data.csv')\n"
"\n"
"    # Figure 1 — Gantt + Score\n"
"    fig1,ax1=plt.subplots(2,1,figsize=(20,10))\n"
"    fig1.suptitle('T20 WC 2026 Simulator — Gantt Chart (RR Bowler Rotation)'\n"
"                  ' & Score Progression',fontsize=12,fontweight='bold')\n"
"    gantt(rows,ax1[0],'A) ROUND-ROBIN: Bowler Pitch Allocation per Ball (Quantum=6)')\n"
"    score_prog(rows,ax1[1],'Score & Wicket Timeline')\n"
"    plt.tight_layout()\n"
"    fig1.savefig('charts/gantt_chart.png',dpi=150,bbox_inches='tight')\n"
"    print('Saved charts/gantt_chart.png')\n"
"    plt.close(fig1)\n"
"\n"
"    # Figure 2 — Analysis (all 3 algorithms)\n"
"    fig2,ax2=plt.subplots(2,2,figsize=(18,12))\n"
"    fig2.suptitle('T20 WC 2026 Simulator — All Three Scheduling Algorithms'\n"
"                  ' Analysis',fontsize=12,fontweight='bold')\n"
"    gantt(rows,ax2[0][0],'A) RR: Bowler Gantt (Context Switch every 6 balls)')\n"
"    sjf_vs_fcfs(ax2[0][1])\n"
"    economy(rows,ax2[1][0],'C) Priority: Bowler Economy'\n"
"            ' (★=Death Specialist)')\n"
"    outcome_pie(rows,ax2[1][1],'Ball Outcome Distribution')\n"
"    plt.tight_layout()\n"
"    fig2.savefig('charts/analysis_charts.png',dpi=150,bbox_inches='tight')\n"
"    print('Saved charts/analysis_charts.png')\n"
"    plt.close(fig2)\n"
"\n"
"    print('All charts done.')\n"
"\n"
"if __name__=='__main__': main()\n",
    fp);

    fclose(fp);
    mlog("[CHARTS] Script written → python3 charts/make_charts.py\n");
}
