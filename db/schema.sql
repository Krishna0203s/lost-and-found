-- College Lost & Found Database Schema
PRAGMA foreign_keys = ON;

-- Users table (students and administrators)
CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    student_id TEXT UNIQUE NOT NULL,
    email TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'student' CHECK(role IN ('student', 'admin')),
    password TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Campus Places / Locations table (managed by administrators)
CREATE TABLE IF NOT EXISTS locations (
    location_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Item Categories table (managed by administrators)
CREATE TABLE IF NOT EXISTS categories (
    category_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    icon TEXT DEFAULT '📦',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Items table (Lost and Found items)
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

-- Matches table (Compatibility scoring between lost and found items)
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

-- Claims table (Claims on found items with ownership verification)
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

-- Returns table (Records of finalized item returns)
CREATE TABLE IF NOT EXISTS returns (
    return_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL REFERENCES items(item_id) ON DELETE CASCADE,
    claim_id INTEGER REFERENCES claims(claim_id) ON DELETE SET NULL,
    claimant_id TEXT NOT NULL,
    verified_by TEXT NOT NULL,
    return_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    notes TEXT
);

-- Indexes for frequent searches and filters
CREATE INDEX IF NOT EXISTS idx_items_category ON items(category);
CREATE INDEX IF NOT EXISTS idx_items_location ON items(location);
CREATE INDEX IF NOT EXISTS idx_items_status ON items(status);
CREATE INDEX IF NOT EXISTS idx_items_type ON items(item_type);
CREATE INDEX IF NOT EXISTS idx_items_priority ON items(priority);
CREATE INDEX IF NOT EXISTS idx_items_report_date ON items(report_date);
CREATE INDEX IF NOT EXISTS idx_matches_pair ON matches(lost_item_id, found_item_id);
CREATE INDEX IF NOT EXISTS idx_claims_found_item ON claims(found_item_id);
