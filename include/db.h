#ifndef DB_H
#define DB_H

#include "models.h"

/* Database thread-safety mutex locking */
void db_lock(void);
void db_unlock(void);

/* Initialize SQLite database with schema from schema.sql or embedded DDL */
int db_init(const char* db_file);

/* Close database connection */
void db_close(void);

/* Seed database with initial realistic sample data */
int db_seed(void);

/* Create new item (Lost or Found) */
int db_create_item(const Item* item, int* out_item_id);

/* Retrieve single item by ID */
int db_get_item(int item_id, Item* out_item);

/* List items filtered by type ("LOST" or "FOUND") */
int db_list_items_by_type(const char* item_type, Item** out_items, int* out_count);

/* Advanced multi-criteria search */
int db_search_items(const ItemFilter* filter, Item** out_items, int* out_count);

/* Update item status with validation check */
int db_update_item_status(int item_id, const char* new_status);

/* Run matching algorithm between item and opposite candidates */
int db_evaluate_item_matches(int item_id, DetailedMatch** out_matches, int* out_count);

/* Run matching algorithm across all lost and found items in database */
int db_evaluate_all_matches(int* out_matches_found);

/* Retrieve existing matches for an item */
int db_get_matches_for_item(int item_id, DetailedMatch** out_matches, int* out_count);

/* Submit an ownership claim for a found item */
int db_create_claim(const Claim* claim, int* out_claim_id);

/* Retrieve claim by ID */
int db_get_claim(int claim_id, Claim* out_claim);

/* List all claims (e.g. for admin review) */
int db_list_claims(Claim** out_claims, int* out_count);

/* Admin reviews claim (approve or reject) and updates item status */
int db_review_claim(int claim_id, const char* status, const char* reviewer, const char* notes);

/* Finalize item return */
int db_record_return(int item_id, int claim_id, const char* claimant_id, const char* verified_by, const char* notes);

/* Aggregate system statistics */
int db_get_statistics(SystemStatistics* stats);

/* User queries & management (Admin Managed) */
int db_get_user_by_identifier(const char* identifier, User* out_user);
int db_list_users(User** out_users, int* out_count);
int db_create_user(const User* user, int* out_id);
int db_delete_user(int user_id);
void db_free_users(User* users);

/* Delete an item by ID (cascades or cleans related records) */
int db_delete_item(int item_id);

/* Retrieve all potential/confirmed matches in the system with score >= min_score */
int db_get_all_matches(DetailedMatch** out_matches, int* out_count, int min_score);

/* Campus Places / Locations CRUD (Admin Managed) */
int db_list_locations(Location** out_locations, int* out_count);
int db_create_location(const Location* loc, int* out_id);
int db_delete_location(int location_id);
void db_free_locations(Location* locations);

/* Item Categories CRUD (Admin Managed) */
int db_list_categories(Category** out_categories, int* out_count);
int db_create_category(const Category* cat, int* out_id);
int db_delete_category(int category_id);
void db_free_categories(Category* categories);

/* Free allocated lists */
void db_free_items(Item* items);
void db_free_matches(DetailedMatch* matches);
void db_free_claims(Claim* claims);

/* Database version information */
const char* db_get_sqlite_version(void);

#endif /* DB_H */
