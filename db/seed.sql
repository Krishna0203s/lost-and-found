-- Clean State Seed Data for College Lost & Found System
-- Keeps only the administrator and student user accounts

-- Administrator Account (admin / admin123)
INSERT OR IGNORE INTO users (user_id, name, student_id, email, role, password) VALUES
(1, 'Campus Admin', 'admin', 'admin@campus.edu', 'admin', 'admin123');

-- Standard User Account (user / user123)
INSERT OR IGNORE INTO users (user_id, name, student_id, email, role, password) VALUES
(2, 'Campus User', 'user', 'user@campus.edu', 'student', 'user123');
