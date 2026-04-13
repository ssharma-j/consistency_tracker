# Habit Tracker (C CLI)

A fully local, command-line habit tracker written in C.  
No external libraries or network dependencies required.

## Features

| # | Feature |
|---|---------|
| 1 | Add up to **10 habits** by name |
| 2 | **Mark each day** – yes (`y`) / no (`n`) per habit |
| 3 | **Persistent storage** – data is saved to `habit_data.txt` and reloaded automatically on next run |
| 4 | **History view** – see all recorded days in reverse-chronological order |
| 5 | **Statistics** – completion rate, current streak, best streak per habit, plus an overall streak |
| 6 | **CSV export** – `habits_export.csv` (opens in Excel / LibreOffice Calc) |
| 7 | **PDF export** – `habits_export.pdf` (self-contained, no external PDF library needed) |

---

## Building

```bash
cd c_tracker
make          # produces ./habit_tracker
```

Requirements: any C99-compatible compiler (`gcc` or `clang`).

To clean build artefacts:

```bash
make clean
```

---

## Running

```bash
./habit_tracker
```

You will be presented with an interactive menu:

```
┌─────────────────────────────────────┐
│        HABIT TRACKER  v1.0          │
├─────────────────────────────────────┤
│  1. Add habit                       │
│  2. View habits                     │
│  3. Mark today's habits             │
│  4. View history                    │
│  5. Show statistics                 │
│  6. Export to CSV (Excel)           │
│  7. Export to PDF                   │
│  8. Quit                            │
└─────────────────────────────────────┘
Choice:
```

### Typical first-time workflow

1. Select **1** to add your first habit (e.g. `Exercise`).  
   Repeat up to 10 times.
2. Select **3** every day to mark which habits you completed.  
   Type `y` (yes), `n` (no), or press **Enter** to skip.
3. Select **5** to see your streaks and completion rates.
4. Select **6** to export to CSV or **7** to export to PDF.

---

## Output files

| File | Description |
|------|-------------|
| `habit_data.txt` | Plain-text persistence file (auto-created) |
| `habits_export.csv` | Excel-compatible spreadsheet with daily log + summary |
| `habits_export.pdf` | PDF report with habit list, daily history, and statistics |

---

## Data structures

```c
/* One habit definition */
typedef struct {
    char name[64];
    int  active;     /* 1 = in use, 0 = deleted */
} Habit;

/* One day's record */
typedef struct {
    char date[11];           /* "YYYY-MM-DD" */
    int  status[MAX_HABITS]; /* 1=yes, 0=no, -1=not recorded */
} DayRecord;

/* Top-level state */
typedef struct {
    Habit     habits[10];
    int       habit_count;
    DayRecord records[365];
    int       record_count;
} HabitTracker;
```

---

## File format (`habit_data.txt`)

```
HABITS 3
0 1 Exercise
1 1 Read 20 pages
2 1 Meditate
RECORDS 1
2026-04-13 1 0 1 -1 -1 -1 -1 -1 -1 -1
```

Each record line: `<date> <status[0]> … <status[9]>`  
Status values: `1` = yes, `0` = no, `-1` = not recorded.

---

## Limitations

- Maximum **10 habits** and **365 days** of history (configurable via `#define` in `habit_tracker.h`).
- The PDF uses standard Type1 fonts (Courier / Courier-Bold) built into every PDF viewer; no font files need to be installed.
- History wraps after 365 days (oldest records are overwritten). For longer periods, increase `MAX_DAYS`.
