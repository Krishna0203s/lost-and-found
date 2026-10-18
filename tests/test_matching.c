#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "matching.h"
#include "validation.h"

int main() {
    printf("=== Running Lost & Found Matching Engine Unit Tests ===\n");

    Item lost = {0};
    lost.item_id = 1;
    strcpy(lost.item_type, "LOST");
    strcpy(lost.item_name, "Student ID Card & RFID Lanyard");
    strcpy(lost.category, "ID Card");
    strcpy(lost.description, "Navy blue campus lanyard with RFID pass and barcode");
    strcpy(lost.location, "Library 2nd Floor");
    strcpy(lost.reported_by, "Alex Rivera");
    strcpy(lost.report_date, "2026-10-05");
    strcpy(lost.priority, "VERY_IMPORTANT");
    strcpy(lost.status, "LOST");

    Item found = {0};
    found.item_id = 2;
    strcpy(found.item_type, "FOUND");
    strcpy(found.item_name, "Campus ID Card with Navy Lanyard");
    strcpy(found.category, "ID Card");
    strcpy(found.description, "Found campus ID card hanging on chair with navy lanyard");
    strcpy(found.location, "Library 2nd Floor");
    strcpy(found.report_date, "2026-10-05");
    strcpy(found.priority, "VERY_IMPORTANT");
    strcpy(found.status, "FOUND");

    MatchScore score;
    int is_match = calculate_match_score(&lost, &found, &score);

    printf("Category score: %d (expected 40)\n", score.category_score);
    assert(score.category_score == 40);

    printf("Location score: %d (expected 30)\n", score.location_score);
    assert(score.location_score == 30);

    printf("Date score: %d (expected 10)\n", score.date_score);
    assert(score.date_score == 10);

    printf("Description score: %d (expected >= 15)\n", score.description_score);
    assert(score.description_score >= 15);

    printf("Total score: %d (expected >= 95)\n", score.total_score);
    assert(score.total_score >= 95);

    printf("Is possible match: %d (expected 1)\n", is_match);
    assert(is_match == 1);

    // Test distinct items (should not match)
    Item umbrella = {0};
    umbrella.item_id = 10;
    strcpy(umbrella.item_type, "FOUND");
    strcpy(umbrella.item_name, "Red Umbrella");
    strcpy(umbrella.category, "Accessories");
    strcpy(umbrella.description, "Large red rain umbrella");
    strcpy(umbrella.location, "Main Gate");
    strcpy(umbrella.report_date, "2026-09-01");

    MatchScore diff_score;
    int diff_match = calculate_match_score(&lost, &umbrella, &diff_score);
    printf("Distinct items total score: %d (expected < 70)\n", diff_score.total_score);
    assert(diff_match == 0);
    assert(diff_score.total_score < 70);

    printf("\n=== Running Status Transition & Validation Unit Tests ===\n");
    // Test valid and invalid status transitions
    assert(validate_status_transition("LOST", "MATCHED") == 1);
    assert(validate_status_transition("FOUND", "CLAIMED") == 1);
    assert(validate_status_transition("CLAIMED", "RETURNED") == 1);
    assert(validate_status_transition("RETURNED", "LOST") == 0);
    assert(validate_status_transition("RETURNED", "CLAIMED") == 0);
    assert(validate_status_transition("RETURNED", "FOUND") == 0);
    printf("Status transition validation tests passed!\n");

    // Test item field validations
    char err[256];
    assert(validate_item_fields(&lost, err, sizeof(err)) == 1);

    Item invalid_item = {0};
    assert(validate_item_fields(&invalid_item, err, sizeof(err)) == 0);

    strcpy(invalid_item.item_name, "Valid Name");
    strcpy(invalid_item.category, "Electronics");
    strcpy(invalid_item.location, "Library");
    strcpy(invalid_item.item_type, "INVALID_TYPE");
    assert(validate_item_fields(&invalid_item, err, sizeof(err)) == 0);
    printf("Item field validation tests passed!\n");

    printf("\nALL CORE ENGINE & VALIDATION UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
