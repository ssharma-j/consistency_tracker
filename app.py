from flask import Flask, render_template, request, redirect, url_for
import sqlite3
from datetime import date, timedelta

app = Flask(__name__)
app.secret_key = "simple-tracker-key"

DB = "tracker.db"


# ---------- DATABASE ----------

def get_db():
    return sqlite3.connect(DB)


def init_db():
    conn = get_db()
    c = conn.cursor()

    c.execute("""
        CREATE TABLE IF NOT EXISTS habits (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        )
    """)

    c.execute("""
        CREATE TABLE IF NOT EXISTS habit_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            habit_id INTEGER,
            day TEXT
        )
    """)

    c.execute("""
        CREATE TABLE IF NOT EXISTS notes (
            day TEXT PRIMARY KEY,
            content TEXT
        )
    """)

    conn.commit()
    conn.close()


init_db()

# ---------- HELPERS ----------

def get_streak():
    conn = get_db()
    c = conn.cursor()

    streak = 0
    today = date.today()

    while True:
        d = today - timedelta(days=streak)
        c.execute("SELECT COUNT(*) FROM habit_logs WHERE day=?", (d.isoformat(),))
        count = c.fetchone()[0]
        if count >= 5:
            streak += 1
        else:
            break

    conn.close()
    return streak


# ---------- ROUTES ----------

@app.route("/", methods=["GET", "POST"])
def home():
    conn = get_db()
    c = conn.cursor()

    today = date.today().isoformat()

    # Add habit
    if request.method == "POST":
        name = request.form.get("habit")
        if name:
            c.execute("INSERT INTO habits (name) VALUES (?)", (name,))
            conn.commit()
        return redirect(url_for("home"))

    # Fetch habits
    c.execute("SELECT * FROM habits")
    habits = c.fetchall()

    # Completed today
    c.execute("SELECT habit_id FROM habit_logs WHERE day=?", (today,))
    completed_ids = {row[0] for row in c.fetchall()}

    # Note
    c.execute("SELECT content FROM notes WHERE day=?", (today,))
    note_row = c.fetchone()
    today_note = note_row[0] if note_row else ""

    conn.close()

    return render_template(
        "index.html",
        habits=habits,
        completed_ids=completed_ids,
        today=today,
        today_note=today_note,
        streak=get_streak()
    )


@app.route("/mark/<int:habit_id>")
def mark(habit_id):
    conn = get_db()
    c = conn.cursor()
    today = date.today().isoformat()

    c.execute(
        "INSERT INTO habit_logs (habit_id, day) VALUES (?, ?)",
        (habit_id, today)
    )

    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/delete/<int:habit_id>")
def delete(habit_id):
    conn = get_db()
    c = conn.cursor()

    c.execute("DELETE FROM habits WHERE id=?", (habit_id,))
    c.execute("DELETE FROM habit_logs WHERE habit_id=?", (habit_id,))

    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/save_note", methods=["POST"])
def save_note():
    content = request.form.get("content", "")
    today = date.today().isoformat()

    conn = get_db()
    c = conn.cursor()

    c.execute("""
        INSERT INTO notes (day, content)
        VALUES (?, ?)
        ON CONFLICT(day) DO UPDATE SET content=excluded.content
    """, (today, content))

    conn.commit()
    conn.close()
    return redirect(url_for("home"))


if __name__ == "__main__":
    app.run(debug=True)
