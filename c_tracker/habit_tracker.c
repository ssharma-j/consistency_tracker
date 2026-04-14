/* =========================================================================
 * habit_tracker.c  –  Local habit-tracking CLI application
 *
 * Features
 * --------
 *  • Add / view up to 10 habits
 *  • Mark each habit yes/no for today
 *  • Persistent storage (plain-text file)
 *  • View full history
 *  • Statistics: per-habit completion %, current streak, best streak
 *  • Export to CSV  (Excel-compatible)
 *  • Export to PDF  (self-contained, no external libraries)
 *
 * Compile:  see Makefile
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#include "habit_tracker.h"

/* =========================================================================
 * Utility helpers
 * ========================================================================= */

/* Fill buf (size >= DATE_LEN) with today as "YYYY-MM-DD" */
void get_today(char *buf)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, DATE_LEN, "%Y-%m-%d", tm);
}

/* Return index of a DayRecord matching date, or -1 if not found */
int find_record(HabitTracker *t, const char *date)
{
    for (int i = 0; i < t->record_count; i++)
        if (strcmp(t->records[i].date, date) == 0)
            return i;
    return -1;
}

/* Flush stdin after scanf to avoid stray newlines */
static void flush_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

/* Read a trimmed line into buf (max len-1 chars) */
static void read_line(char *buf, int len)
{
    if (!fgets(buf, len, stdin)) {
        buf[0] = '\0';
        return;
    }
    /* strip trailing newline / whitespace */
    int n = (int)strlen(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = '\0';
}

/* =========================================================================
 * Initialisation
 * ========================================================================= */

void init_tracker(HabitTracker *t)
{
    memset(t, 0, sizeof(*t));
    /* mark all record slots as "not recorded" */
    for (int i = 0; i < MAX_DAYS; i++)
        for (int j = 0; j < MAX_HABITS; j++)
            t->records[i].status[j] = -1;
}

/* =========================================================================
 * Habit management
 * ========================================================================= */

void add_habit(HabitTracker *t)
{
    /* count active habits */
    int active = 0;
    for (int i = 0; i < t->habit_count; i++)
        if (t->habits[i].active)
            active++;

    if (active >= MAX_HABITS) {
        printf("You already have %d active habits (maximum).\n", MAX_HABITS);
        return;
    }
    if (t->habit_count >= MAX_HABITS) {
        printf("No free slots available.\n");
        return;
    }

    printf("Enter habit name: ");
    fflush(stdout);
    char name[MAX_NAME_LEN];
    read_line(name, sizeof(name));

    if (strlen(name) == 0) {
        printf("Empty name – habit not added.\n");
        return;
    }

    /* reuse a deleted slot if possible */
    int slot = -1;
    for (int i = 0; i < t->habit_count; i++) {
        if (!t->habits[i].active) { slot = i; break; }
    }
    if (slot == -1)
        slot = t->habit_count++;

    snprintf(t->habits[slot].name, MAX_NAME_LEN, "%s", name);
    t->habits[slot].active = 1;

    printf("Habit \"%s\" added (slot %d).\n", name, slot + 1);
    save_data(t);
}

void view_habits(HabitTracker *t)
{
    int any = 0;
    printf("\n--- Active Habits ---\n");
    for (int i = 0; i < t->habit_count; i++) {
        if (t->habits[i].active) {
            printf("  [%d] %s\n", i + 1, t->habits[i].name);
            any = 1;
        }
    }
    if (!any)
        printf("  (none – use 'Add habit' to get started)\n");
    printf("---------------------\n\n");
}

/* =========================================================================
 * Daily tracking
 * ========================================================================= */

void mark_today(HabitTracker *t)
{
    char today[DATE_LEN];
    get_today(today);

    /* Find or create today's record */
    int ri = find_record(t, today);
    if (ri == -1) {
        if (t->record_count >= MAX_DAYS) {
            printf("Record limit reached (%d days).\n", MAX_DAYS);
            return;
        }
        ri = t->record_count++;
        strncpy(t->records[ri].date, today, DATE_LEN);
        for (int j = 0; j < MAX_HABITS; j++)
            t->records[ri].status[j] = -1;
    }

    printf("\n--- Mark habits for %s ---\n", today);
    int marked = 0;
    for (int i = 0; i < t->habit_count; i++) {
        if (!t->habits[i].active) continue;

        int cur = t->records[ri].status[i];
        printf("  [%d] %-40s  (current: %s)\n",
               i + 1,
               t->habits[i].name,
               cur == 1 ? "YES" : (cur == 0 ? "NO" : "not set"));
        printf("       Enter y/n (or Enter to skip): ");
        fflush(stdout);

        char ans[8];
        read_line(ans, sizeof(ans));

        if (ans[0] == 'y' || ans[0] == 'Y') {
            t->records[ri].status[i] = 1;
            marked++;
        } else if (ans[0] == 'n' || ans[0] == 'N') {
            t->records[ri].status[i] = 0;
            marked++;
        }
    }

    if (marked == 0)
        printf("No changes made.\n");
    else {
        printf("Saved %d update(s) for %s.\n", marked, today);
        save_data(t);
    }
}

/* =========================================================================
 * History view
 * ========================================================================= */

void view_history(HabitTracker *t)
{
    if (t->record_count == 0 || t->habit_count == 0) {
        printf("No history recorded yet.\n");
        return;
    }

    printf("\n--- History (most recent first) ---\n");

    /* header row */
    printf("%-12s", "Date");
    for (int i = 0; i < t->habit_count; i++)
        if (t->habits[i].active)
            printf("  %-18.18s", t->habits[i].name);
    printf("\n");

    /* separator */
    printf("%-12s", "------------");
    for (int i = 0; i < t->habit_count; i++)
        if (t->habits[i].active)
            printf("  %-18s", "------------------");
    printf("\n");

    /* rows (reverse chronological) */
    for (int r = t->record_count - 1; r >= 0; r--) {
        printf("%-12s", t->records[r].date);
        for (int i = 0; i < t->habit_count; i++) {
            if (!t->habits[i].active) continue;
            int s = t->records[r].status[i];
            printf("  %-18s", s == 1 ? "YES" : (s == 0 ? "NO" : "-"));
        }
        printf("\n");
    }
    printf("-----------------------------------\n\n");
}

/* =========================================================================
 * Statistics
 * ========================================================================= */

void show_statistics(HabitTracker *t)
{
    if (t->habit_count == 0) {
        printf("No habits defined yet.\n");
        return;
    }

    char today[DATE_LEN];
    get_today(today);

    printf("\n=== Statistics ===\n");

    /* Per-habit stats */
    for (int i = 0; i < t->habit_count; i++) {
        if (!t->habits[i].active) continue;

        int yes = 0, no = 0, total = 0;
        int cur_streak = 0, best_streak = 0, run = 0;

        for (int r = 0; r < t->record_count; r++) {
            int s = t->records[r].status[i];
            if (s == 1) { yes++; total++; run++; }
            else if (s == 0) { no++; total++; run = 0; }
            if (run > best_streak) best_streak = run;
        }

        /* current streak: walk backwards from today's record */
        for (int r = t->record_count - 1; r >= 0; r--) {
            if (t->records[r].status[i] == 1)
                cur_streak++;
            else if (t->records[r].status[i] == 0)
                break;
            /* skip -1 (not recorded) without breaking streak */
        }

        double pct = total > 0 ? 100.0 * yes / total : 0.0;

        printf("\nHabit: %s\n", t->habits[i].name);
        printf("  Completed : %d day(s)\n", yes);
        printf("  Missed    : %d day(s)\n", no);
        printf("  Rate      : %.1f%%\n", pct);
        printf("  Best streak : %d day(s)\n", best_streak);
        printf("  Curr streak : %d day(s)\n", cur_streak);
    }

    /* Overall streak: all active habits must be YES */
    int overall = 0;
    for (int r = t->record_count - 1; r >= 0; r--) {
        int all_done = 1;
        for (int i = 0; i < t->habit_count; i++) {
            if (!t->habits[i].active) continue;
            if (t->records[r].status[i] != 1) { all_done = 0; break; }
        }
        if (all_done)
            overall++;
        else
            break;
    }
    printf("\nOverall streak (all habits done): %d day(s)\n", overall);
    printf("==================\n\n");
}

/* =========================================================================
 * CSV export
 * ========================================================================= */

void export_csv(HabitTracker *t)
{
    FILE *fp = fopen(CSV_FILE, "w");
    if (!fp) { perror("Cannot open " CSV_FILE); return; }

    /* Header row */
    fprintf(fp, "Date");
    for (int i = 0; i < t->habit_count; i++)
        if (t->habits[i].active)
            fprintf(fp, ",\"%s\"", t->habits[i].name);
    fprintf(fp, "\n");

    /* Data rows */
    for (int r = 0; r < t->record_count; r++) {
        fprintf(fp, "%s", t->records[r].date);
        for (int i = 0; i < t->habit_count; i++) {
            if (!t->habits[i].active) continue;
            int s = t->records[r].status[i];
            fprintf(fp, ",%s",
                    s == 1 ? "Yes" : (s == 0 ? "No" : ""));
        }
        fprintf(fp, "\n");
    }

    /* Summary section */
    fprintf(fp, "\n");
    fprintf(fp, "Summary\n");
    fprintf(fp, "Habit,Total Days,Completed,Missed,Rate (%%)\n");

    for (int i = 0; i < t->habit_count; i++) {
        if (!t->habits[i].active) continue;
        int yes = 0, no = 0;
        for (int r = 0; r < t->record_count; r++) {
            int s = t->records[r].status[i];
            if (s == 1) yes++;
            else if (s == 0) no++;
        }
        int total = yes + no;
        double pct = total > 0 ? 100.0 * yes / total : 0.0;
        fprintf(fp, "\"%s\",%d,%d,%d,%.1f\n",
                t->habits[i].name, total, yes, no, pct);
    }

    fclose(fp);
    printf("CSV exported to \"%s\".\n", CSV_FILE);
}

/* =========================================================================
 * PDF export  (minimal self-contained PDF writer – no external libs)
 * ========================================================================= */

/*
 * We build a small PDF manually.
 * Structure:
 *   obj 1  – Catalog
 *   obj 2  – Pages (container)
 *   obj 3  – Font (Courier)
 *   obj 4  – Font (Courier-Bold)
 *   obj 5 … N  – one Page + one content stream per page
 *
 * Each page is A4 (595 x 842 pt).
 */

#define PDF_PAGE_W  595
#define PDF_PAGE_H  842
#define PDF_MARGIN   40
#define PDF_LINE_H   14  /* line height in points */
#define PDF_FONT_SZ  10

/* We collect all lines in a dynamic array then paginate */
#define MAX_PDF_LINES 4096

static char pdf_lines[MAX_PDF_LINES][256];
static int  pdf_line_count = 0;

static void pdf_add_line(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void pdf_add_line(const char *fmt, ...)
{
    if (pdf_line_count >= MAX_PDF_LINES) return;
    /* NULL or empty string → emit a blank line and return immediately
     * so that vsnprintf is never called with a NULL format string. */
    if (fmt == NULL || fmt[0] == '\0') {
        pdf_lines[pdf_line_count++][0] = '\0';
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(pdf_lines[pdf_line_count++], 255, fmt, ap);
    va_end(ap);
}

/* Escape special PDF string characters */
static void pdf_escape(const char *src, char *dst, int dstlen)
{
    int j = 0;
    for (int i = 0; src[i] && j < dstlen - 2; i++) {
        if (src[i] == '(' || src[i] == ')' || src[i] == '\\')
            dst[j++] = '\\';
        if (j < dstlen - 1)
            dst[j++] = src[i];
    }
    dst[j] = '\0';
}

void export_pdf(HabitTracker *t)
{
    /* ---- build logical lines ---- */
    pdf_line_count = 0;

    char today[DATE_LEN];
    get_today(today);

    pdf_add_line("HABIT TRACKER REPORT");
    pdf_add_line("Generated: %s", today);
    pdf_add_line("========================================");
    pdf_add_line(NULL);

    /* Active habits list */
    pdf_add_line("HABITS");
    pdf_add_line("------");
    int none = 1;
    for (int i = 0; i < t->habit_count; i++) {
        if (t->habits[i].active) {
            pdf_add_line("  %d. %s", i + 1, t->habits[i].name);
            none = 0;
        }
    }
    if (none) pdf_add_line("  (no habits defined)");
    pdf_add_line(NULL);

    /* History */
    pdf_add_line("DAILY HISTORY");
    pdf_add_line("-------------");
    if (t->record_count == 0) {
        pdf_add_line("  (no records yet)");
    } else {
        /* Build a compact representation per day */
        for (int r = t->record_count - 1; r >= 0; r--) {
            /* header line for the date */
            pdf_add_line("  %s", t->records[r].date);
            for (int i = 0; i < t->habit_count; i++) {
                if (!t->habits[i].active) continue;
                int s = t->records[r].status[i];
                const char *mark = s == 1 ? "[YES]" : (s == 0 ? "[NO] " : "[ - ]");
                pdf_add_line("    %s  %s", mark, t->habits[i].name);
            }
        }
    }
    pdf_add_line(NULL);

    /* Statistics */
    pdf_add_line("STATISTICS");
    pdf_add_line("----------");
    for (int i = 0; i < t->habit_count; i++) {
        if (!t->habits[i].active) continue;
        int yes = 0, no = 0, best = 0, run = 0, cur = 0;
        for (int r = 0; r < t->record_count; r++) {
            int s = t->records[r].status[i];
            if (s == 1) { yes++; run++; }
            else if (s == 0) { no++; run = 0; }
            if (run > best) best = run;
        }
        for (int r = t->record_count - 1; r >= 0; r--) {
            if (t->records[r].status[i] == 1) cur++;
            else if (t->records[r].status[i] == 0) break;
        }
        int total = yes + no;
        double pct = total > 0 ? 100.0 * yes / total : 0.0;
        pdf_add_line("  %s", t->habits[i].name);
        pdf_add_line("    Rate: %.1f%%  Best streak: %d  Current streak: %d",
                     pct, best, cur);
    }

    /* ---- paginate ---- */
    int lines_per_page = (PDF_PAGE_H - 2 * PDF_MARGIN) / PDF_LINE_H;
    int total_pages = (pdf_line_count + lines_per_page - 1) / lines_per_page;
    if (total_pages == 0) total_pages = 1;

    /* ---- write PDF ---- */
    FILE *fp = fopen(PDF_FILE, "wb");
    if (!fp) { perror("Cannot open " PDF_FILE); return; }

    /* We need to track byte offsets for the xref table.
     * Object layout: 4 fixed objects + 2 objects (page + content) per page.
     * MAX_PDF_LINES / lines_per_page gives an upper bound on page count. */
    int max_pdf_pages = (MAX_PDF_LINES + lines_per_page - 1) / lines_per_page + 1;
    long *offsets = (long *)calloc((size_t)(4 + max_pdf_pages * 2), sizeof(long));
    if (!offsets) { fclose(fp); printf("Out of memory.\n"); return; }
    int  obj_count = 0;

    /* Object numbering:
     *   1 = Catalog
     *   2 = Pages
     *   3 = Font Courier
     *   4 = Font Courier-Bold
     *   5,6 = page1 page_obj, content_obj
     *   7,8 = page2 …  etc.
     */
    int first_page_obj = 5;

#define WRITE_OBJ_START(n)  do { offsets[(n)-1] = ftell(fp); \
                                  fprintf(fp, "%d 0 obj\n", (n)); } while(0)
#define WRITE_OBJ_END()     fprintf(fp, "endobj\n\n")

    /* PDF header */
    fprintf(fp, "%%PDF-1.4\n");
    fprintf(fp, "%%%c%c%c%c\n", 0xe2, 0xe3, 0xcf, 0xd3); /* binary hint */

    /* obj 1 – Catalog */
    WRITE_OBJ_START(1);
    fprintf(fp, "<< /Type /Catalog /Pages 2 0 R >>\n");
    WRITE_OBJ_END();

    /* obj 2 – Pages (we'll patch the Kids list and Count after) */
    /* We'll write it later once we know total_pages */

    /* obj 3 – Font Courier */
    WRITE_OBJ_START(3);
    fprintf(fp, "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\n");
    WRITE_OBJ_END();

    /* obj 4 – Font Courier-Bold */
    WRITE_OBJ_START(4);
    fprintf(fp, "<< /Type /Font /Subtype /Type1 /BaseFont /Courier-Bold >>\n");
    WRITE_OBJ_END();

    /* We write Pages obj after pages so we know their IDs.
     * Actually, forward references are fine in PDF – just write obj 2 now. */
    offsets[1] = ftell(fp);
    fprintf(fp, "2 0 obj\n");
    fprintf(fp, "<< /Type /Pages /Count %d /Kids [", total_pages);
    for (int p = 0; p < total_pages; p++)
        fprintf(fp, "%d 0 R ", first_page_obj + p * 2);
    fprintf(fp, "]\n>>\n");
    WRITE_OBJ_END();

    obj_count = 4; /* 1..4 written */

    /* ---- per-page objects ---- */
    int line_idx = 0;
    for (int p = 0; p < total_pages; p++) {
        int page_obj    = first_page_obj + p * 2;
        int content_obj = page_obj + 1;

        /* Build content stream in memory.
         * Each line in the stream is at most ~300 bytes (Tm operator + escaped text).
         * Allocate based on lines_per_page to avoid over-allocation. */
        size_t cbuf_size = (size_t)(lines_per_page + 4) * 320;
        char *cbuf = (char *)malloc(cbuf_size);
        if (!cbuf) { free(offsets); fclose(fp); printf("Out of memory.\n"); return; }
        int clen = 0;

        /* Begin text */
        clen += sprintf(cbuf + clen, "BT\n");
        /* Use bold for first line on first page */
        clen += sprintf(cbuf + clen, "/F2 %d Tf\n", PDF_FONT_SZ + 2);

        float y = PDF_PAGE_H - PDF_MARGIN - PDF_LINE_H;

        for (int li = 0; li < lines_per_page && line_idx < pdf_line_count; li++, line_idx++) {
            /* Switch to regular font after the title block on page 1 */
            if (p == 0 && line_idx == 0)
                ; /* bold already set */
            else if (p == 0 && line_idx == 3)
                clen += sprintf(cbuf + clen, "/F1 %d Tf\n", PDF_FONT_SZ);

            char esc[512];
            pdf_escape(pdf_lines[line_idx], esc, sizeof(esc));

            /* Use Tm (text matrix) for absolute positioning: 1 0 0 1 x y Tm */
            clen += sprintf(cbuf + clen,
                            "1 0 0 1 %d %.1f Tm\n(%s) Tj\n",
                            PDF_MARGIN, y, esc);
            y -= PDF_LINE_H;
        }

        clen += sprintf(cbuf + clen, "ET\n");

        /* Content object */
        WRITE_OBJ_START(content_obj);
        fprintf(fp, "<< /Length %d >>\n", clen);
        fprintf(fp, "stream\n");
        fwrite(cbuf, 1, clen, fp);
        fprintf(fp, "\nendstream\n");
        WRITE_OBJ_END();
        free(cbuf);

        /* Page object */
        WRITE_OBJ_START(page_obj);
        fprintf(fp,
                "<< /Type /Page /Parent 2 0 R\n"
                "   /MediaBox [0 0 %d %d]\n"
                "   /Contents %d 0 R\n"
                "   /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >>\n"
                ">>\n",
                PDF_PAGE_W, PDF_PAGE_H, content_obj);
        WRITE_OBJ_END();

        obj_count += 2;
    }

    int total_objs = 4 + total_pages * 2;

    /* xref table */
    long xref_offset = ftell(fp);
    fprintf(fp, "xref\n");
    fprintf(fp, "0 %d\n", total_objs + 1);
    fprintf(fp, "0000000000 65535 f \n");
    for (int i = 0; i < total_objs; i++)
        fprintf(fp, "%010ld 00000 n \n", offsets[i]);

    /* trailer */
    fprintf(fp, "trailer\n<< /Size %d /Root 1 0 R >>\n", total_objs + 1);
    fprintf(fp, "startxref\n%ld\n%%%%EOF\n", xref_offset);

    free(offsets);
    fclose(fp);
    printf("PDF exported to \"%s\" (%d page(s)).\n", PDF_FILE, total_pages);
}

/* =========================================================================
 * Data persistence  (plain-text format)
 * ========================================================================= */

/*
 * File format:
 *
 *   HABITS <count>
 *   <index> <active> <name>
 *   ...
 *   RECORDS <count>
 *   <date> <s0> <s1> ... <s9>
 *   ...
 */

void save_data(HabitTracker *t)
{
    FILE *fp = fopen(DATA_FILE, "w");
    if (!fp) { perror("Cannot save " DATA_FILE); return; }

    fprintf(fp, "HABITS %d\n", t->habit_count);
    for (int i = 0; i < t->habit_count; i++)
        fprintf(fp, "%d %d %s\n", i, t->habits[i].active, t->habits[i].name);

    fprintf(fp, "RECORDS %d\n", t->record_count);
    for (int r = 0; r < t->record_count; r++) {
        fprintf(fp, "%s", t->records[r].date);
        for (int i = 0; i < MAX_HABITS; i++)
            fprintf(fp, " %d", t->records[r].status[i]);
        fprintf(fp, "\n");
    }

    fclose(fp);
}

void load_data(HabitTracker *t)
{
    FILE *fp = fopen(DATA_FILE, "r");
    /* No data file means this is a first run; the tracker is already
     * fully initialised by init_tracker(), so just return. */
    if (!fp) return;

    char line[256];
    int habit_count = 0, record_count = 0;

    if (fscanf(fp, "HABITS %d\n", &habit_count) != 1) goto done;
    t->habit_count = 0;

    for (int i = 0; i < habit_count; i++) {
        int idx, active;
        char name[MAX_NAME_LEN];
        if (fscanf(fp, "%d %d ", &idx, &active) != 2) goto done;
        if (!fgets(name, sizeof(name), fp)) goto done;
        /* strip newline */
        int n = (int)strlen(name);
        while (n > 0 && (name[n-1] == '\n' || name[n-1] == '\r'))
            name[--n] = '\0';

        if (idx < MAX_HABITS) {
            snprintf(t->habits[idx].name, MAX_NAME_LEN, "%s", name);
            t->habits[idx].active = active;
            if (idx + 1 > t->habit_count)
                t->habit_count = idx + 1;
        }
    }

    if (fscanf(fp, "RECORDS %d\n", &record_count) != 1) goto done;
    t->record_count = 0;

    for (int r = 0; r < record_count && r < MAX_DAYS; r++) {
        char date[DATE_LEN];
        if (fscanf(fp, "%10s", date) != 1) goto done;
        memcpy(t->records[r].date, date, DATE_LEN);
        for (int i = 0; i < MAX_HABITS; i++) {
            int s;
            if (fscanf(fp, " %d", &s) != 1) s = -1;
            t->records[r].status[i] = s;
        }
        t->record_count++;
        if (fgets(line, sizeof(line), fp) == NULL && r < record_count - 1)
            goto done;
    }

done:
    fclose(fp);
}

/* =========================================================================
 * Main menu loop
 * ========================================================================= */

static void print_menu(void)
{
    printf("┌─────────────────────────────────────┐\n");
    printf("│        HABIT TRACKER  v1.0          │\n");
    printf("├─────────────────────────────────────┤\n");
    printf("│  1. Add habit                       │\n");
    printf("│  2. View habits                     │\n");
    printf("│  3. Mark today's habits             │\n");
    printf("│  4. View history                    │\n");
    printf("│  5. Show statistics                 │\n");
    printf("│  6. Export to CSV (Excel)           │\n");
    printf("│  7. Export to PDF                   │\n");
    printf("│  8. Quit                            │\n");
    printf("└─────────────────────────────────────┘\n");
    printf("Choice: ");
    fflush(stdout);
}

int main(void)
{
    HabitTracker tracker;
    init_tracker(&tracker);
    load_data(&tracker);

    printf("\nWelcome to Habit Tracker!\n");
    if (tracker.habit_count == 0)
        printf("(Tip: Start by adding your habits with option 1)\n\n");

    int running = 1;
    while (running) {
        print_menu();

        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            flush_stdin();
            continue;
        }
        flush_stdin();
        printf("\n");

        switch (choice) {
            case 1: add_habit(&tracker);      break;
            case 2: view_habits(&tracker);    break;
            case 3: mark_today(&tracker);     break;
            case 4: view_history(&tracker);   break;
            case 5: show_statistics(&tracker);break;
            case 6: export_csv(&tracker);     break;
            case 7: export_pdf(&tracker);     break;
            case 8: running = 0;              break;
            default: printf("Invalid choice. Please enter 1-8.\n\n"); break;
        }
    }

    printf("Goodbye!\n");
    return 0;
}
