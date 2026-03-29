#!/usr/bin/env python3
"""T20 Simulator Chart Generator — FINAL FIXED VERSION"""

import csv, os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

COLORS = ['#2196F3','#4CAF50','#FF5722','#9C27B0',
          '#FF9800','#00BCD4','#E91E63','#8BC34A']

OC_COL  = {'DOT':'#90A4AE','1':'#81C784','2':'#4DB6AC',
           '3':'#FFD54F','4':'#FF8A65','6':'#E57373',
           'WD':'#CE93D8','NB':'#FFCC02','W':'#B71C1C','RO':'#880E4F'}


# ✅ GLOBAL CLEAN FUNCTION
def clean_rows(rows):
    clean = []
    for r in rows:
        ball = str(r.get('Ball','')).strip()
        if not ball.isdigit():
            continue
        if not r.get('Bowler'):
            continue
        clean.append(r)
    return clean


def load(path):
    if not os.path.exists(path):
        return []
    with open(path,newline='') as f:
        return list(csv.DictReader(f))


# ================= GANTT =================
def gantt(rows, ax, title):
    rows = clean_rows(rows)

    if not rows:
        ax.set_title(title+' (no data)')
        return

    bowlers = list(dict.fromkeys(r['Bowler'] for r in rows))
    bc = {b:COLORS[i%len(COLORS)] for i,b in enumerate(bowlers)}

    for r in rows:
        ball = int(r['Ball'])
        y = bowlers.index(r['Bowler'])

        ax.barh(y,0.85,left=ball-0.85,
                color=bc[r['Bowler']],
                edgecolor=OC_COL.get(r.get('Outcome',''),'#BDBDBD'),
                linewidth=1.0,height=0.6)

    max_ball = int(rows[-1]['Ball'])

    for ov in range(0, max_ball+2, 6):
        ax.axvline(ov,color='#455A64',lw=0.5,ls='--',alpha=0.5)

    ax.set_yticks(range(len(bowlers)))
    ax.set_yticklabels(bowlers,fontsize=8)
    ax.set_xlabel('Ball Number',fontsize=9)
    ax.set_title(title,fontsize=10,fontweight='bold')
    ax.set_xlim(0, max_ball+1)
    ax.grid(axis='x',ls=':',alpha=0.3)

    patches=[mpatches.Patch(color=bc[b],label=b) for b in bowlers]
    ax.legend(handles=patches,fontsize=7,loc='lower right',
              ncol=2,framealpha=0.8)


# ================= SCORE PROGRESSION =================
def score_prog(rows, ax, title):
    rows = clean_rows(rows)

    if not rows:
        ax.set_title(title+' (no data)')
        return

    balls = [int(r['Ball']) for r in rows]
    runs  = [int(r.get('TotalRuns',0)) for r in rows]
    wkts  = [int(r.get('Wickets',0)) for r in rows]

    ax.fill_between(balls,runs,alpha=0.15,color='#1565C0')
    ax.plot(balls,runs,'b-o',ms=2.5,lw=1.5,label='Runs')

    ax.set_ylabel('Total Runs',color='#1565C0',fontsize=9)
    ax.set_xlabel('Ball Number',fontsize=9)
    ax.set_title(title,fontsize=10,fontweight='bold')

    ax2=ax.twinx()
    ax2.step(balls,wkts,color='#C62828',lw=1.5,ls='--',label='Wickets')
    ax2.set_ylabel('Wickets',color='#C62828',fontsize=9)
    ax2.set_ylim(0,11)

    prev=0
    for r in rows:
        w=int(r.get('Wickets',0))
        if w>prev:
            ax.axvline(int(r['Ball']),color='#C62828',alpha=0.35,lw=1,ls=':')
        prev=w

    max_ball = int(rows[-1]['Ball'])
    for ov in range(0, max_ball+2, 6):
        ax.axvline(ov,color='#455A64',lw=0.4,ls='--',alpha=0.3)

    ax.legend(loc='upper left',fontsize=8)
    ax2.legend(loc='upper right',fontsize=8)
    ax.grid(ls=':',alpha=0.3)


# ================= ECONOMY =================
def economy(rows, ax, title):
    rows = clean_rows(rows)

    if not rows:
        ax.set_title(title+' (no data)')
        return

    st={}
    for r in rows:
        b=r['Bowler']
        if b not in st:
            st[b]={'runs':0,'balls':0,'wkts':0}

        rv=int(r.get('Runs',0)) if str(r.get('Runs','')).lstrip('-').isdigit() else 0
        st[b]['runs']+=rv

        if r.get('Outcome') not in ('WD','NB'):
            st[b]['balls']+=1

        if r.get('Outcome')=='W':
            st[b]['wkts']+=1

    names,econs,wkts=[],[],[]

    for n,s in st.items():
        ov=s['balls']/6.0
        if ov>0:
            names.append(n.split()[0])
            econs.append(round(s['runs']/ov,2))
            wkts.append(s['wkts'])

    x=np.arange(len(names))
    bars=ax.bar(x,econs,color=COLORS[:len(names)],
                edgecolor='white',linewidth=0.8,width=0.6)

    for bar,w in zip(bars,wkts):
        ax.text(bar.get_x()+bar.get_width()/2,
                bar.get_height()+0.15,
                f'{w}W',ha='center',va='bottom',
                fontsize=8,fontweight='bold')

    ax.set_xticks(x)
    ax.set_xticklabels(names,rotation=30,ha='right',fontsize=8)
    ax.set_ylabel('Economy Rate',fontsize=9)
    ax.set_title(title,fontsize=10,fontweight='bold')
    ax.axhline(8,color='red',ls='--',lw=1,alpha=0.6,label='Econ=8')
    ax.legend(fontsize=8)
    ax.grid(axis='y',ls=':',alpha=0.4)


# ================= PIE =================
def outcome_pie(rows, ax, title):
    rows = clean_rows(rows)

    if not rows:
        ax.set_title(title+' (no data)')
        return

    counts={}
    for r in rows:
        o = r.get('Outcome','UNKNOWN')
        counts[o]=counts.get(o,0)+1

    labels=list(counts.keys())
    sizes=list(counts.values())
    cols=[OC_COL.get(l,'#BDBDBD') for l in labels]

    ax.pie(sizes,labels=labels,colors=cols,
           autopct='%1.1f%%',startangle=140,
           textprops={'fontsize':8})

    ax.set_title(title,fontsize=10,fontweight='bold')


# ================= MAIN =================
def main():
    os.makedirs('charts',exist_ok=True)

    rows = load('logs/gantt_data.csv')

    fig1,ax1=plt.subplots(2,1,figsize=(20,10))
    gantt(rows,ax1[0],'Bowler Gantt')
    score_prog(rows,ax1[1],'Score Timeline')

    plt.tight_layout()
    fig1.savefig('charts/gantt_chart.png',dpi=150)
    plt.close(fig1)

    fig2,ax2=plt.subplots(2,2,figsize=(18,12))
    gantt(rows,ax2[0][0],'RR Scheduling')
    economy(rows,ax2[1][0],'Economy Rate')
    outcome_pie(rows,ax2[1][1],'Outcomes')

    plt.tight_layout()
    fig2.savefig('charts/analysis_charts.png',dpi=150)
    plt.close(fig2)

    print("✅ Charts generated successfully")


if __name__=='__main__':
    main()