#ifndef MATCHING_H
#define MATCHING_H

#include "models.h"

/* Evaluates compatibility between a lost item and a found item.
   Populates MatchScore with category, location, description, date and total scores.
   Returns 1 if score >= 70 (Strong Possible Match), 0 otherwise. */
int calculate_match_score(const Item* lost_item, const Item* found_item, MatchScore* out_score);

/* Compares category strings. Returns score (0 or 40). */
int score_category(const char* cat1, const char* cat2);

/* Compares location strings. Returns score (0, 20, or 30). */
int score_location(const char* loc1, const char* loc2);

/* Compares descriptions using token overlap. Returns score (0 to 20). */
int score_description(const char* desc1, const char* desc2);

/* Compares date strings. Returns score (0, 5, or 10). */
int score_date(const char* date1, const char* date2);

#endif /* MATCHING_H */
