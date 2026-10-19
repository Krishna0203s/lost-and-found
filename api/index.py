"""
Campus Lost & Found System — Self-Contained Serverless Function (/api/index.py)
Serves REST API endpoints (/api/*) and provides static asset fallback for Vercel Serverless.
Zero external dependencies (Python standard library only).
"""

import os
import sys
import json
import sqlite3
import datetime
import mimetypes
import re
import urllib.parse
from http.server import BaseHTTPRequestHandler

# ------------------------------------------------------------------------------
# 1. EMBEDDED SQL SCHEMA & INITIAL RECORDS
# ------------------------------------------------------------------------------
SCHEMA_SQL = """
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    student_id TEXT UNIQUE NOT NULL,
    email TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'student' CHECK(role IN ('student', 'admin')),
    password TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS locations (
    location_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS categories (
    category_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    icon TEXT DEFAULT '📦',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS items (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_type TEXT NOT NULL CHECK(item_type IN ('LOST', 'FOUND')),
    item_name TEXT NOT NULL,
    category TEXT NOT NULL,
    description TEXT NOT NULL,
    location TEXT NOT NULL,
    reported_by TEXT NOT NULL,
    student_id TEXT,
    contact_info TEXT,
    report_date TEXT NOT NULL,
    priority TEXT NOT NULL DEFAULT 'NORMAL' CHECK(priority IN ('NORMAL', 'IMPORTANT', 'VERY_IMPORTANT')),
    status TEXT NOT NULL CHECK(status IN ('LOST', 'FOUND', 'MATCHED', 'CLAIMED', 'RETURNED')),
    storage_location TEXT,
    identifying_details TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS matches (
    match_id INTEGER PRIMARY KEY AUTOINCREMENT,
    lost_item_id INTEGER NOT NULL REFERENCES items(item_id) ON DELETE CASCADE,
    found_item_id INTEGER NOT NULL REFERENCES items(item_id) ON DELETE CASCADE,
    category_score INTEGER NOT NULL,
    location_score INTEGER NOT NULL,
    description_score INTEGER NOT NULL,
    date_score INTEGER NOT NULL,
    total_score INTEGER NOT NULL,
    match_status TEXT NOT NULL DEFAULT 'POTENTIAL' CHECK(match_status IN ('POTENTIAL', 'CONFIRMED', 'DISMISSED')),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(lost_item_id, found_item_id)
);

CREATE TABLE IF NOT EXISTS claims (
    claim_id INTEGER PRIMARY KEY AUTOINCREMENT,
    found_item_id INTEGER NOT NULL REFERENCES items(item_id) ON DELETE CASCADE,
    claimant_name TEXT NOT NULL,
    claimant_id TEXT NOT NULL,
    claimant_contact TEXT NOT NULL,
    verification_response TEXT NOT NULL,
    claim_status TEXT NOT NULL DEFAULT 'PENDING' CHECK(claim_status IN ('PENDING', 'APPROVED', 'REJECTED')),
    admin_notes TEXT,
    submitted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    reviewed_at DATETIME,
    reviewed_by TEXT
);

CREATE TABLE IF NOT EXISTS returns (
    return_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL REFERENCES items(item_id) ON DELETE CASCADE,
    claim_id INTEGER REFERENCES claims(claim_id) ON DELETE SET NULL,
    claimant_id TEXT NOT NULL,
    verified_by TEXT NOT NULL,
    return_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    notes TEXT
);

INSERT OR IGNORE INTO users (user_id, name, student_id, email, role, password) VALUES
(1, 'Campus Admin', 'admin', 'admin@campus.edu', 'admin', 'admin123'),
(2, 'Student krishna', 'krishna', 'krishna@campus.edu', 'student', ''),
(3, 'Gourav', '26215121', 'gourav@campus.edu', 'student', '');

INSERT OR IGNORE INTO locations (location_id, name, description) VALUES
(1, 'Class', 'Campus Lecture Hall / Classroom');

INSERT OR IGNORE INTO categories (category_id, name, icon) VALUES
(1, 'Electronics', '💻');

INSERT OR IGNORE INTO items (item_id, item_type, item_name, category, description, location, reported_by, student_id, contact_info, report_date, priority, status) VALUES
(1, 'LOST', 'Cable', 'Electronics', 'Black USB-C cable', 'Class', 'krishna', 'krishna', 'krishna@campus.edu', '2026-10-11', 'NORMAL', 'MATCHED'),
(2, 'FOUND', 'cable', 'Electronics', 'USB-C fast charging cable found under desk', 'Class', 'Security Desk', '', 'security@campus.edu', '2026-10-11', 'NORMAL', 'MATCHED');

INSERT OR IGNORE INTO matches (match_id, lost_item_id, found_item_id, category_score, location_score, description_score, date_score, total_score, match_status) VALUES
(1, 1, 2, 40, 30, 20, 10, 100, 'POTENTIAL');
"""

# ------------------------------------------------------------------------------
# 2. PATHS & DATABASE RESOLUTION (Writable in /tmp for Serverless)
# ------------------------------------------------------------------------------
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(CURRENT_DIR)

if os.environ.get("VERCEL") or not os.access(ROOT_DIR, os.W_OK):
    DB_PATH = "/tmp/lostfound.db"
else:
    DB_PATH = os.path.join(ROOT_DIR, "lostfound.db")

def init_db_if_needed():
    global DB_PATH
    if os.environ.get("VERCEL") or not os.access(ROOT_DIR, os.W_OK):
        DB_PATH = "/tmp/lostfound.db"
    else:
        DB_PATH = os.path.join(ROOT_DIR, "lostfound.db")

    needs_init = False
    if not os.path.exists(DB_PATH) or os.path.getsize(DB_PATH) == 0:
        needs_init = True
    else:
        try:
            test_conn = sqlite3.connect(DB_PATH, timeout=3.0)
            row = test_conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='users';").fetchone()
            test_conn.close()
            if not row:
                needs_init = True
        except Exception:
            needs_init = True

    if needs_init:
        copied = False
        if DB_PATH.startswith("/tmp"):
            for candidate in [os.path.join(ROOT_DIR, "lostfound.db"), os.path.join(CURRENT_DIR, "lostfound.db"), "lostfound.db"]:
                if os.path.exists(candidate) and os.path.getsize(candidate) > 0:
                    try:
                        import shutil
                        shutil.copy2(candidate, DB_PATH)
                        copied = True
                        break
                    except Exception:
                        pass
        if not copied:
            try:
                conn = sqlite3.connect(DB_PATH)
                conn.executescript(SCHEMA_SQL)
                conn.close()
            except Exception as e:
                print(f"[DB Init Error]: {e}", file=sys.stderr)

init_db_if_needed()

def get_db():
    init_db_if_needed()
    conn = sqlite3.connect(DB_PATH, timeout=10.0, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON;")
    return conn


# ------------------------------------------------------------------------------
# 3. STATIC FILE SERVING HELPERS
# ------------------------------------------------------------------------------
def get_static_file(raw_path):
    clean = raw_path.strip("/")
    if clean.startswith("web/"):
        clean = clean[4:]
    elif clean == "web":
        clean = ""

    if not clean or clean == "index.html":
        clean = "index.html"

    # Search in public/, web/, and current directory
    search_dirs = [
        os.path.join(ROOT_DIR, "public"),
        os.path.join(ROOT_DIR, "web"),
        os.path.join(CURRENT_DIR, "public"),
        os.path.join(CURRENT_DIR, "web"),
    ]

    for sdir in search_dirs:
        target = os.path.normpath(os.path.join(sdir, clean))
        if os.path.isfile(target):
            mime, _ = mimetypes.guess_type(target)
            if not mime:
                mime = "application/octet-stream"
            if mime.startswith("text/") or "javascript" in mime or "json" in mime:
                mime += "; charset=utf-8"
            with open(target, "rb") as f:
                return f.read(), mime
    return None, None

def is_known_api_endpoint(path):
    p = path.lstrip("/")
    if p.startswith("api"):
        return True
    first_seg = p.split("/")[0]
    return first_seg in (
        "statistics", "seed", "items", "matches", "claims",
        "auth", "users", "locations", "categories", "c-info", "test-matching"
    )

def resolve_request_path(raw_path, headers, query_params):
    """
    Recovers the actual API route, accounting for:
    1. Vercel internal rewrites with forwarded query params (?__path=/api/...)
    2. Vercel edge rewrite/proxy headers (x-matched-path, x-forwarded-uri, etc.)
    3. Direct execution (local WSGI / BaseHTTPRequestHandler)
    """
    clean = (raw_path or "").strip()

    # 1. Forwarded path via query parameter from rewrite
    forwarded = query_params.get("__path") or query_params.get("path") or query_params.get("route")
    if forwarded:
        forwarded = forwarded.strip().lstrip("/")
        if forwarded and not forwarded.startswith("api"):
            forwarded = f"api/{forwarded}"
        return f"/{forwarded}" if forwarded else "/api"

    # 2. Vercel edge rewrite and reverse proxy headers
    for h in ("x-matched-path", "x-forwarded-uri", "x-invoke-path", "x-vercel-matched-path"):
        val = headers.get(h) or headers.get(h.title()) or headers.get(h.upper().replace("-", "_"))
        if val:
            parsed = urllib.parse.urlparse(val).path
            if parsed and parsed not in ("/api/index.py", "/api/index", "/index.py", "/main.py"):
                return parsed

    # 3. Direct endpoint paths
    if clean and clean not in ("/api/index.py", "/api/index", "/index.py", "/main.py", ""):
        return clean

    # 4. Fallback for root API invocation
    if clean in ("/api/index.py", "/api/index"):
        return "/api"

    return clean or "/"


# ------------------------------------------------------------------------------
# 4. MATCHING ENGINE
# ------------------------------------------------------------------------------
STOP_WORDS = {
    "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for", "of",
    "with", "by", "from", "up", "about", "into", "through", "after", "is", "was",
    "are", "were", "it", "its", "my", "me", "this", "that", "has", "have", "had",
    "item", "lost", "found"
}

def tokenize_text(text):
    if not text:
        return []
    words = re.findall(r"[a-zA-Z0-9]+", text.lower())
    tokens = []
    for w in words:
        if len(w) >= 3 and w not in STOP_WORDS and w not in tokens:
            tokens.append(w)
    return tokens

def score_category(cat1, cat2):
    if not cat1 or not cat2:
        return 0
    c1, c2 = cat1.strip().lower(), cat2.strip().lower()
    if c1 == c2:
        return 40
    if c1 in c2 or c2 in c1:
        return 30
    return 0

def score_location(loc1, loc2):
    if not loc1 or not loc2:
        return 0
    l1, l2 = loc1.strip().lower(), loc2.strip().lower()
    if l1 == l2:
        return 30
    if l1 in l2 or l2 in l1:
        return 20
    t1 = tokenize_text(loc1)
    t2 = tokenize_text(loc2)
    for w1 in t1:
        if w1 in t2:
            return 20
    return 0

def score_description(desc1, desc2):
    if not desc1 or not desc2:
        return 0
    d1, d2 = desc1.strip().lower(), desc2.strip().lower()
    if d1 == d2:
        return 20
    t1 = tokenize_text(desc1)
    t2 = tokenize_text(desc2)
    if not t1 or not t2:
        return 0
    matches = 0
    for w1 in t1:
        for w2 in t2:
            if w1 == w2 or (len(w1) > 4 and len(w2) > 4 and (w1 in w2 or w2 in w1)):
                matches += 1
                break
    if matches >= 3:
        return 20
    if matches == 2:
        return 15
    if matches == 1:
        return 10
    return 0

def score_date(date1, date2):
    if not date1 or not date2:
        return 0
    if date1 == date2:
        return 10
    try:
        d1 = datetime.datetime.strptime(date1[:10], "%Y-%m-%d")
        d2 = datetime.datetime.strptime(date2[:10], "%Y-%m-%d")
        diff = abs((d1 - d2).total_seconds()) / 86400.0
        if diff <= 2.0:
            return 5
        if diff <= 5.0:
            return 3
    except Exception:
        pass
    return 0

def calculate_match_score(lost, found):
    cat_s = score_category(lost.get("category", ""), found.get("category", ""))
    loc_s = score_location(lost.get("location", ""), found.get("location", ""))
    full_lost = f"{lost.get('item_name', '')} {lost.get('description', '')}"
    full_found = f"{found.get('item_name', '')} {found.get('description', '')}"
    desc_s = score_description(full_lost, full_found)
    date_s = score_date(lost.get("report_date", ""), found.get("report_date", ""))
    total = min(100, cat_s + loc_s + desc_s + date_s)
    is_match = total >= 70
    return {
        "lost_item_id": lost.get("item_id"),
        "found_item_id": found.get("item_id"),
        "category_score": cat_s,
        "location_score": loc_s,
        "description_score": desc_s,
        "date_score": date_s,
        "total_score": total,
        "is_possible_match": bool(is_match),
        "match_status": "POTENTIAL" if is_match else "DISMISSED"
    }

def evaluate_matches_for_item(conn, item_id):
    cur = conn.cursor()
    cur.execute("SELECT * FROM items WHERE item_id = ?", (item_id,))
    target = cur.fetchone()
    if not target:
        return
    target_dict = dict(target)
    opposite_type = "FOUND" if target_dict["item_type"] == "LOST" else "LOST"
    cur.execute("SELECT * FROM items WHERE item_type = ? AND status != 'RETURNED'", (opposite_type,))
    candidates = [dict(r) for r in cur.fetchall()]

    for c in candidates:
        lost = target_dict if target_dict["item_type"] == "LOST" else c
        found = c if target_dict["item_type"] == "LOST" else target_dict
        res = calculate_match_score(lost, found)
        cur.execute("""
            INSERT INTO matches (
                lost_item_id, found_item_id, category_score, location_score,
                description_score, date_score, total_score, match_status
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(lost_item_id, found_item_id) DO UPDATE SET
                category_score=excluded.category_score,
                location_score=excluded.location_score,
                description_score=excluded.description_score,
                date_score=excluded.date_score,
                total_score=excluded.total_score,
                match_status=excluded.match_status;
        """, (
            lost["item_id"], found["item_id"],
            res["category_score"], res["location_score"],
            res["description_score"], res["date_score"],
            res["total_score"], res["match_status"]
        ))
    conn.commit()

# ------------------------------------------------------------------------------
# 5. AUTHENTICATION & ACCESS CONTROL
# ------------------------------------------------------------------------------
def get_auth_user(headers, conn):
    auth_header = headers.get("Authorization") or headers.get("authorization") or ""
    if auth_header.lower().startswith("bearer "):
        auth_header = auth_header[7:].strip()
    if not auth_header:
        return None

    parts = auth_header.split(":")
    identifier = ""
    if len(parts) >= 2:
        identifier = parts[1]
    elif len(parts) == 1:
        identifier = parts[0]

    if not identifier:
        return None

    cur = conn.cursor()
    cur.execute("SELECT * FROM users WHERE LOWER(student_id) = LOWER(?) OR LOWER(email) = LOWER(?) OR LOWER(name) = LOWER(?)", (identifier, identifier, identifier))
    row = cur.fetchone()
    if row:
        return dict(row)
    return None

def is_admin(user):
    return user is not None and str(user.get("role", "")).lower() == "admin"

def serialize_item(item_dict, include_secrets=True):
    d = dict(item_dict)
    if not include_secrets:
        d.pop("identifying_details", None)
    return d

def serialize_match(match_row, lost_row, found_row, include_secrets=True):
    m = dict(match_row)
    m["is_possible_match"] = bool(m.get("total_score", 0) >= 70)
    m["lost_item"] = serialize_item(lost_row, include_secrets) if lost_row else None
    m["found_item"] = serialize_item(found_row, include_secrets) if found_row else None
    return m

# ------------------------------------------------------------------------------
# 6. REST API ROUTER
# ------------------------------------------------------------------------------
def handle_api(method, raw_path, query_params, body, headers):
    path = raw_path
    if not path.startswith("/api"):
        path = "/api" + ("" if path.startswith("/") else "/") + path

    conn = get_db()
    auth_user = get_auth_user(headers, conn)
    user_is_admin = is_admin(auth_user)

    # 0. GET /api or /api/ (Operational Status & Health Check)
    if method == "GET" and path in ("/api", "/api/"):
        return 200, {
            "success": True,
            "message": "Campus Lost & Found Serverless API is operational",
            "version": "1.0",
            "status": "healthy",
            "endpoints": [
                "/api/statistics",
                "/api/items/lost",
                "/api/items/found",
                "/api/items/search",
                "/api/matches",
                "/api/claims",
                "/api/auth/login",
                "/api/users",
                "/api/locations",
                "/api/categories",
                "/api/c-info"
            ]
        }

    # 1. GET /api/statistics
    if method == "GET" and path == "/api/statistics":
        cur = conn.cursor()
        total_lost = cur.execute("SELECT COUNT(*) FROM items WHERE item_type = 'LOST'").fetchone()[0]
        total_found = cur.execute("SELECT COUNT(*) FROM items WHERE item_type = 'FOUND'").fetchone()[0]
        possible_matches = cur.execute("SELECT COUNT(*) FROM matches WHERE total_score >= 70").fetchone()[0]
        claimed = cur.execute("SELECT COUNT(*) FROM items WHERE status = 'CLAIMED'").fetchone()[0]
        returned = cur.execute("SELECT COUNT(*) FROM items WHERE status = 'RETURNED'").fetchone()[0]
        still_lost = cur.execute("SELECT COUNT(*) FROM items WHERE item_type = 'LOST' AND status NOT IN ('CLAIMED', 'RETURNED')").fetchone()[0]

        top_cat = cur.execute("SELECT category FROM items GROUP BY category ORDER BY count(*) DESC LIMIT 1").fetchone()
        most_common_category = top_cat[0] if top_cat else "None"

        top_loc = cur.execute("SELECT location FROM items GROUP BY location ORDER BY count(*) DESC LIMIT 1").fetchone()
        most_common_location = top_loc[0] if top_loc else "None"

        recovery_rate = round((returned / total_lost * 100.0), 1) if total_lost > 0 else 0.0

        return 200, {
            "success": True,
            "data": {
                "total_lost": total_lost,
                "total_found": total_found,
                "possible_matches": possible_matches,
                "claimed": claimed,
                "returned": returned,
                "still_lost": still_lost,
                "most_common_category": most_common_category,
                "most_common_location": most_common_location,
                "recovery_rate": recovery_rate
            }
        }

    # 2. POST /api/seed
    if method == "POST" and path == "/api/seed":
        if not user_is_admin:
            return 403, {"success": False, "error": "Access Denied: Administrator privileges required to reseed database"}
        conn.executescript(SCHEMA_SQL)
        items = [dict(r) for r in conn.execute("SELECT * FROM items").fetchall()]
        for item in items:
            evaluate_matches_for_item(conn, item["item_id"])
        return 200, {"success": True, "message": "Database successfully seeded with realistic campus data"}

    # 3. GET /api/items/lost
    if method == "GET" and path == "/api/items/lost":
        cur = conn.cursor()
        cur.execute("SELECT * FROM items WHERE item_type = 'LOST' ORDER BY item_id DESC")
        rows = [serialize_item(dict(r), user_is_admin) for r in cur.fetchall()]
        return 200, {"success": True, "count": len(rows), "data": rows}

    # 4. GET /api/items/found
    if method == "GET" and path == "/api/items/found":
        cur = conn.cursor()
        cur.execute("SELECT * FROM items WHERE item_type = 'FOUND' ORDER BY item_id DESC")
        rows = [serialize_item(dict(r), user_is_admin) for r in cur.fetchall()]
        return 200, {"success": True, "count": len(rows), "data": rows}

    # 5. POST /api/items/lost
    if method == "POST" and path == "/api/items/lost":
        data = body or {}
        item_name = (data.get("item_name") or "").strip()
        category = (data.get("category") or "").strip()
        location = (data.get("location") or "").strip()
        description = (data.get("description") or "").strip()
        reported_by = (data.get("reported_by") or "").strip()
        report_date = (data.get("report_date") or "").strip()
        priority = (data.get("priority") or "NORMAL").strip()
        student_id = (data.get("student_id") or "").strip()
        contact_info = (data.get("contact_info") or "").strip()
        storage_location = (data.get("storage_location") or "").strip()
        identifying_details = (data.get("identifying_details") or "").strip()

        if not item_name or not category or not location or not reported_by:
            return 400, {"success": False, "error": "Missing required fields (item_name, category, location, reported_by)"}
        if not report_date:
            report_date = datetime.date.today().isoformat()

        cur = conn.cursor()
        cur.execute("""
            INSERT INTO items (
                item_type, item_name, category, description, location,
                reported_by, student_id, contact_info, report_date, priority,
                status, storage_location, identifying_details
            ) VALUES ('LOST', ?, ?, ?, ?, ?, ?, ?, ?, ?, 'LOST', ?, ?)
        """, (
            item_name, category, description, location,
            reported_by, student_id, contact_info, report_date, priority,
            storage_location, identifying_details
        ))
        conn.commit()
        new_id = cur.lastrowid
        evaluate_matches_for_item(conn, new_id)
        cur.execute("SELECT * FROM items WHERE item_id = ?", (new_id,))
        created = serialize_item(dict(cur.fetchone()), True)
        return 201, {"success": True, "message": "Lost item reported successfully", "item": created}

    # 6. POST /api/items/found
    if method == "POST" and path == "/api/items/found":
        data = body or {}
        item_name = (data.get("item_name") or "").strip()
        category = (data.get("category") or "").strip()
        location = (data.get("location") or "").strip()
        description = (data.get("description") or "").strip()
        reported_by = (data.get("reported_by") or "").strip()
        report_date = (data.get("report_date") or "").strip()
        priority = (data.get("priority") or "NORMAL").strip()
        student_id = (data.get("student_id") or "").strip()
        contact_info = (data.get("contact_info") or "").strip()
        storage_location = (data.get("storage_location") or "").strip()
        identifying_details = (data.get("identifying_details") or "").strip()

        if not item_name or not category or not location or not reported_by:
            return 400, {"success": False, "error": "Missing required fields (item_name, category, location, reported_by)"}
        if not report_date:
            report_date = datetime.date.today().isoformat()

        cur = conn.cursor()
        cur.execute("""
            INSERT INTO items (
                item_type, item_name, category, description, location,
                reported_by, student_id, contact_info, report_date, priority,
                status, storage_location, identifying_details
            ) VALUES ('FOUND', ?, ?, ?, ?, ?, ?, ?, ?, ?, 'FOUND', ?, ?)
        """, (
            item_name, category, description, location,
            reported_by, student_id, contact_info, report_date, priority,
            storage_location, identifying_details
        ))
        conn.commit()
        new_id = cur.lastrowid
        evaluate_matches_for_item(conn, new_id)
        cur.execute("SELECT * FROM items WHERE item_id = ?", (new_id,))
        created = serialize_item(dict(cur.fetchone()), True)
        return 201, {"success": True, "message": "Found item reported successfully", "item": created}

    # 7. GET /api/items/search
    if method == "GET" and path == "/api/items/search":
        clauses = []
        params = []
        q = query_params.get("query", "").strip()
        cat = query_params.get("category", "").strip()
        loc = query_params.get("location", "").strip()
        status = query_params.get("status", "").strip()
        itype = query_params.get("type", "").strip()
        prio = query_params.get("priority", "").strip()
        stu = query_params.get("student_id", "").strip()

        if q:
            clauses.append("(item_name LIKE ? OR description LIKE ? OR location LIKE ?)")
            pat = f"%{q}%"
            params.extend([pat, pat, pat])
        if cat:
            clauses.append("category = ?")
            params.append(cat)
        if loc:
            clauses.append("location = ?")
            params.append(loc)
        if status:
            clauses.append("status = ?")
            params.append(status)
        if itype and itype != "ALL":
            clauses.append("item_type = ?")
            params.append(itype)
        if prio:
            clauses.append("priority = ?")
            params.append(prio)
        if stu:
            clauses.append("student_id = ?")
            params.append(stu)

        where_sql = f"WHERE {' AND '.join(clauses)}" if clauses else ""
        cur = conn.cursor()
        cur.execute(f"SELECT * FROM items {where_sql} ORDER BY item_id DESC", params)
        rows = [serialize_item(dict(r), user_is_admin) for r in cur.fetchall()]
        return 200, {"success": True, "count": len(rows), "data": rows}

    # 8. Matches: GET /api/matches & POST /api/matches
    if path == "/api/matches":
        if method == "GET":
            cur = conn.cursor()
            cur.execute("""
                SELECT m.*,
                       l.item_id as l_id, l.item_name as l_name, l.category as l_cat, l.description as l_desc,
                       l.location as l_loc, l.reported_by as l_rep, l.student_id as l_stu, l.contact_info as l_con,
                       l.report_date as l_date, l.priority as l_prio, l.status as l_stat, l.storage_location as l_stor,
                       l.identifying_details as l_ident, l.created_at as l_created, l.updated_at as l_updated,
                       f.item_id as f_id, f.item_name as f_name, f.category as f_cat, f.description as f_desc,
                       f.location as f_loc, f.reported_by as f_rep, f.student_id as f_stu, f.contact_info as f_con,
                       f.report_date as f_date, f.priority as f_prio, f.status as f_stat, f.storage_location as f_stor,
                       f.identifying_details as f_ident, f.created_at as f_created, f.updated_at as f_updated
                FROM matches m
                JOIN items l ON m.lost_item_id = l.item_id
                JOIN items f ON m.found_item_id = f.item_id
                WHERE m.total_score >= 70
                ORDER BY m.total_score DESC LIMIT 50
            """)
            out = []
            for r in cur.fetchall():
                row_dict = dict(r)
                lost_item = {
                    "item_id": row_dict["l_id"], "item_type": "LOST", "item_name": row_dict["l_name"],
                    "category": row_dict["l_cat"], "description": row_dict["l_desc"], "location": row_dict["l_loc"],
                    "reported_by": row_dict["l_rep"], "student_id": row_dict["l_stu"], "contact_info": row_dict["l_con"],
                    "report_date": row_dict["l_date"], "priority": row_dict["l_prio"], "status": row_dict["l_stat"],
                    "storage_location": row_dict["l_stor"], "identifying_details": row_dict["l_ident"],
                    "created_at": row_dict["l_created"], "updated_at": row_dict["l_updated"]
                }
                found_item = {
                    "item_id": row_dict["f_id"], "item_type": "FOUND", "item_name": row_dict["f_name"],
                    "category": row_dict["f_cat"], "description": row_dict["f_desc"], "location": row_dict["f_loc"],
                    "reported_by": row_dict["f_rep"], "student_id": row_dict["f_stu"], "contact_info": row_dict["f_con"],
                    "report_date": row_dict["f_date"], "priority": row_dict["f_prio"], "status": row_dict["f_stat"],
                    "storage_location": row_dict["f_stor"], "identifying_details": row_dict["f_ident"],
                    "created_at": row_dict["f_created"], "updated_at": row_dict["f_updated"]
                }
                out.append(serialize_match(row_dict, lost_item, found_item, user_is_admin))
            return 200, {"success": True, "count": len(out), "matches": out}

        elif method == "POST":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Administrator privileges required to evaluate matches"}
            cur = conn.cursor()
            cur.execute("SELECT * FROM items WHERE item_type = 'LOST'")
            losts = [dict(r) for r in cur.fetchall()]
            cur.execute("SELECT * FROM items WHERE item_type = 'FOUND'")
            founds = [dict(r) for r in cur.fetchall()]
            strong = 0
            for l in losts:
                for f in founds:
                    score = calculate_match_score(l, f)
                    if score["is_possible_match"]:
                        strong += 1
                    cur.execute("""
                        INSERT INTO matches (
                            lost_item_id, found_item_id, category_score, location_score,
                            description_score, date_score, total_score, match_status
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                        ON CONFLICT(lost_item_id, found_item_id) DO UPDATE SET
                            category_score=excluded.category_score,
                            location_score=excluded.location_score,
                            description_score=excluded.description_score,
                            date_score=excluded.date_score,
                            total_score=excluded.total_score,
                            match_status=excluded.match_status;
                    """, (
                        l["item_id"], f["item_id"],
                        score["category_score"], score["location_score"],
                        score["description_score"], score["date_score"],
                        score["total_score"], score["match_status"]
                    ))
            conn.commit()
            return 200, {"success": True, "strong_matches_count": strong, "message": "Matching engine evaluation completed across all records"}

    # 9. Item Sub-routes: /api/items/{id}...
    if path.startswith("/api/items/"):
        sub = path[11:]
        parts = sub.split("/")
        try:
            item_id = int(parts[0])
        except ValueError:
            return 400, {"success": False, "error": "Invalid item ID"}

        cur = conn.cursor()
        cur.execute("SELECT * FROM items WHERE item_id = ?", (item_id,))
        item_row = cur.fetchone()
        if not item_row:
            return 404, {"success": False, "error": "Item not found"}
        item_dict = dict(item_row)
        is_owner = auth_user and auth_user.get("student_id") and (auth_user["student_id"].lower() == (item_dict.get("student_id") or "").lower())

        if len(parts) == 1:
            if method == "GET":
                return 200, {"success": True, "item": serialize_item(item_dict, user_is_admin or is_owner)}

            elif method == "DELETE":
                if not auth_user:
                    return 401, {"success": False, "error": "Authentication required to delete item"}
                if not user_is_admin and not is_owner:
                    return 403, {"success": False, "error": "Access Denied: Administrator or owner privileges required"}
                cur.execute("DELETE FROM items WHERE item_id = ?", (item_id,))
                conn.commit()
                return 200, {"success": True, "message": "Item deleted successfully"}

            elif method == "PATCH":
                if not auth_user:
                    return 401, {"success": False, "error": "Authentication required to update item"}
                if not user_is_admin and not is_owner:
                    return 403, {"success": False, "error": "Access Denied: Administrator or owner privileges required"}
                new_status = (body or {}).get("status", "").strip()
                if not new_status:
                    return 400, {"success": False, "error": "Status is required"}
                cur.execute("UPDATE items SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE item_id = ?", (new_status, item_id))
                conn.commit()
                cur.execute("SELECT * FROM items WHERE item_id = ?", (item_id,))
                updated = serialize_item(dict(cur.fetchone()), user_is_admin or is_owner)
                return 200, {"success": True, "message": "Item updated successfully", "item": updated}

        elif len(parts) == 2 and parts[1] == "matches" and method == "GET":
            cur.execute("""
                SELECT m.*,
                       l.item_id as l_id, l.item_name as l_name, l.category as l_cat, l.description as l_desc,
                       l.location as l_loc, l.reported_by as l_rep, l.student_id as l_stu, l.contact_info as l_con,
                       l.report_date as l_date, l.priority as l_prio, l.status as l_stat, l.storage_location as l_stor,
                       l.identifying_details as l_ident, l.created_at as l_created, l.updated_at as l_updated,
                       f.item_id as f_id, f.item_name as f_name, f.category as f_cat, f.description as f_desc,
                       f.location as f_loc, f.reported_by as f_rep, f.student_id as f_stu, f.contact_info as f_con,
                       f.report_date as f_date, f.priority as f_prio, f.status as f_stat, f.storage_location as f_stor,
                       f.identifying_details as f_ident, f.created_at as f_created, f.updated_at as f_updated
                FROM matches m
                JOIN items l ON m.lost_item_id = l.item_id
                JOIN items f ON m.found_item_id = f.item_id
                WHERE (m.lost_item_id = ? OR m.found_item_id = ?) AND m.total_score >= 70
                ORDER BY m.total_score DESC
            """, (item_id, item_id))
            out = []
            for r in cur.fetchall():
                row_dict = dict(r)
                lost_item = {"item_id": row_dict["l_id"], "item_type": "LOST", "item_name": row_dict["l_name"], "category": row_dict["l_cat"], "description": row_dict["l_desc"], "location": row_dict["l_loc"], "reported_by": row_dict["l_rep"], "student_id": row_dict["l_stu"], "contact_info": row_dict["l_con"], "report_date": row_dict["l_date"], "priority": row_dict["l_prio"], "status": row_dict["l_stat"], "storage_location": row_dict["l_stor"], "identifying_details": row_dict["l_ident"], "created_at": row_dict["l_created"], "updated_at": row_dict["l_updated"]}
                found_item = {"item_id": row_dict["f_id"], "item_type": "FOUND", "item_name": row_dict["f_name"], "category": row_dict["f_cat"], "description": row_dict["f_desc"], "location": row_dict["f_loc"], "reported_by": row_dict["f_rep"], "student_id": row_dict["f_stu"], "contact_info": row_dict["f_con"], "report_date": row_dict["f_date"], "priority": row_dict["f_prio"], "status": row_dict["f_stat"], "storage_location": row_dict["f_stor"], "identifying_details": row_dict["f_ident"], "created_at": row_dict["f_created"], "updated_at": row_dict["f_updated"]}
                out.append(serialize_match(row_dict, lost_item, found_item, user_is_admin))
            return 200, {"success": True, "count": len(out), "matches": out}

        elif len(parts) == 2 and parts[1] == "return" and method == "POST":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Administrator privileges required"}
            data = body or {}
            claim_id = data.get("claim_id") or None
            claimant_id = data.get("claimant_id", "Student")
            verified_by = data.get("verified_by", "Campus Administrator")
            notes = data.get("notes", "Returned at campus lost & found desk")

            cur.execute("UPDATE items SET status = 'RETURNED', updated_at = CURRENT_TIMESTAMP WHERE item_id = ?", (item_id,))
            cur.execute("""
                INSERT INTO returns (item_id, claim_id, claimant_id, verified_by, notes)
                VALUES (?, ?, ?, ?, ?)
            """, (item_id, claim_id, claimant_id, verified_by, notes))
            conn.commit()
            return 200, {"success": True, "message": "Item successfully marked as RETURNED to rightful owner"}

    # 10. Claims: POST /api/claims, GET /api/claims
    if path == "/api/claims":
        if method == "POST":
            data = body or {}
            found_item_id = data.get("found_item_id")
            claimant_name = (data.get("claimant_name") or "").strip()
            claimant_id = (data.get("claimant_id") or "").strip()
            claimant_contact = (data.get("claimant_contact") or "").strip()
            verification_response = (data.get("verification_response") or "").strip()

            if not found_item_id or not claimant_name or not claimant_id or not claimant_contact:
                return 400, {"success": False, "error": "Missing required claim fields"}

            cur = conn.cursor()
            cur.execute("SELECT * FROM items WHERE item_id = ?", (found_item_id,))
            item = cur.fetchone()
            if not item:
                return 404, {"success": False, "error": "Found item record not found in database"}
            if item["item_type"] != "FOUND":
                return 400, {"success": False, "error": "Claims can only be filed on found items"}
            if item["status"] == "RETURNED":
                return 400, {"success": False, "error": "This item has already been returned"}

            cur.execute("""
                INSERT INTO claims (
                    found_item_id, claimant_name, claimant_id, claimant_contact,
                    verification_response, claim_status
                ) VALUES (?, ?, ?, ?, ?, 'PENDING')
            """, (found_item_id, claimant_name, claimant_id, claimant_contact, verification_response))
            conn.commit()
            new_claim_id = cur.lastrowid
            cur.execute("SELECT * FROM claims WHERE claim_id = ?", (new_claim_id,))
            claim_row = dict(cur.fetchone())
            claim_row["found_item"] = serialize_item(dict(item), True)
            return 201, {"success": True, "message": "Ownership claim submitted and placed in verification queue", "claim": claim_row}

        elif method == "GET":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Administrator privileges required to access claims"}
            cur = conn.cursor()
            cur.execute("""
                SELECT c.*, i.item_name, i.category, i.location, i.status as item_status
                FROM claims c
                JOIN items i ON c.found_item_id = i.item_id
                ORDER BY c.claim_id DESC
            """)
            claims = [dict(r) for r in cur.fetchall()]
            return 200, {"success": True, "count": len(claims), "claims": claims}

    # 11. Claims Sub-routes: GET /api/claims/{id}, PATCH /api/claims/{id}
    if path.startswith("/api/claims/"):
        try:
            claim_id = int(path[12:])
        except ValueError:
            return 400, {"success": False, "error": "Invalid claim ID"}

        cur = conn.cursor()
        cur.execute("SELECT * FROM claims WHERE claim_id = ?", (claim_id,))
        claim_row = cur.fetchone()
        if not claim_row:
            return 404, {"success": False, "error": "Claim not found"}
        claim_dict = dict(claim_row)

        if method == "GET":
            is_claimant = auth_user and (auth_user.get("student_id") or "").lower() == (claim_dict.get("claimant_id") or "").lower()
            if not user_is_admin and not is_claimant:
                return 403, {"success": False, "error": "Access Denied"}
            return 200, {"success": True, "claim": claim_dict}

        elif method == "PATCH":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Administrator privileges required"}
            data = body or {}
            status = data.get("status", "APPROVED")
            reviewer = data.get("reviewed_by", "Campus Admin")
            admin_notes = data.get("admin_notes", "")

            cur.execute("""
                UPDATE claims
                SET claim_status = ?, reviewed_by = ?, admin_notes = ?, reviewed_at = CURRENT_TIMESTAMP
                WHERE claim_id = ?
            """, (status, reviewer, admin_notes, claim_id))
            if status == "APPROVED":
                cur.execute("UPDATE items SET status = 'CLAIMED' WHERE item_id = ?", (claim_dict["found_item_id"],))
            conn.commit()
            cur.execute("SELECT * FROM claims WHERE claim_id = ?", (claim_id,))
            updated = dict(cur.fetchone())
            return 200, {"success": True, "message": "Claim review updated successfully", "claim": updated}

    # 12. User Authentication: POST /api/auth/login
    if method == "POST" and path == "/api/auth/login":
        data = body or {}
        ident = (data.get("identifier") or data.get("student_id") or data.get("email") or data.get("username") or "").strip()
        pwd = (data.get("password") or data.get("pass") or "").strip()

        if not ident:
            return 400, {"success": False, "error": "Identifier (student ID, email, or username) is required"}

        cur = conn.cursor()
        cur.execute("SELECT * FROM users WHERE LOWER(student_id) = LOWER(?) OR LOWER(email) = LOWER(?) OR LOWER(name) = LOWER(?)", (ident, ident, ident))
        row = cur.fetchone()
        if not row:
            return 401, {"success": False, "error": "Invalid credentials: No account found"}

        user_dict = dict(row)
        expected_pwd = user_dict.get("password") or ""
        if expected_pwd and expected_pwd != pwd:
            return 401, {"success": False, "error": "Invalid credentials: Incorrect password"}

        user_dict.pop("password", None)
        return 200, {"success": True, "message": "User authenticated successfully", "user": user_dict}

    # 13. Users CRUD: GET /api/users, POST /api/users, DELETE /api/users/{id}
    if path == "/api/users":
        if method == "GET":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Admin required"}
            cur = conn.cursor()
            cur.execute("SELECT user_id, name, student_id, email, role, created_at FROM users ORDER BY user_id DESC")
            users = [dict(r) for r in cur.fetchall()]
            return 200, {"success": True, "count": len(users), "users": users}

        elif method == "POST":
            data = body or {}
            name = (data.get("name") or "").strip()
            student_id = (data.get("student_id") or "").strip()
            email = (data.get("email") or "").strip()
            role = (data.get("role") or "student").strip()
            password = (data.get("password") or data.get("pass") or "").strip()

            if not name or not student_id or not email:
                return 400, {"success": False, "error": "Name, student ID, and email are required"}
            if role == "admin" and not user_is_admin:
                return 403, {"success": False, "error": "Access Denied: Admin privileges required to create admin accounts"}

            cur = conn.cursor()
            try:
                cur.execute("INSERT INTO users (name, student_id, email, role, password) VALUES (?, ?, ?, ?, ?)",
                            (name, student_id, email, role, password))
                conn.commit()
                return 201, {"success": True, "user_id": cur.lastrowid, "message": "User registered successfully"}
            except sqlite3.IntegrityError:
                return 400, {"success": False, "error": "Student ID or Email already exists"}

    if path.startswith("/api/users/") and method == "DELETE":
        if not user_is_admin:
            return 403, {"success": False, "error": "Access Denied"}
        try:
            uid = int(path[11:])
        except ValueError:
            return 400, {"success": False, "error": "Invalid user ID"}
        cur = conn.cursor()
        cur.execute("DELETE FROM users WHERE user_id = ?", (uid,))
        conn.commit()
        return 200, {"success": True, "message": "User removed successfully"}

    # 14. Locations: GET /api/locations, POST /api/locations, DELETE /api/locations/{id}
    if path == "/api/locations":
        if method == "GET":
            cur = conn.cursor()
            cur.execute("SELECT * FROM locations ORDER BY name ASC")
            locs = [dict(r) for r in cur.fetchall()]
            return 200, {"success": True, "count": len(locs), "locations": locs}

        elif method == "POST":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied"}
            name = (body or {}).get("name", "").strip()
            desc = (body or {}).get("description", "").strip()
            if not name:
                return 400, {"success": False, "error": "Location name is required"}
            cur = conn.cursor()
            try:
                cur.execute("INSERT INTO locations (name, description) VALUES (?, ?)", (name, desc))
                conn.commit()
                return 201, {"success": True, "location_id": cur.lastrowid, "message": "Campus location added successfully"}
            except sqlite3.IntegrityError:
                return 400, {"success": False, "error": "Location name already exists"}

    if path.startswith("/api/locations/") and method == "DELETE":
        if not user_is_admin:
            return 403, {"success": False, "error": "Access Denied"}
        try:
            lid = int(path[15:])
        except ValueError:
            return 400, {"success": False, "error": "Invalid location ID"}
        cur = conn.cursor()
        cur.execute("DELETE FROM locations WHERE location_id = ?", (lid,))
        conn.commit()
        return 200, {"success": True, "message": "Campus location removed successfully"}

    # 15. Categories: GET /api/categories, POST /api/categories, DELETE /api/categories/{id}
    if path == "/api/categories":
        if method == "GET":
            cur = conn.cursor()
            cur.execute("SELECT * FROM categories ORDER BY name ASC")
            cats = [dict(r) for r in cur.fetchall()]
            return 200, {"success": True, "count": len(cats), "categories": cats}

        elif method == "POST":
            if not user_is_admin:
                return 403, {"success": False, "error": "Access Denied"}
            name = (body or {}).get("name", "").strip()
            icon = (body or {}).get("icon", "📦").strip() or "📦"
            if not name:
                return 400, {"success": False, "error": "Category name is required"}
            cur = conn.cursor()
            try:
                cur.execute("INSERT INTO categories (name, icon) VALUES (?, ?)", (name, icon))
                conn.commit()
                return 201, {"success": True, "category_id": cur.lastrowid, "message": "Category added successfully"}
            except sqlite3.IntegrityError:
                return 400, {"success": False, "error": "Category name already exists"}

    if path.startswith("/api/categories/") and method == "DELETE":
        if not user_is_admin:
            return 403, {"success": False, "error": "Access Denied"}
        try:
            cid = int(path[16:])
        except ValueError:
            return 400, {"success": False, "error": "Invalid category ID"}
        cur = conn.cursor()
        cur.execute("DELETE FROM categories WHERE category_id = ?", (cid,))
        conn.commit()
        return 200, {"success": True, "message": "Category removed successfully"}

    # 16. Telemetry: GET /api/c-info, POST /api/test-matching
    if method == "GET" and path == "/api/c-info":
        return 200, {
            "success": True,
            "data": {
                "compiler": "Python 3 Serverless Engine + C99 Core Integration",
                "c_standard": "ISO/IEC 9899:1999 (C99 Compatible)",
                "platform": "Cloud Serverless (Vercel) / Linux POSIX",
                "concurrency": "Multi-threaded Worker Pool / Serverless Event Loop",
                "networking": "HTTP/1.1 REST Serverless Dispatcher",
                "database_engine": f"SQLite {sqlite3.sqlite_version}",
                "matching_engine": {
                    "max_category_score": 40,
                    "max_location_score": 30,
                    "max_description_score": 20,
                    "max_date_score": 10,
                    "match_threshold": 70,
                    "formula": "Total = S_category(40) + S_location(30) + S_description(20) + S_date(10)"
                }
            }
        }

    if method == "POST" and path == "/api/test-matching":
        data = body or {}
        lost = data.get("lost")
        found = data.get("found")
        if not lost or not found:
            return 400, {"success": False, "error": "Both 'lost' and 'found' objects are required"}
        score = calculate_match_score(lost, found)
        return 200, {"success": True, "data": score}

    return 404, {"success": False, "error": f"Endpoint not found: {method} {raw_path}"}

HTTP_STATUS_PHRASES = {
    200: "200 OK",
    201: "201 Created",
    204: "204 No Content",
    400: "400 Bad Request",
    401: "401 Unauthorized",
    403: "403 Forbidden",
    404: "404 Not Found",
    409: "409 Conflict",
    500: "500 Internal Server Error"
}

# ------------------------------------------------------------------------------
# 7. WSGI COMPATIBLE ENTRYPOINT (`app`)
# ------------------------------------------------------------------------------
def app(environ, start_response):
    raw_path = environ.get("PATH_INFO", "/")
    method = environ.get("REQUEST_METHOD", "GET").upper()
    query_string = environ.get("QUERY_STRING", "")
    query_params = {k: v[0] for k, v in urllib.parse.parse_qs(query_string).items()}

    headers = {}
    for key, val in environ.items():
        if key.startswith("HTTP_"):
            clean_k = key[5:].replace("_", "-")
            headers[clean_k.title()] = val
            headers[clean_k.lower()] = val
        elif key in ("CONTENT_TYPE", "CONTENT_LENGTH"):
            clean_k = key.replace("_", "-")
            headers[clean_k.title()] = val
            headers[clean_k.lower()] = val

    if method == "OPTIONS":
        start_response("204 No Content", [
            ("Access-Control-Allow-Origin", "*"),
            ("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS"),
            ("Access-Control-Allow-Headers", "Content-Type, Authorization")
        ])
        return [b""]

    # Resolve target API route (recovering path from rewrites/headers if needed)
    path = resolve_request_path(raw_path, headers, query_params)

    # If this is an API route, process via API router
    if is_known_api_endpoint(path):
        body = None
        try:
            content_length = int(environ.get("CONTENT_LENGTH", 0) or 0)
        except ValueError:
            content_length = 0

        if content_length > 0:
            raw_body = environ["wsgi.input"].read(content_length)
            try:
                body = json.loads(raw_body.decode("utf-8"))
            except Exception:
                body = {}

        status_code, resp_data = handle_api(method, path, query_params, body, headers)
        resp_bytes = json.dumps(resp_data).encode("utf-8")
        status_line = HTTP_STATUS_PHRASES.get(status_code, f"{status_code} OK" if status_code < 400 else f"{status_code} Error")
        start_response(status_line, [
            ("Content-Type", "application/json; charset=utf-8"),
            ("Content-Length", str(len(resp_bytes))),
            ("Access-Control-Allow-Origin", "*"),
            ("Access-Control-Allow-Headers", "Content-Type, Authorization"),
            ("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
        ])
        return [resp_bytes]

    # Non-API route: serve static asset fallback (e.g. /web/, /index.html, /css/style.css)
    file_bytes, mime_type = get_static_file(path)
    if file_bytes is not None:
        start_response("200 OK", [
            ("Content-Type", mime_type),
            ("Content-Length", str(len(file_bytes))),
            ("Cache-Control", "public, max-age=3600")
        ])
        return [file_bytes]

    err_bytes = json.dumps({"success": False, "error": f"Endpoint not found: {method} {path}"}).encode("utf-8")
    start_response("404 Not Found", [
        ("Content-Type", "application/json; charset=utf-8"),
        ("Content-Length", str(len(err_bytes)))
    ])
    return [err_bytes]

# ------------------------------------------------------------------------------
# 8. VERCEL BaseHTTPRequestHandler ENTRYPOINT (`handler`)
# ------------------------------------------------------------------------------
class handler(BaseHTTPRequestHandler):
    def _handle_request(self, method):
        parsed = urllib.parse.urlparse(self.path)
        raw_path = parsed.path
        query_params = {k: v[0] for k, v in urllib.parse.parse_qs(parsed.query).items()}
        headers = {}
        for k, v in self.headers.items():
            headers[k] = v
            headers[k.lower()] = v

        if method == "OPTIONS":
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
            self.end_headers()
            return

        # Resolve target API route (recovering path from rewrites/headers if needed)
        path = resolve_request_path(raw_path, headers, query_params)

        if is_known_api_endpoint(path):
            body = None
            length = int(self.headers.get("Content-Length", 0) or 0)
            if length > 0:
                raw = self.rfile.read(length)
                try:
                    body = json.loads(raw.decode("utf-8"))
                except Exception:
                    body = {}

            code, resp_data = handle_api(method, path, query_params, body, headers)
            data = json.dumps(resp_data).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS")
            self.end_headers()
            self.wfile.write(data)
            return

        file_bytes, mime_type = get_static_file(path)
        if file_bytes is not None:
            self.send_response(200)
            self.send_header("Content-Type", mime_type)
            self.send_header("Content-Length", str(len(file_bytes)))
            self.send_header("Cache-Control", "public, max-age=3600")
            self.end_headers()
            self.wfile.write(file_bytes)
            return

        self.send_response(404)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.end_headers()
        self.wfile.write(json.dumps({"success": False, "error": f"Endpoint not found: {method} {path}"}).encode("utf-8"))

    def do_GET(self):
        self._handle_request("GET")

    def do_POST(self):
        self._handle_request("POST")

    def do_PATCH(self):
        self._handle_request("PATCH")

    def do_DELETE(self):
        self._handle_request("DELETE")

    def do_OPTIONS(self):
        self._handle_request("OPTIONS")

__all__ = ["app", "handler"]
