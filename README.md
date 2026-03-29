# T20 WC Cricket Simulator — CSC-204 Assignment-1

## Build & Run

```bash
# 1. Build
make

# 2. Run simulation (single command, all 3 algorithms active)
./cricket_sim

# 3. Generate charts
python3 charts/make_charts.py
```

That's it. One run demonstrates all three scheduling algorithms simultaneously.

---

## Scheduling Algorithms (all run in one simulation)

| Algorithm | Applied To | How |
|---|---|---|
| **Round-Robin** | Bowler over transitions | Quantum = 6 balls; context switch after every over |
| **SJF** | Incoming batsman on wicket | Tail-ender with shortest `expected_balls` promoted first |
| **Priority** | Bowler selection in death overs | When over ≥ 14, death specialist gets highest CPU priority |

---

## Thread Count: 27 total

- 6 Bowler threads
- 11 Batsman threads (semaphore limits crease to 2 at a time)
- 10 Fielder threads (sleep on `pthread_cond_wait`, wake on `ball_in_air`)

---

## Synchronization

| Primitive | Variable | Guards |
|---|---|---|
| Mutex | `score_mtx` | Atomic score update (Wide + Single same cycle) |
| Mutex | `pitch_mtx` | Pitch = Critical Section (one bowler at a time) |
| Semaphore(2) | `crease_sem` | Only 2 batsmen at crease; 3rd blocks in WAIT |
| Cond Var | `cv_delivery` | Batsman waits for bowler to deliver |
| Cond Var | `cv_stroke` | Bowler waits for batsman stroke to complete |
| Cond Var | `cv_air` | Fielders sleep until `ball_in_air = true` |
| Mutex | `runout_mtx` | Deadlock (run-out) resource allocation |

---

## Deadlock Handling

Run-out = circular wait. Batsman A holds End-2 wants End-1; Batsman B holds End-1 wants End-2.
RAG cycle detected → Umpire (kernel) kills the striker thread. Next batsman enters via SJF.

---

## Output Files

| File | Contents |
|---|---|
| `logs/match_log.txt` | Full ball-by-ball commentary |
| `logs/gantt_data.csv` | Per-ball data for charts |
| `charts/gantt_chart.png` | Gantt (RR bowler allocation) + score graph |
| `charts/analysis_charts.png` | RR Gantt, SJF vs FCFS, economy, outcomes |

---

## Requirements

- GCC + pthreads (Linux/WSL/macOS)
- Python 3 + matplotlib (`pip3 install matplotlib`)
