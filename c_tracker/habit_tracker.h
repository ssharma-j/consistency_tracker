#ifndef HABIT_TRACKER_H
#define HABIT_TRACKER_H

/* -----------------------------------------------------------------------
 * habit_tracker.h  –  data structures and public API
 * ----------------------------------------------------------------------- */

#define MAX_HABITS   10
#define MAX_NAME_LEN 64
#define MAX_DAYS     365
#define DATE_LEN     11   /* "YYYY-MM-DD\0" */

#define DATA_FILE  "habit_data.txt"
#define CSV_FILE   "habits_export.csv"
#define PDF_FILE   "habits_export.pdf"

/* A single habit definition */
typedef struct {
    char name[MAX_NAME_LEN];
    int  active;   /* 1 = in use, 0 = deleted */
} Habit;

/* One day's completion record */
typedef struct {
    char date[DATE_LEN];
    /*
     * status[i]:  1 = completed (yes)
     *             0 = not completed (no)
     *            -1 = not recorded for that slot
     */
    int  status[MAX_HABITS];
} DayRecord;

/* Top-level tracker state */
typedef struct {
    Habit     habits[MAX_HABITS];
    int       habit_count;   /* total slots used (including deleted) */
    DayRecord records[MAX_DAYS];
    int       record_count;
} HabitTracker;

/* ---- function declarations ---- */
void init_tracker   (HabitTracker *t);
void add_habit      (HabitTracker *t);
void view_habits    (HabitTracker *t);
void mark_today     (HabitTracker *t);
void view_history   (HabitTracker *t);
void show_statistics(HabitTracker *t);
void export_csv     (HabitTracker *t);
void export_pdf     (HabitTracker *t);
void save_data      (HabitTracker *t);
void load_data      (HabitTracker *t);

/* Utility */
void get_today      (char *buf);           /* fills buf with "YYYY-MM-DD" */
int  find_record    (HabitTracker *t, const char *date); /* index or -1 */

#endif /* HABIT_TRACKER_H */
