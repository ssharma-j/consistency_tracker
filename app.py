from flask import Flask, render_template, request, redirect, url_for, session
import sqlite3
from datetime import datetime, timedelta
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__)
app.secret_key = "change-this-secret-key"
DB_NAME = "tracker.db"


# ---------------- DATABASE INIT ----------------
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    c.execute("""
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE,
        password TEXT
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS habits (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT,
        difficulty TEXT,
        user_id INTEGER
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        habit_id INTEGER,
        date TEXT,
        status INTEGER
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS daily_notes (
        note_date TEXT,
        content TEXT,
        user_id INTEGER,
        PRIMARY KEY (note_date, user_id)
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS weekly_reflection (
        week_start TEXT,
        content TEXT,
        user_id INTEGER,
        PRIMARY KEY (week_start, user_id)
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS daily_focus (
        focus_date TEXT,
        f1 TEXT, f2 TEXT, f3 TEXT,
        user_id INTEGER,
        PRIMARY KEY (focus_date, user_id)
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS daily_energy (
        energy_date TEXT,
        level TEXT,
        user_id INTEGER,
        PRIMARY KEY (energy_date, user_id)
    )""")

    c.execute("""
    CREATE TABLE IF NOT EXISTS mvd_usage (
        used_date TEXT,
        user_id INTEGER,
        PRIMARY KEY (used_date, user_id)
    )""")

    conn.commit()
    conn.close()


# ---------------- AUTH ----------------
@app.route("/register", methods=["GET", "POST"])
def register():
    if request.method == "POST":
        username = request.form["username"]
        password = generate_password_hash(request.form["password"])

        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()
        try:
            c.execute(
                "INSERT INTO users (username, password) VALUES (?,?)",
                (username, password)
            )
            conn.commit()
        except:
            conn.close()
            return "User already exists"
        conn.close()
        return redirect(url_for("login"))

    return render_template("register.html")


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form["username"]
        password = request.form["password"]

        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()
        c.execute("SELECT id, password FROM users WHERE username=?", (username,))
        user = c.fetchone()
        conn.close()

        if user and check_password_hash(user[1], password):
            session["user_id"] = user[0]
            return redirect(url_for("home"))

        return "Invalid credentials"

    return render_template("login.html")


@app.route("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))


# ---------------- STREAK CALCULATION (FIXED) ----------------
def calculate_streaks(c, user_id):
    c.execute("""
        SELECT logs.date, COUNT(*) 
        FROM logs
        JOIN habits ON habits.id = logs.habit_id
        WHERE logs.status = 1
          AND habits.user_id = ?
        GROUP BY logs.date
        ORDER BY logs.date
    """, (user_id,))

    rows = c.fetchall()
    if not rows:
        return 0, 0

    successful_days = []

    for date_str, count in rows:
        if count >= 5:   # success condition
            successful_days.append(
                datetime.strptime(date_str, "%Y-%m-%d").date()
            )

    if not successful_days:
        return 0, 0

    best_streak = 1
    temp_streak = 1

    for i in range(1, len(successful_days)):
        if successful_days[i] == successful_days[i - 1] + timedelta(days=1):
            temp_streak += 1
            best_streak = max(best_streak, temp_streak)
        else:
            temp_streak = 1

    today = datetime.now().date()
    current_streak = temp_streak if successful_days[-1] == today else 0

    return current_streak, best_streak


# ---------------- HEATMAP ----------------
def get_heatmap(c, user_id, days=90):
    today = datetime.now().date()
    heatmap = {}

    for i in range(days):
        d = today - timedelta(days=i)
        ds = d.strftime("%Y-%m-%d")

        c.execute("""
            SELECT COUNT(*) FROM logs
            JOIN habits ON habits.id = logs.habit_id
            WHERE logs.date=? AND logs.status=1 AND habits.user_id=?
        """, (ds, user_id))
        count = c.fetchone()[0]

        c.execute("""
            SELECT content FROM daily_notes
            WHERE note_date=? AND user_id=?
        """, (ds, user_id))
        note_row = c.fetchone()
        note = note_row[0] if note_row else ""

        if count >= 5:
            color = "green"
        elif count > 0:
            color = "yellow"
        else:
            color = "red"

        heatmap[ds] = {"count": count, "color": color, "note": note}

    return heatmap


# ---------------- HOME ROUTE (THIS IS WHAT YOU COULDN'T FIND) ----------------
@app.route("/")
def home():
    if "user_id" not in session:
        return redirect(url_for("login"))

    user_id = session["user_id"]
    today = datetime.now().date()
    today_db = today.strftime("%Y-%m-%d")
    today_ui = today.strftime("%d-%m-%Y")

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    # Habits
    c.execute("SELECT id, name, difficulty FROM habits WHERE user_id=?", (user_id,))
    habits_raw = c.fetchall()

    c.execute("""
        SELECT habit_id FROM logs
        JOIN habits ON habits.id = logs.habit_id
        WHERE logs.date=? AND logs.status=1 AND habits.user_id=?
    """, (today_db, user_id))
    done = [r[0] for r in c.fetchall()]

    habits = [{
        "id": h[0],
        "name": h[1],
        "difficulty": h[2],
        "done": h[0] in done
    } for h in habits_raw]

    # Heatmap
    heatmap = get_heatmap(c, user_id)

    # Streaks (THIS FIXES YOUR ISSUE)
    current_streak, best_streak = calculate_streaks(c, user_id)

    # Daily note
    c.execute("SELECT content FROM daily_notes WHERE note_date=? AND user_id=?",
              (today_db, user_id))
    note_row = c.fetchone()
    daily_note = note_row[0] if note_row else ""

    # Focus
    c.execute("SELECT f1,f2,f3 FROM daily_focus WHERE focus_date=? AND user_id=?",
              (today_db, user_id))
    focus = c.fetchone() or ("", "", "")

    # Energy
    c.execute("SELECT level FROM daily_energy WHERE energy_date=? AND user_id=?",
              (today_db, user_id))
    energy_row = c.fetchone()
    energy = energy_row[0] if energy_row else ""

    # Weekly reflection
    show_reflection = today.weekday() == 6
    week_start = (today - timedelta(days=6)).strftime("%Y-%m-%d")
    c.execute("""
        SELECT content FROM weekly_reflection
        WHERE week_start=? AND user_id=?
    """, (week_start, user_id))
    wr = c.fetchone()
    weekly_reflection = wr[0] if wr else ""

    conn.close()

    return render_template(
        "index.html",
        today=today_ui,
        habits=habits,
        heatmap=heatmap,
        daily_note=daily_note,
        focus=focus,
        energy=energy,
        show_reflection=show_reflection,
        weekly_reflection=weekly_reflection,
        current_streak=current_streak,
        best_streak=best_streak
    )


# ---------------- ACTION ROUTES ----------------
@app.route("/add_habit", methods=["POST"])
def add_habit():
    user_id = session["user_id"]
    name = request.form["name"]
    difficulty = request.form["difficulty"]

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        INSERT INTO habits (name, difficulty, user_id)
        VALUES (?,?,?)
    """, (name, difficulty, user_id))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/delete_habit/<int:hid>", methods=["POST"])
def delete_habit(hid):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("DELETE FROM logs WHERE habit_id=?", (hid,))
    c.execute("DELETE FROM habits WHERE id=?", (hid,))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/toggle/<int:hid>")
def toggle(hid):
    today = datetime.now().strftime("%Y-%m-%d")

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("SELECT id FROM logs WHERE habit_id=? AND date=?", (hid, today))
    row = c.fetchone()

    if row:
        c.execute("DELETE FROM logs WHERE id=?", (row[0],))
    else:
        c.execute("INSERT INTO logs (habit_id,date,status) VALUES (?,?,1)", (hid, today))

    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/save_note", methods=["POST"])
def save_note():
    user_id = session["user_id"]
    today = datetime.now().strftime("%Y-%m-%d")
    content = request.form["content"]

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        REPLACE INTO daily_notes (note_date,content,user_id)
        VALUES (?,?,?)
    """, (today, content, user_id))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/save_focus", methods=["POST"])
def save_focus():
    user_id = session["user_id"]
    today = datetime.now().strftime("%Y-%m-%d")
    f1, f2, f3 = request.form["f1"], request.form["f2"], request.form["f3"]

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        REPLACE INTO daily_focus
        (focus_date,f1,f2,f3,user_id)
        VALUES (?,?,?,?,?)
    """, (today, f1, f2, f3, user_id))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/set_energy/<level>")
def set_energy(level):
    user_id = session["user_id"]
    today = datetime.now().strftime("%Y-%m-%d")

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        REPLACE INTO daily_energy (energy_date,level,user_id)
        VALUES (?,?,?)
    """, (today, level, user_id))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


@app.route("/save_weekly_reflection", methods=["POST"])
def save_weekly_reflection():
    user_id = session["user_id"]
    today = datetime.now().date()
    week_start = (today - timedelta(days=6)).strftime("%Y-%m-%d")
    content = request.form["content"]

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute("""
        REPLACE INTO weekly_reflection (week_start,content,user_id)
        VALUES (?,?,?)
    """, (week_start, content, user_id))
    conn.commit()
    conn.close()
    return redirect(url_for("home"))


# ---------------- RUN ----------------
if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000)

