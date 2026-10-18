#ifndef MODELS_H
#define MODELS_H

#include <stddef.h>

#define MAX_NAME_LEN 128
#define MAX_DESC_LEN 512
#define MAX_LOC_LEN 128
#define MAX_SHORT_LEN 64
#define MAX_DATE_LEN 32

/* Core Item structure representing both Lost and Found entities */
typedef struct {
    int item_id;
    char item_type[16];           /* "LOST" or "FOUND" */
    char item_name[MAX_NAME_LEN];
    char category[MAX_SHORT_LEN];
    char description[MAX_DESC_LEN];
    char location[MAX_LOC_LEN];
    char reported_by[MAX_NAME_LEN];
    char student_id[MAX_SHORT_LEN];
    char contact_info[MAX_NAME_LEN];
    char report_date[MAX_DATE_LEN];
    char priority[MAX_SHORT_LEN];  /* "NORMAL", "IMPORTANT", "VERY_IMPORTANT" */
    char status[MAX_SHORT_LEN];    /* "LOST", "FOUND", "MATCHED", "CLAIMED", "RETURNED" */
    char storage_location[MAX_LOC_LEN];
    char identifying_details[MAX_DESC_LEN];
    char created_at[MAX_DATE_LEN * 2];
    char updated_at[MAX_DATE_LEN * 2];
} Item;

/* Match score calculation and record */
typedef struct {
    int match_id;
    int lost_item_id;
    int found_item_id;
    int category_score;       /* max 40 */
    int location_score;       /* max 30 */
    int description_score;    /* max 20 */
    int date_score;           /* max 10 */
    int total_score;          /* max 100 */
    int is_possible_match;    /* 1 if >= 70, 0 otherwise */
    char match_status[MAX_SHORT_LEN]; /* "POTENTIAL", "CONFIRMED", "DISMISSED" */
    char created_at[MAX_DATE_LEN * 2];
} MatchScore;

/* Detailed match result combining match score and both items */
typedef struct {
    MatchScore score;
    Item lost_item;
    Item found_item;
} DetailedMatch;

/* Claim record */
typedef struct {
    int claim_id;
    int found_item_id;
    char claimant_name[MAX_NAME_LEN];
    char claimant_id[MAX_SHORT_LEN];
    char claimant_contact[MAX_NAME_LEN];
    char verification_response[MAX_DESC_LEN];
    char claim_status[MAX_SHORT_LEN]; /* "PENDING", "APPROVED", "REJECTED" */
    char admin_notes[MAX_DESC_LEN];
    char submitted_at[MAX_DATE_LEN * 2];
    char reviewed_at[MAX_DATE_LEN * 2];
    char reviewed_by[MAX_NAME_LEN];
    Item item; /* Embedded found item */
} Claim;

/* Return record */
typedef struct {
    int return_id;
    int item_id;
    int claim_id;
    char claimant_id[MAX_SHORT_LEN];
    char verified_by[MAX_NAME_LEN];
    char return_date[MAX_DATE_LEN * 2];
    char notes[MAX_DESC_LEN];
} ReturnRecord;

/* System aggregate statistics */
typedef struct {
    int total_lost;
    int total_found;
    int possible_matches;
    int claimed;
    int returned;
    int still_lost;
    char most_common_category[MAX_SHORT_LEN];
    char most_common_location[MAX_LOC_LEN];
    double recovery_rate; /* percentage 0.0 - 100.0 */
} SystemStatistics;

/* Search filter options */
typedef struct {
    char category[MAX_SHORT_LEN];
    char location[MAX_LOC_LEN];
    char query[MAX_NAME_LEN];
    char student_id[MAX_SHORT_LEN];
    char status[MAX_SHORT_LEN];
    char item_type[16];
    char priority[MAX_SHORT_LEN];
} ItemFilter;

/* Registered user for authentication and role control */
typedef struct {
    int user_id;
    char name[MAX_NAME_LEN];
    char student_id[MAX_SHORT_LEN];
    char email[MAX_NAME_LEN];
    char role[MAX_SHORT_LEN];      /* "student" or "admin" */
    char password[MAX_SHORT_LEN];  /* Optional password (e.g. "admin123") */
    char created_at[MAX_DATE_LEN * 2];
} User;

/* Campus Location / Place (Admin Managed) */
typedef struct {
    int location_id;
    char name[MAX_LOC_LEN];
    char description[MAX_DESC_LEN];
    char created_at[MAX_DATE_LEN * 2];
} Location;

/* Item Category (Admin Managed) */
typedef struct {
    int category_id;
    char name[MAX_SHORT_LEN];
    char icon[32];
    char created_at[MAX_DATE_LEN * 2];
} Category;

#endif /* MODELS_H */
