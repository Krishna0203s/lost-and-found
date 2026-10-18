#define _GNU_SOURCE
#include "db.h"
#include "matching.h"
#include "validation.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

static sqlite3* g_db = NULL;
static pthread_mutex_t g_db_mutex;
static int g_db_mutex_inited = 0;

void db_lock(void) {
    if (!g_db_mutex_inited) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&g_db_mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        g_db_mutex_inited = 1;
    }
    pthread_mutex_lock(&g_db_mutex);
}

void db_unlock(void) {
    pthread_mutex_unlock(&g_db_mutex);
}

static void item_from_stmt(sqlite3_stmt* stmt, Item* item) {
    memset(item, 0, sizeof(Item));
    item->item_id = sqlite3_column_int(stmt, 0);

    const char* p;
    p = (const char*)sqlite3_column_text(stmt, 1);
    if (p) strncpy(item->item_type, p, sizeof(item->item_type) - 1);

    p = (const char*)sqlite3_column_text(stmt, 2);
    if (p) strncpy(item->item_name, p, sizeof(item->item_name) - 1);

    p = (const char*)sqlite3_column_text(stmt, 3);
    if (p) strncpy(item->category, p, sizeof(item->category) - 1);

    p = (const char*)sqlite3_column_text(stmt, 4);
    if (p) strncpy(item->description, p, sizeof(item->description) - 1);

    p = (const char*)sqlite3_column_text(stmt, 5);
    if (p) strncpy(item->location, p, sizeof(item->location) - 1);

    p = (const char*)sqlite3_column_text(stmt, 6);
    if (p) strncpy(item->reported_by, p, sizeof(item->reported_by) - 1);

    p = (const char*)sqlite3_column_text(stmt, 7);
    if (p) strncpy(item->student_id, p, sizeof(item->student_id) - 1);

    p = (const char*)sqlite3_column_text(stmt, 8);
    if (p) strncpy(item->contact_info, p, sizeof(item->contact_info) - 1);

    p = (const char*)sqlite3_column_text(stmt, 9);
    if (p) strncpy(item->report_date, p, sizeof(item->report_date) - 1);

    p = (const char*)sqlite3_column_text(stmt, 10);
    if (p) strncpy(item->priority, p, sizeof(item->priority) - 1);

    p = (const char*)sqlite3_column_text(stmt, 11);
    if (p) strncpy(item->status, p, sizeof(item->status) - 1);

    p = (const char*)sqlite3_column_text(stmt, 12);
    if (p) strncpy(item->storage_location, p, sizeof(item->storage_location) - 1);

    p = (const char*)sqlite3_column_text(stmt, 13);
    if (p) strncpy(item->identifying_details, p, sizeof(item->identifying_details) - 1);

    p = (const char*)sqlite3_column_text(stmt, 14);
    if (p) strncpy(item->created_at, p, sizeof(item->created_at) - 1);

    p = (const char*)sqlite3_column_text(stmt, 15);
    if (p) strncpy(item->updated_at, p, sizeof(item->updated_at) - 1);
}

int db_init(const char* db_file) {
    if (g_db) return 1;

    int rc = sqlite3_open_v2(db_file, &g_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database %s: %s\n", db_file, sqlite3_errmsg(g_db));
        return 0;
    }

    sqlite3_busy_timeout(g_db, 5000);
    sqlite3_exec(g_db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    /* Read schema.sql if available, or create tables directly */
    FILE* f = fopen("db/schema.sql", "r");
    if (!f) {
        f = fopen("college_lost_found_project/db/schema.sql", "r");
    }

    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc(sz + 1);
        if (buf) {
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            char* err = NULL;
            rc = sqlite3_exec(g_db, buf, NULL, NULL, &err);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "Schema exec warning: %s\n", err ? err : "unknown");
                sqlite3_free(err);
            }
            free(buf);
        }
        fclose(f);
    }

    /* Ensure locations and categories tables exist and have defaults */
    sqlite3_exec(g_db,
        "CREATE TABLE IF NOT EXISTS locations ("
        "location_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT UNIQUE NOT NULL, "
        "description TEXT, "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);", NULL, NULL, NULL);

    sqlite3_exec(g_db,
        "CREATE TABLE IF NOT EXISTS categories ("
        "category_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT UNIQUE NOT NULL, "
        "icon TEXT DEFAULT '📦', "
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);", NULL, NULL, NULL);


    /* Ensure password column exists in users table (migration safety) */
    sqlite3_exec(g_db, "ALTER TABLE users ADD COLUMN password TEXT DEFAULT '';", NULL, NULL, NULL);

    /* Ensure baseline admin account exists if no admin account is present */
    sqlite3_stmt* admin_stmt = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM users WHERE role = 'admin';", -1, &admin_stmt, NULL) == SQLITE_OK) {
        int admin_count = 0;
        if (sqlite3_step(admin_stmt) == SQLITE_ROW) {
            admin_count = sqlite3_column_int(admin_stmt, 0);
        }
        sqlite3_finalize(admin_stmt);
        if (admin_count == 0) {
            sqlite3_exec(g_db,
                "INSERT INTO users (name, student_id, email, role, password) "
                "VALUES ('admin', 'admin', 'admin@campus.edu', 'admin', 'admin123');",
                NULL, NULL, NULL);
        }
    }

    return 1;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

int db_seed(void) {
    if (!g_db) return 0;
    FILE* f = fopen("db/seed.sql", "r");
    if (!f) {
        f = fopen("college_lost_found_project/db/seed.sql", "r");
    }
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }

    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    char* err = NULL;
    int rc = sqlite3_exec(g_db, buf, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Seed warning: %s\n", err ? err : "unknown");
        sqlite3_free(err);
    }
    free(buf);

    /* Run full match evaluation after seeding */
    int matched = 0;
    db_evaluate_all_matches(&matched);
    return 1;
}

int db_create_item(const Item* item, int* out_item_id) {
    if (!g_db || !item) return 0;

    const char* sql = "INSERT INTO items ("
                      "item_type, item_name, category, description, location, "
                      "reported_by, student_id, contact_info, report_date, priority, "
                      "status, storage_location, identifying_details"
                      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, item->item_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, item->item_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, item->category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, item->description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, item->location, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, item->reported_by, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, item->student_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, item->contact_info, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, item->report_date, -1, SQLITE_STATIC);

    const char* prio = (strlen(item->priority) > 0) ? item->priority : "NORMAL";
    sqlite3_bind_text(stmt, 10, prio, -1, SQLITE_STATIC);

    const char* st = (strlen(item->status) > 0) ? item->status : item->item_type;
    sqlite3_bind_text(stmt, 11, st, -1, SQLITE_STATIC);

    sqlite3_bind_text(stmt, 12, item->storage_location, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, item->identifying_details, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    int new_id = (int)sqlite3_last_insert_rowid(g_db);
    if (out_item_id) *out_item_id = new_id;

    /* Automatically trigger matching evaluation for this newly added item */
    DetailedMatch* matches = NULL;
    int match_count = 0;
    db_evaluate_item_matches(new_id, &matches, &match_count);
    db_free_matches(matches);

    return 1;
}

int db_get_item(int item_id, Item* out_item) {
    if (!g_db || !out_item) return 0;

    const char* sql = "SELECT item_id, item_type, item_name, category, description, location, "
                      "reported_by, student_id, contact_info, report_date, priority, "
                      "status, storage_location, identifying_details, created_at, updated_at "
                      "FROM items WHERE item_id = ?;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, item_id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        item_from_stmt(stmt, out_item);
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_list_items_by_type(const char* item_type, Item** out_items, int* out_count) {
    ItemFilter filter = {0};
    if (item_type) {
        strncpy(filter.item_type, item_type, sizeof(filter.item_type) - 1);
    }
    return db_search_items(&filter, out_items, out_count);
}

int db_search_items(const ItemFilter* filter, Item** out_items, int* out_count) {
    if (!g_db || !out_items || !out_count) return 0;
    *out_items = NULL;
    *out_count = 0;

    char sql[2048] = "SELECT item_id, item_type, item_name, category, description, location, "
                     "reported_by, student_id, contact_info, report_date, priority, "
                     "status, storage_location, identifying_details, created_at, updated_at "
                     "FROM items WHERE 1=1 ";

    if (filter) {
        if (strlen(filter->item_type) > 0) {
            strcat(sql, " AND UPPER(item_type) = UPPER(?) ");
        }
        if (strlen(filter->category) > 0) {
            strcat(sql, " AND UPPER(category) = UPPER(?) ");
        }
        if (strlen(filter->location) > 0) {
            strcat(sql, " AND location LIKE ? ");
        }
        if (strlen(filter->student_id) > 0) {
            strcat(sql, " AND (student_id LIKE ? OR reported_by LIKE ?) ");
        }
        if (strlen(filter->status) > 0) {
            strcat(sql, " AND UPPER(status) = UPPER(?) ");
        }
        if (strlen(filter->priority) > 0) {
            strcat(sql, " AND UPPER(priority) = UPPER(?) ");
        }
        if (strlen(filter->query) > 0) {
            strcat(sql, " AND (item_name LIKE ? OR description LIKE ? OR identifying_details LIKE ?) ");
        }
    }

    strcat(sql, " ORDER BY item_id DESC;");

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Search prepare error: %s\n", sqlite3_errmsg(g_db));
        return 0;
    }

    int idx = 1;
    if (filter) {
        if (strlen(filter->item_type) > 0) {
            sqlite3_bind_text(stmt, idx++, filter->item_type, -1, SQLITE_STATIC);
        }
        if (strlen(filter->category) > 0) {
            sqlite3_bind_text(stmt, idx++, filter->category, -1, SQLITE_STATIC);
        }
        if (strlen(filter->location) > 0) {
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "%%%s%%", filter->location);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
        }
        if (strlen(filter->student_id) > 0) {
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "%%%s%%", filter->student_id);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
        }
        if (strlen(filter->status) > 0) {
            sqlite3_bind_text(stmt, idx++, filter->status, -1, SQLITE_STATIC);
        }
        if (strlen(filter->priority) > 0) {
            sqlite3_bind_text(stmt, idx++, filter->priority, -1, SQLITE_STATIC);
        }
        if (strlen(filter->query) > 0) {
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "%%%s%%", filter->query);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, pattern, -1, SQLITE_TRANSIENT);
        }
    }

    int cap = 16;
    Item* list = malloc(sizeof(Item) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            Item* tmp = realloc(list, sizeof(Item) * cap);
            if (!tmp) {
                free(list);
                sqlite3_finalize(stmt);
                return 0;
            }
            list = tmp;
        }
        item_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_items = list;
    *out_count = count;
    return 1;
}

int db_update_item_status(int item_id, const char* new_status) {
    if (!g_db || !new_status) return 0;

    Item current;
    if (!db_get_item(item_id, &current)) return 0;

    if (!validate_status_transition(current.status, new_status)) {
        fprintf(stderr, "Invalid status transition: %s -> %s for item %d\n",
                current.status, new_status, item_id);
        return 0;
    }

    const char* sql = "UPDATE items SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE item_id = ?;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, item_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

int db_evaluate_item_matches(int item_id, DetailedMatch** out_matches, int* out_count) {
    if (!g_db || !out_matches || !out_count) return 0;
    *out_matches = NULL;
    *out_count = 0;

    Item target;
    if (!db_get_item(item_id, &target)) return 0;

    const char* opposite_type = (strcasecmp(target.item_type, "LOST") == 0) ? "FOUND" : "LOST";
    Item* candidates = NULL;
    int cand_count = 0;
    ItemFilter filter = {0};
    strncpy(filter.item_type, opposite_type, sizeof(filter.item_type) - 1);

    if (!db_search_items(&filter, &candidates, &cand_count)) return 0;

    int cap = 16;
    DetailedMatch* list = malloc(sizeof(DetailedMatch) * cap);
    int count = 0;

    for (int i = 0; i < cand_count; i++) {
        Item* lost = (strcasecmp(target.item_type, "LOST") == 0) ? &target : &candidates[i];
        Item* found = (strcasecmp(target.item_type, "LOST") == 0) ? &candidates[i] : &target;

        MatchScore score;
        calculate_match_score(lost, found, &score);

        /* Save or update match in database */
        const char* upsert_sql =
            "INSERT INTO matches (lost_item_id, found_item_id, category_score, location_score, "
            "description_score, date_score, total_score, match_status) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(lost_item_id, found_item_id) DO UPDATE SET "
            "category_score=excluded.category_score, location_score=excluded.location_score, "
            "description_score=excluded.description_score, date_score=excluded.date_score, "
            "total_score=excluded.total_score, match_status=excluded.match_status;";

        sqlite3_stmt* mstmt = NULL;
        if (sqlite3_prepare_v2(g_db, upsert_sql, -1, &mstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(mstmt, 1, lost->item_id);
            sqlite3_bind_int(mstmt, 2, found->item_id);
            sqlite3_bind_int(mstmt, 3, score.category_score);
            sqlite3_bind_int(mstmt, 4, score.location_score);
            sqlite3_bind_int(mstmt, 5, score.description_score);
            sqlite3_bind_int(mstmt, 6, score.date_score);
            sqlite3_bind_int(mstmt, 7, score.total_score);
            sqlite3_bind_text(mstmt, 8, score.match_status, -1, SQLITE_STATIC);
            sqlite3_step(mstmt);
            sqlite3_finalize(mstmt);
        }

        /* If this is a strong match (>= 70), suggest or mark as MATCHED if currently LOST or FOUND */
        if (score.is_possible_match) {
            if (strcasecmp(lost->status, "LOST") == 0) {
                db_update_item_status(lost->item_id, "MATCHED");
            }
            if (strcasecmp(found->status, "FOUND") == 0) {
                db_update_item_status(found->item_id, "MATCHED");
            }
        }

        if (count >= cap) {
            cap *= 2;
            DetailedMatch* tmp = realloc(list, sizeof(DetailedMatch) * cap);
            if (!tmp) break;
            list = tmp;
        }

        list[count].score = score;
        list[count].lost_item = *lost;
        list[count].found_item = *found;
        count++;
    }

    db_free_items(candidates);

    /* Sort descending by total_score */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (list[j].score.total_score > list[i].score.total_score) {
                DetailedMatch temp = list[i];
                list[i] = list[j];
                list[j] = temp;
            }
        }
    }

    *out_matches = list;
    *out_count = count;
    return 1;
}

int db_evaluate_all_matches(int* out_matches_found) {
    if (!g_db) return 0;
    Item* lost_items = NULL;
    int lost_count = 0;
    db_list_items_by_type("LOST", &lost_items, &lost_count);

    int total_strong = 0;
    for (int i = 0; i < lost_count; i++) {
        DetailedMatch* matches = NULL;
        int mcount = 0;
        db_evaluate_item_matches(lost_items[i].item_id, &matches, &mcount);
        for (int j = 0; j < mcount; j++) {
            if (matches[j].score.is_possible_match) total_strong++;
        }
        db_free_matches(matches);
    }

    db_free_items(lost_items);
    if (out_matches_found) *out_matches_found = total_strong;
    return 1;
}

int db_get_matches_for_item(int item_id, DetailedMatch** out_matches, int* out_count) {
    return db_evaluate_item_matches(item_id, out_matches, out_count);
}

int db_create_claim(const Claim* claim, int* out_claim_id) {
    if (!g_db || !claim) return 0;

    /* Verify found item exists, is type FOUND, and not already RETURNED */
    Item found;
    if (!db_get_item(claim->found_item_id, &found)) return 0;
    if (strcasecmp(found.item_type, "FOUND") != 0) return 0;
    if (strcasecmp(found.status, "RETURNED") == 0) return 0;

    const char* sql = "INSERT INTO claims ("
                      "found_item_id, claimant_name, claimant_id, claimant_contact, "
                      "verification_response, claim_status"
                      ") VALUES (?, ?, ?, ?, ?, 'PENDING');";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, claim->found_item_id);
    sqlite3_bind_text(stmt, 2, claim->claimant_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, claim->claimant_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, claim->claimant_contact, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, claim->verification_response, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    int new_id = (int)sqlite3_last_insert_rowid(g_db);
    if (out_claim_id) *out_claim_id = new_id;

    /* Mark found item as MATCHED if currently FOUND */
    if (strcasecmp(found.status, "FOUND") == 0) {
        db_update_item_status(claim->found_item_id, "MATCHED");
    }

    return 1;
}

int db_get_claim(int claim_id, Claim* out_claim) {
    if (!g_db || !out_claim) return 0;
    memset(out_claim, 0, sizeof(Claim));

    const char* sql = "SELECT claim_id, found_item_id, claimant_name, claimant_id, "
                      "claimant_contact, verification_response, claim_status, admin_notes, "
                      "submitted_at, reviewed_at, reviewed_by FROM claims WHERE claim_id = ?;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, claim_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_claim->claim_id = sqlite3_column_int(stmt, 0);
        out_claim->found_item_id = sqlite3_column_int(stmt, 1);

        const char* p;
        p = (const char*)sqlite3_column_text(stmt, 2);
        if (p) strncpy(out_claim->claimant_name, p, sizeof(out_claim->claimant_name) - 1);

        p = (const char*)sqlite3_column_text(stmt, 3);
        if (p) strncpy(out_claim->claimant_id, p, sizeof(out_claim->claimant_id) - 1);

        p = (const char*)sqlite3_column_text(stmt, 4);
        if (p) strncpy(out_claim->claimant_contact, p, sizeof(out_claim->claimant_contact) - 1);

        p = (const char*)sqlite3_column_text(stmt, 5);
        if (p) strncpy(out_claim->verification_response, p, sizeof(out_claim->verification_response) - 1);

        p = (const char*)sqlite3_column_text(stmt, 6);
        if (p) strncpy(out_claim->claim_status, p, sizeof(out_claim->claim_status) - 1);

        p = (const char*)sqlite3_column_text(stmt, 7);
        if (p) strncpy(out_claim->admin_notes, p, sizeof(out_claim->admin_notes) - 1);

        p = (const char*)sqlite3_column_text(stmt, 8);
        if (p) strncpy(out_claim->submitted_at, p, sizeof(out_claim->submitted_at) - 1);

        p = (const char*)sqlite3_column_text(stmt, 9);
        if (p) strncpy(out_claim->reviewed_at, p, sizeof(out_claim->reviewed_at) - 1);

        p = (const char*)sqlite3_column_text(stmt, 10);
        if (p) strncpy(out_claim->reviewed_by, p, sizeof(out_claim->reviewed_by) - 1);

        db_get_item(out_claim->found_item_id, &out_claim->item);

        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_list_claims(Claim** out_claims, int* out_count) {
    if (!g_db || !out_claims || !out_count) return 0;
    *out_claims = NULL;
    *out_count = 0;

    const char* sql = "SELECT claim_id, found_item_id, claimant_name, claimant_id, "
                      "claimant_contact, verification_response, claim_status, admin_notes, "
                      "submitted_at, reviewed_at, reviewed_by FROM claims ORDER BY claim_id DESC;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    int cap = 16;
    Claim* list = malloc(sizeof(Claim) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            Claim* tmp = realloc(list, sizeof(Claim) * cap);
            if (!tmp) break;
            list = tmp;
        }

        Claim* c = &list[count];
        memset(c, 0, sizeof(Claim));
        c->claim_id = sqlite3_column_int(stmt, 0);
        c->found_item_id = sqlite3_column_int(stmt, 1);

        const char* p;
        p = (const char*)sqlite3_column_text(stmt, 2);
        if (p) strncpy(c->claimant_name, p, sizeof(c->claimant_name) - 1);

        p = (const char*)sqlite3_column_text(stmt, 3);
        if (p) strncpy(c->claimant_id, p, sizeof(c->claimant_id) - 1);

        p = (const char*)sqlite3_column_text(stmt, 4);
        if (p) strncpy(c->claimant_contact, p, sizeof(c->claimant_contact) - 1);

        p = (const char*)sqlite3_column_text(stmt, 5);
        if (p) strncpy(c->verification_response, p, sizeof(c->verification_response) - 1);

        p = (const char*)sqlite3_column_text(stmt, 6);
        if (p) strncpy(c->claim_status, p, sizeof(c->claim_status) - 1);

        p = (const char*)sqlite3_column_text(stmt, 7);
        if (p) strncpy(c->admin_notes, p, sizeof(c->admin_notes) - 1);

        p = (const char*)sqlite3_column_text(stmt, 8);
        if (p) strncpy(c->submitted_at, p, sizeof(c->submitted_at) - 1);

        p = (const char*)sqlite3_column_text(stmt, 9);
        if (p) strncpy(c->reviewed_at, p, sizeof(c->reviewed_at) - 1);

        p = (const char*)sqlite3_column_text(stmt, 10);
        if (p) strncpy(c->reviewed_by, p, sizeof(c->reviewed_by) - 1);

        db_get_item(c->found_item_id, &c->item);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_claims = list;
    *out_count = count;
    return 1;
}

int db_review_claim(int claim_id, const char* status, const char* reviewer, const char* notes) {
    if (!g_db || !status) return 0;

    Claim claim;
    if (!db_get_claim(claim_id, &claim)) return 0;

    const char* sql = "UPDATE claims SET claim_status = ?, reviewed_by = ?, admin_notes = ?, "
                      "reviewed_at = CURRENT_TIMESTAMP WHERE claim_id = ?;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, reviewer ? reviewer : "Admin", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, notes ? notes : "", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, claim_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return 0;

    /* Update item status accordingly */
    if (strcasecmp(status, "APPROVED") == 0) {
        db_update_item_status(claim.found_item_id, "CLAIMED");
    } else if (strcasecmp(status, "REJECTED") == 0) {
        /* Only reset item status to FOUND if there are no other APPROVED or PENDING claims */
        sqlite3_stmt* chk = NULL;
        int has_approved = 0;
        int has_pending = 0;
        if (sqlite3_prepare_v2(g_db, "SELECT claim_status, COUNT(*) FROM claims WHERE found_item_id = ? AND claim_id != ? GROUP BY claim_status;", -1, &chk, NULL) == SQLITE_OK) {
            sqlite3_bind_int(chk, 1, claim.found_item_id);
            sqlite3_bind_int(chk, 2, claim_id);
            while (sqlite3_step(chk) == SQLITE_ROW) {
                const char* st = (const char*)sqlite3_column_text(chk, 0);
                int cnt = sqlite3_column_int(chk, 1);
                if (st && strcasecmp(st, "APPROVED") == 0 && cnt > 0) has_approved = 1;
                if (st && strcasecmp(st, "PENDING") == 0 && cnt > 0) has_pending = 1;
            }
            sqlite3_finalize(chk);
        }
        if (!has_approved) {
            db_update_item_status(claim.found_item_id, has_pending ? "MATCHED" : "FOUND");
        }
    }

    return 1;
}

int db_record_return(int item_id, int claim_id, const char* claimant_id, const char* verified_by, const char* notes) {
    if (!g_db) return 0;

    Item item;
    if (!db_get_item(item_id, &item)) return 0;
    if (strcasecmp(item.status, "RETURNED") == 0) return 0; /* Already returned */

    const char* sql = "INSERT INTO returns (item_id, claim_id, claimant_id, verified_by, notes) "
                      "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, item_id);
    if (claim_id > 0) {
        sqlite3_bind_int(stmt, 2, claim_id);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_text(stmt, 3, claimant_id ? claimant_id : "Unknown", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, verified_by ? verified_by : "Campus Security", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, notes ? notes : "Handed over at campus desk", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return 0;

    /* Ensure associated claim is APPROVED if claim_id > 0 */
    if (claim_id > 0) {
        Claim clm;
        if (db_get_claim(claim_id, &clm) && strcasecmp(clm.claim_status, "APPROVED") != 0) {
            db_review_claim(claim_id, "APPROVED", verified_by, "Approved upon physical item handover.");
        }
    }

    /* Update item status to RETURNED */
    db_update_item_status(item_id, "RETURNED");

    /* Also if there are matching paired items, update them if applicable */
    if (strcasecmp(item.item_type, "FOUND") == 0) {
        /* If claimant reported a matching LOST item, mark it RETURNED */
        if (claimant_id && strlen(claimant_id) > 0) {
            const char* stu_sql = "SELECT item_id FROM items WHERE item_type = 'LOST' "
                                  "AND (student_id = ? OR reported_by = ?) AND status != 'RETURNED' LIMIT 1;";
            sqlite3_stmt* stu_stmt = NULL;
            if (sqlite3_prepare_v2(g_db, stu_sql, -1, &stu_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stu_stmt, 1, claimant_id, -1, SQLITE_STATIC);
                sqlite3_bind_text(stu_stmt, 2, claimant_id, -1, SQLITE_STATIC);
                if (sqlite3_step(stu_stmt) == SQLITE_ROW) {
                    int lost_id = sqlite3_column_int(stu_stmt, 0);
                    db_update_item_status(lost_id, "RETURNED");
                }
                sqlite3_finalize(stu_stmt);
            }
        }
        /* Check highest matching LOST item in matches */
        const char* pair_sql = "SELECT lost_item_id FROM matches WHERE found_item_id = ? "
                               "AND total_score >= 70 ORDER BY total_score DESC LIMIT 1;";
        sqlite3_stmt* pair_stmt = NULL;
        if (sqlite3_prepare_v2(g_db, pair_sql, -1, &pair_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(pair_stmt, 1, item_id);
            if (sqlite3_step(pair_stmt) == SQLITE_ROW) {
                int lost_id = sqlite3_column_int(pair_stmt, 0);
                db_update_item_status(lost_id, "RETURNED");
            }
            sqlite3_finalize(pair_stmt);
        }
        /* Confirm matches */
        sqlite3_stmt* mup = NULL;
        if (sqlite3_prepare_v2(g_db, "UPDATE matches SET match_status = 'CONFIRMED' WHERE found_item_id = ? AND total_score >= 70;", -1, &mup, NULL) == SQLITE_OK) {
            sqlite3_bind_int(mup, 1, item_id);
            sqlite3_step(mup);
            sqlite3_finalize(mup);
        }
    } else if (strcasecmp(item.item_type, "LOST") == 0) {
        /* If returning a LOST item directly, check for paired FOUND item */
        const char* pair_sql = "SELECT found_item_id FROM matches WHERE lost_item_id = ? "
                               "AND total_score >= 70 ORDER BY total_score DESC LIMIT 1;";
        sqlite3_stmt* pair_stmt = NULL;
        if (sqlite3_prepare_v2(g_db, pair_sql, -1, &pair_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(pair_stmt, 1, item_id);
            if (sqlite3_step(pair_stmt) == SQLITE_ROW) {
                int found_id = sqlite3_column_int(pair_stmt, 0);
                db_update_item_status(found_id, "RETURNED");
            }
            sqlite3_finalize(pair_stmt);
        }
        sqlite3_stmt* mup = NULL;
        if (sqlite3_prepare_v2(g_db, "UPDATE matches SET match_status = 'CONFIRMED' WHERE lost_item_id = ? AND total_score >= 70;", -1, &mup, NULL) == SQLITE_OK) {
            sqlite3_bind_int(mup, 1, item_id);
            sqlite3_step(mup);
            sqlite3_finalize(mup);
        }
    }

    return 1;
}

int db_get_statistics(SystemStatistics* stats) {
    if (!g_db || !stats) return 0;
    memset(stats, 0, sizeof(SystemStatistics));

    sqlite3_stmt* stmt = NULL;

    /* Total lost */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM items WHERE item_type = 'LOST';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->total_lost = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Total found */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM items WHERE item_type = 'FOUND';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->total_found = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Possible matches (total score >= 70) */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM matches WHERE total_score >= 70;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->possible_matches = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Claimed */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM items WHERE status = 'CLAIMED';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->claimed = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Returned */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM items WHERE status = 'RETURNED';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->returned = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Still lost */
    if (sqlite3_prepare_v2(g_db, "SELECT COUNT(*) FROM items WHERE item_type = 'LOST' AND status NOT IN ('CLAIMED', 'RETURNED');", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats->still_lost = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Most common category */
    if (sqlite3_prepare_v2(g_db, "SELECT category, COUNT(*) as c FROM items GROUP BY category ORDER BY c DESC LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* c = (const char*)sqlite3_column_text(stmt, 0);
            if (c) strncpy(stats->most_common_category, c, sizeof(stats->most_common_category) - 1);
        }
        sqlite3_finalize(stmt);
    }
    if (strlen(stats->most_common_category) == 0) {
        strcpy(stats->most_common_category, "None");
    }

    /* Most common location */
    if (sqlite3_prepare_v2(g_db, "SELECT location, COUNT(*) as c FROM items GROUP BY location ORDER BY c DESC LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* loc = (const char*)sqlite3_column_text(stmt, 0);
            if (loc) strncpy(stats->most_common_location, loc, sizeof(stats->most_common_location) - 1);
        }
        sqlite3_finalize(stmt);
    }
    if (strlen(stats->most_common_location) == 0) {
        strcpy(stats->most_common_location, "None");
    }

    /* Recovery rate: returned items / total lost items */
    int total_resolved = stats->returned;
    if (stats->total_lost > 0) {
        stats->recovery_rate = ((double)total_resolved / (double)stats->total_lost) * 100.0;
        if (stats->recovery_rate > 100.0) stats->recovery_rate = 100.0;
    } else {
        stats->recovery_rate = 0.0;
    }

    return 1;
}

static void user_from_stmt(sqlite3_stmt* stmt, User* u) {
    memset(u, 0, sizeof(User));
    u->user_id = sqlite3_column_int(stmt, 0);

    const char* p = (const char*)sqlite3_column_text(stmt, 1);
    if (p) strncpy(u->name, p, sizeof(u->name) - 1);

    p = (const char*)sqlite3_column_text(stmt, 2);
    if (p) strncpy(u->student_id, p, sizeof(u->student_id) - 1);

    p = (const char*)sqlite3_column_text(stmt, 3);
    if (p) strncpy(u->email, p, sizeof(u->email) - 1);

    p = (const char*)sqlite3_column_text(stmt, 4);
    if (p) strncpy(u->role, p, sizeof(u->role) - 1);

    p = (const char*)sqlite3_column_text(stmt, 5);
    if (p) strncpy(u->password, p, sizeof(u->password) - 1);

    p = (const char*)sqlite3_column_text(stmt, 6);
    if (p) strncpy(u->created_at, p, sizeof(u->created_at) - 1);
}

int db_get_user_by_identifier(const char* identifier, User* out_user) {
    if (!g_db || !identifier || !out_user) return 0;
    memset(out_user, 0, sizeof(User));

    const char* sql = "SELECT user_id, name, student_id, email, role, COALESCE(password, ''), created_at "
                      "FROM users "
                      "WHERE UPPER(student_id) = UPPER(?) "
                      "   OR UPPER(email) = UPPER(?) "
                      "   OR UPPER(name) = UPPER(?) LIMIT 1;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, identifier, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, identifier, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, identifier, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        user_from_stmt(stmt, out_user);
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int db_list_users(User** out_users, int* out_count) {
    if (!g_db || !out_users || !out_count) return 0;
    *out_users = NULL;
    *out_count = 0;

    const char* sql = "SELECT user_id, name, student_id, email, role, COALESCE(password, ''), created_at "
                      "FROM users ORDER BY role DESC, user_id ASC;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    int cap = 8;
    User* list = malloc(sizeof(User) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            User* tmp = realloc(list, sizeof(User) * cap);
            if (!tmp) break;
            list = tmp;
        }
        user_from_stmt(stmt, &list[count]);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_users = list;
    *out_count = count;
    return 1;
}

void db_free_users(User* users) {
    if (users) free(users);
}

int db_create_user(const User* user, int* out_id) {
    if (!g_db || !user || !user->name[0] || !user->student_id[0] || !user->email[0]) return 0;

    const char* role = (user->role[0] != '\0') ? user->role : "student";
    const char* password = (user->password[0] != '\0') ? user->password : "";
    const char* sql = "INSERT INTO users (name, student_id, email, role, password) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, user->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user->student_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user->email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, role, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, password, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return 0;

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(g_db);
    return 1;
}

int db_delete_user(int user_id) {
    if (!g_db || user_id <= 0) return 0;

    /* Check if target user exists and inspect role */
    const char* check_sql = "SELECT role FROM users WHERE user_id = ?;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, check_sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, user_id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0; /* User not found */
    }

    const char* role = (const char*)sqlite3_column_text(stmt, 0);
    int is_admin = (role && strcmp(role, "admin") == 0);
    sqlite3_finalize(stmt);

    /* Safeguard: If target is admin, ensure at least one other admin remains */
    if (is_admin) {
        const char* count_sql = "SELECT COUNT(*) FROM users WHERE role = 'admin' AND user_id != ?;";
        if (sqlite3_prepare_v2(g_db, count_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            int other_admins = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                other_admins = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
            if (other_admins < 1) {
                return 0; /* Cannot delete the sole administrator */
            }
        }
    }

    const char* del_sql = "DELETE FROM users WHERE user_id = ?;";
    if (sqlite3_prepare_v2(g_db, del_sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

void db_free_items(Item* items) {
    if (items) free(items);
}

void db_free_matches(DetailedMatch* matches) {
    if (matches) free(matches);
}

void db_free_claims(Claim* claims) {
    if (claims) free(claims);
}

int db_delete_item(int item_id) {
    if (!g_db || item_id <= 0) return 0;

    Item check_item;
    if (!db_get_item(item_id, &check_item)) {
        return 0; /* Item not found */
    }

    /* Delete associated matches */
    sqlite3_stmt* s1 = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM matches WHERE lost_item_id = ? OR found_item_id = ?;", -1, &s1, NULL) == SQLITE_OK) {
        sqlite3_bind_int(s1, 1, item_id);
        sqlite3_bind_int(s1, 2, item_id);
        sqlite3_step(s1);
        sqlite3_finalize(s1);
    }

    /* Delete associated claims */
    sqlite3_stmt* s2 = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM claims WHERE found_item_id = ?;", -1, &s2, NULL) == SQLITE_OK) {
        sqlite3_bind_int(s2, 1, item_id);
        sqlite3_step(s2);
        sqlite3_finalize(s2);
    }

    /* Delete associated returns */
    sqlite3_stmt* s3 = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM returns WHERE item_id = ?;", -1, &s3, NULL) == SQLITE_OK) {
        sqlite3_bind_int(s3, 1, item_id);
        sqlite3_step(s3);
        sqlite3_finalize(s3);
    }

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM items WHERE item_id = ?;", -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, item_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

int db_get_all_matches(DetailedMatch** out_matches, int* out_count, int min_score) {
    if (!g_db || !out_matches || !out_count) return 0;
    *out_matches = NULL;
    *out_count = 0;

    const char* sql =
        "SELECT match_id, lost_item_id, found_item_id, category_score, location_score, "
        "description_score, date_score, total_score, match_status, created_at "
        "FROM matches WHERE total_score >= ? ORDER BY total_score DESC, created_at DESC;";

    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, min_score);

    int cap = 16;
    DetailedMatch* list = malloc(sizeof(DetailedMatch) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            DetailedMatch* tmp = realloc(list, sizeof(DetailedMatch) * cap);
            if (!tmp) break;
            list = tmp;
        }

        memset(&list[count], 0, sizeof(DetailedMatch));
        list[count].score.match_id = sqlite3_column_int(stmt, 0);
        list[count].score.lost_item_id = sqlite3_column_int(stmt, 1);
        list[count].score.found_item_id = sqlite3_column_int(stmt, 2);
        list[count].score.category_score = sqlite3_column_int(stmt, 3);
        list[count].score.location_score = sqlite3_column_int(stmt, 4);
        list[count].score.description_score = sqlite3_column_int(stmt, 5);
        list[count].score.date_score = sqlite3_column_int(stmt, 6);
        list[count].score.total_score = sqlite3_column_int(stmt, 7);
        list[count].score.is_possible_match = (list[count].score.total_score >= 70) ? 1 : 0;

        const char* st = (const char*)sqlite3_column_text(stmt, 8);
        if (st) strncpy(list[count].score.match_status, st, sizeof(list[count].score.match_status) - 1);
        const char* dt = (const char*)sqlite3_column_text(stmt, 9);
        if (dt) strncpy(list[count].score.created_at, dt, sizeof(list[count].score.created_at) - 1);

        db_get_item(list[count].score.lost_item_id, &list[count].lost_item);
        db_get_item(list[count].score.found_item_id, &list[count].found_item);

        /* Only include if both items still exist */
        if (list[count].lost_item.item_id > 0 && list[count].found_item.item_id > 0) {
            count++;
        }
    }

    sqlite3_finalize(stmt);
    *out_matches = list;
    *out_count = count;
    return 1;
}

int db_list_locations(Location** out_locations, int* out_count) {
    if (!g_db || !out_locations || !out_count) return 0;
    *out_locations = NULL;
    *out_count = 0;

    const char* sql = "SELECT location_id, name, description, created_at FROM locations ORDER BY name ASC;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    int cap = 16;
    Location* list = malloc(sizeof(Location) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            Location* tmp = realloc(list, sizeof(Location) * cap);
            if (!tmp) break;
            list = tmp;
        }

        memset(&list[count], 0, sizeof(Location));
        list[count].location_id = sqlite3_column_int(stmt, 0);
        const char* p = (const char*)sqlite3_column_text(stmt, 1);
        if (p) strncpy(list[count].name, p, sizeof(list[count].name) - 1);
        p = (const char*)sqlite3_column_text(stmt, 2);
        if (p) strncpy(list[count].description, p, sizeof(list[count].description) - 1);
        p = (const char*)sqlite3_column_text(stmt, 3);
        if (p) strncpy(list[count].created_at, p, sizeof(list[count].created_at) - 1);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_locations = list;
    *out_count = count;
    return 1;
}

int db_create_location(const Location* loc, int* out_id) {
    if (!g_db || !loc || strlen(loc->name) == 0) return 0;

    const char* sql = "INSERT INTO locations (name, description) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, loc->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, loc->description, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return 0;

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(g_db);
    return 1;
}

int db_delete_location(int location_id) {
    if (!g_db || location_id <= 0) return 0;

    const char* sql = "DELETE FROM locations WHERE location_id = ?;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, location_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

void db_free_locations(Location* locations) {
    if (locations) free(locations);
}

int db_list_categories(Category** out_categories, int* out_count) {
    if (!g_db || !out_categories || !out_count) return 0;
    *out_categories = NULL;
    *out_count = 0;

    const char* sql = "SELECT category_id, name, icon, created_at FROM categories ORDER BY name ASC;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    int cap = 16;
    Category* list = malloc(sizeof(Category) * cap);
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            Category* tmp = realloc(list, sizeof(Category) * cap);
            if (!tmp) break;
            list = tmp;
        }

        memset(&list[count], 0, sizeof(Category));
        list[count].category_id = sqlite3_column_int(stmt, 0);
        const char* p = (const char*)sqlite3_column_text(stmt, 1);
        if (p) strncpy(list[count].name, p, sizeof(list[count].name) - 1);
        p = (const char*)sqlite3_column_text(stmt, 2);
        if (p) strncpy(list[count].icon, p, sizeof(list[count].icon) - 1);
        p = (const char*)sqlite3_column_text(stmt, 3);
        if (p) strncpy(list[count].created_at, p, sizeof(list[count].created_at) - 1);
        count++;
    }

    sqlite3_finalize(stmt);
    *out_categories = list;
    *out_count = count;
    return 1;
}

int db_create_category(const Category* cat, int* out_id) {
    if (!g_db || !cat || strlen(cat->name) == 0) return 0;

    const char* sql = "INSERT INTO categories (name, icon) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, cat->name, -1, SQLITE_STATIC);
    const char* icon = (strlen(cat->icon) > 0) ? cat->icon : "📦";
    sqlite3_bind_text(stmt, 2, icon, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return 0;

    if (out_id) *out_id = (int)sqlite3_last_insert_rowid(g_db);
    return 1;
}

int db_delete_category(int category_id) {
    if (!g_db || category_id <= 0) return 0;

    const char* sql = "DELETE FROM categories WHERE category_id = ?;";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, category_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

void db_free_categories(Category* categories) {
    if (categories) free(categories);
}

const char* db_get_sqlite_version(void) {
    return sqlite3_libversion();
}

