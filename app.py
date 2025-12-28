from flask import Flask, render_template, request, redirect, url_for
import sqlite3
from datetime import date

app = Flask(__name__)
app.secret_key = "tracker-secret-key"

DB_NAME = "tracker.db"


# -------------------- DATABASE INIT --------------------

def get_db():
    return sqlite3.connect(DB_NAME)


def init_db():
    conn = get_db()
    c = conn.cursor()

    # Habits
    c.execute("""
        CREATE TABLE IF NOT EXISTS habits (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            difficulty TEXT NOT NULL
        )
    """)

    # Habit completion log
    c.execute("""
        CREATE TABLE IF NOT EXISTS habit_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            habit_id INTEGER,
            day DATE,
            FOREIGN KEY(habit_id) REFERENCES habits(id)
        )
    """)

    # Daily notes
    c.execute("""
        CREATE TABLE IF NOT EXISTS notes (
            day DATE PRIMARY KEY,
            content TEXT
        )
    """)

    conn.commit()
    conn.close()


init_db()

# -------------------- ROUTES --------------------

@app.route("/", methods=["GET", "POST"])
def home():
    conn = get_db()
    c = conn.cursor()

    today = date.today().isoformat()

    # Add habit
    if request.method == "POST":
        name = request.form.get("name")
        difficulty = request.form.get("difficulty")
        if name:
            c.execute(
                "INSERT INTO habits (name, difficulty) VALUES (?, ?)",
                (name, difficulty)
            )
            conn.commit()
        return redirect(url_for("home"))

    # Fetch habits
    c.execute("SELECT * FROM habits")
    habits = c.fetchall()

    # Completed habits today
    c.execute(
        "SELECT habit_id FROM habit_logs WHERE day = ?",
        (today,)
    )
    completed_ids = {row[0] for row in c.fetchall()}

    # Today's note
    c.execute("SELECT content FROM notes WHERE day = ?", (today,))
    note_row = c.fetchone()
    today_note = note_row[0] if note_row else ""

    conn.close()

    return render_template(
        "index.html",
        habits=habits,
        completed_ids=completed_ids,
        today=today,
        today_note=today_note
    )


@app.route("/mark/<int:habit_id>")
def mark_habit(habit_id):
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
def delete_habit(habit_id):
    conn = get_db()
    c = conn.cursor()

    c.execute("DELETE FROM habits WHERE id = ?", (habit_id,))
    c.execute("DELETE FROM habit_logs WHERE habit_id = ?", (habit_id,))

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


# -------------------- RUN --------------------

if __name__ == "__main__":
    app.run(debug=True)
