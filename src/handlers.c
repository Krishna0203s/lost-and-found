#define _GNU_SOURCE
#include "handlers.h"
#include "db.h"
#include "matching.h"
#include "validation.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void http_response_free(HttpResponse* res) {
    if (res && res->body) {
        free(res->body);
        res->body = NULL;
        res->body_len = 0;
    }
}

static void set_json_response(HttpResponse* res, int status_code, cJSON* json) {
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    res->status_code = status_code;
    strncpy(res->content_type, "application/json", sizeof(res->content_type) - 1);
    res->body = str;
    res->body_len = str ? strlen(str) : 0;
}

static void set_error_response(HttpResponse* res, int status_code, const char* message) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddFalseToObject(json, "success");
    cJSON_AddStringToObject(json, "error", message);
    cJSON_AddNumberToObject(json, "code", status_code);
    set_json_response(res, status_code, json);
}

/* URL decoding helper */
static void url_decode(char* dst, const char* src, size_t dst_len) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_len - 1) {
        if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* Query parameter extractor */
static int get_query_param(const char* query, const char* key, char* out_val, size_t max_len) {
    if (!query || !key || !out_val) return 0;
    out_val[0] = '\0';

    size_t key_len = strlen(key);
    const char* p = query;

    while (*p) {
        if ((p == query || *(p - 1) == '&') && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char* val_start = p + key_len + 1;
            const char* val_end = strchr(val_start, '&');
            size_t len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

            char temp[512] = {0};
            if (len >= sizeof(temp)) len = sizeof(temp) - 1;
            strncpy(temp, val_start, len);
            temp[len] = '\0';

            url_decode(out_val, temp, max_len);
            return 1;
        }
        p++;
    }
    return 0;
}

static int get_auth_user(const HttpRequest* req, User* out_user) {
    if (!req || !out_user || strlen(req->auth_header) == 0) return 0;

    const char* header = req->auth_header;
    if (strncasecmp(header, "Bearer ", 7) == 0) {
        header += 7;
    }
    while (*header == ' ') header++;
    if (*header == '\0') return 0;

    char token[256] = {0};
    snprintf(token, sizeof(token), "%s", header);

    char part1[64] = {0};
    char part2[64] = {0};
    char part3[64] = {0};

    char* colon1 = strchr(token, ':');
    if (colon1) {
        int len1 = (int)(colon1 - token);
        if (len1 >= (int)sizeof(part1)) len1 = (int)sizeof(part1) - 1;
        snprintf(part1, sizeof(part1), "%.*s", len1, token);

        char* colon2 = strchr(colon1 + 1, ':');
        if (colon2) {
            int len2 = (int)(colon2 - (colon1 + 1));
            if (len2 >= (int)sizeof(part2)) len2 = (int)sizeof(part2) - 1;
            snprintf(part2, sizeof(part2), "%.*s", len2, colon1 + 1);
            snprintf(part3, sizeof(part3), "%s", colon2 + 1);
        } else {
            snprintf(part2, sizeof(part2), "%s", colon1 + 1);
        }
    } else {
        snprintf(part1, sizeof(part1), "%s", token);
    }

    User user;
    memset(&user, 0, sizeof(User));
    int found = 0;
    if (part2[0] != '\0') {
        found = db_get_user_by_identifier(part2, &user);
    }
    if (!found && part1[0] != '\0') {
        found = db_get_user_by_identifier(part1, &user);
    }

    if (found) {
        *out_user = user;
        return 1;
    }
    return 0;
}

static int is_admin_request(const HttpRequest* req) {
    User user;
    if (!get_auth_user(req, &user)) return 0;
    return (strcasecmp(user.role, "admin") == 0);
}

static cJSON* item_to_json_ext(const Item* item, int include_secrets) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "item_id", item->item_id);
    cJSON_AddStringToObject(obj, "item_type", item->item_type);
    cJSON_AddStringToObject(obj, "item_name", item->item_name);
    cJSON_AddStringToObject(obj, "category", item->category);
    cJSON_AddStringToObject(obj, "description", item->description);
    cJSON_AddStringToObject(obj, "location", item->location);
    cJSON_AddStringToObject(obj, "reported_by", item->reported_by);
    cJSON_AddStringToObject(obj, "student_id", item->student_id);
    cJSON_AddStringToObject(obj, "contact_info", item->contact_info);
    cJSON_AddStringToObject(obj, "report_date", item->report_date);
    cJSON_AddStringToObject(obj, "priority", item->priority);
    cJSON_AddStringToObject(obj, "status", item->status);
    cJSON_AddStringToObject(obj, "storage_location", item->storage_location);
    if (include_secrets) {
        cJSON_AddStringToObject(obj, "identifying_details", item->identifying_details);
    }
    cJSON_AddStringToObject(obj, "created_at", item->created_at);
    cJSON_AddStringToObject(obj, "updated_at", item->updated_at);
    return obj;
}

static cJSON* item_to_json(const Item* item) {
    return item_to_json_ext(item, 1);
}

static cJSON* match_to_json_ext(const DetailedMatch* m, int include_secrets) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "match_id", m->score.match_id);
    cJSON_AddNumberToObject(obj, "lost_item_id", m->score.lost_item_id);
    cJSON_AddNumberToObject(obj, "found_item_id", m->score.found_item_id);
    cJSON_AddNumberToObject(obj, "category_score", m->score.category_score);
    cJSON_AddNumberToObject(obj, "location_score", m->score.location_score);
    cJSON_AddNumberToObject(obj, "description_score", m->score.description_score);
    cJSON_AddNumberToObject(obj, "date_score", m->score.date_score);
    cJSON_AddNumberToObject(obj, "total_score", m->score.total_score);
    cJSON_AddBoolToObject(obj, "is_possible_match", m->score.is_possible_match);
    cJSON_AddStringToObject(obj, "match_status", m->score.match_status);

    cJSON_AddItemToObject(obj, "lost_item", item_to_json_ext(&m->lost_item, include_secrets));
    cJSON_AddItemToObject(obj, "found_item", item_to_json_ext(&m->found_item, include_secrets));
    return obj;
}

__attribute__((unused)) static cJSON* match_to_json(const DetailedMatch* m) {
    return match_to_json_ext(m, 1);
}

static cJSON* claim_to_json_ext(const Claim* c, int include_secrets) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "claim_id", c->claim_id);
    cJSON_AddNumberToObject(obj, "found_item_id", c->found_item_id);
    cJSON_AddStringToObject(obj, "claimant_name", c->claimant_name);
    cJSON_AddStringToObject(obj, "claimant_id", c->claimant_id);
    cJSON_AddStringToObject(obj, "claimant_contact", c->claimant_contact);
    if (include_secrets) {
        cJSON_AddStringToObject(obj, "verification_response", c->verification_response);
        cJSON_AddStringToObject(obj, "admin_notes", c->admin_notes);
    }
    cJSON_AddStringToObject(obj, "claim_status", c->claim_status);
    cJSON_AddStringToObject(obj, "submitted_at", c->submitted_at);
    cJSON_AddStringToObject(obj, "reviewed_at", c->reviewed_at);
    cJSON_AddStringToObject(obj, "reviewed_by", c->reviewed_by);

    if (c->item.item_id > 0) {
        cJSON_AddItemToObject(obj, "found_item", item_to_json_ext(&c->item, include_secrets));
    }
    return obj;
}

static cJSON* claim_to_json(const Claim* c) {
    return claim_to_json_ext(c, 1);
}

static cJSON* user_to_json(const User* u) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "user_id", u->user_id);
    cJSON_AddStringToObject(obj, "name", u->name);
    cJSON_AddStringToObject(obj, "student_id", u->student_id);
    cJSON_AddStringToObject(obj, "email", u->email);
    cJSON_AddStringToObject(obj, "role", u->role);
    cJSON_AddStringToObject(obj, "created_at", u->created_at);
    return obj;
}

static cJSON* location_to_json(const Location* loc) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "location_id", loc->location_id);
    cJSON_AddStringToObject(obj, "name", loc->name);
    cJSON_AddStringToObject(obj, "description", loc->description);
    cJSON_AddStringToObject(obj, "created_at", loc->created_at);
    return obj;
}

static cJSON* category_to_json(const Category* cat) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "category_id", cat->category_id);
    cJSON_AddStringToObject(obj, "name", cat->name);
    cJSON_AddStringToObject(obj, "icon", cat->icon);
    cJSON_AddStringToObject(obj, "created_at", cat->created_at);
    return obj;
}

static void parse_item_from_json(cJSON* root, Item* item) {
    memset(item, 0, sizeof(Item));

    cJSON* it = cJSON_GetObjectItem(root, "item_name");
    if (it && cJSON_IsString(it)) strncpy(item->item_name, it->valuestring, sizeof(item->item_name) - 1);

    it = cJSON_GetObjectItem(root, "category");
    if (it && cJSON_IsString(it)) strncpy(item->category, it->valuestring, sizeof(item->category) - 1);

    it = cJSON_GetObjectItem(root, "description");
    if (it && cJSON_IsString(it)) strncpy(item->description, it->valuestring, sizeof(item->description) - 1);

    it = cJSON_GetObjectItem(root, "location");
    if (it && cJSON_IsString(it)) strncpy(item->location, it->valuestring, sizeof(item->location) - 1);

    it = cJSON_GetObjectItem(root, "reported_by");
    if (it && cJSON_IsString(it)) strncpy(item->reported_by, it->valuestring, sizeof(item->reported_by) - 1);

    it = cJSON_GetObjectItem(root, "student_id");
    if (it && cJSON_IsString(it)) strncpy(item->student_id, it->valuestring, sizeof(item->student_id) - 1);

    it = cJSON_GetObjectItem(root, "contact_info");
    if (it && cJSON_IsString(it)) strncpy(item->contact_info, it->valuestring, sizeof(item->contact_info) - 1);

    it = cJSON_GetObjectItem(root, "report_date");
    if (it && cJSON_IsString(it)) strncpy(item->report_date, it->valuestring, sizeof(item->report_date) - 1);

    it = cJSON_GetObjectItem(root, "priority");
    if (it && cJSON_IsString(it)) strncpy(item->priority, it->valuestring, sizeof(item->priority) - 1);

    it = cJSON_GetObjectItem(root, "storage_location");
    if (it && cJSON_IsString(it)) strncpy(item->storage_location, it->valuestring, sizeof(item->storage_location) - 1);

    it = cJSON_GetObjectItem(root, "identifying_details");
    if (it && cJSON_IsString(it)) strncpy(item->identifying_details, it->valuestring, sizeof(item->identifying_details) - 1);
}

void handle_api_request(const HttpRequest* req, HttpResponse* res) {
    /* 1. Statistics: GET /api/statistics */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/statistics") == 0) {
        SystemStatistics stats;
        if (!db_get_statistics(&stats)) {
            set_error_response(res, 500, "Failed to retrieve statistics");
            return;
        }

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON* data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "total_lost", stats.total_lost);
        cJSON_AddNumberToObject(data, "total_found", stats.total_found);
        cJSON_AddNumberToObject(data, "possible_matches", stats.possible_matches);
        cJSON_AddNumberToObject(data, "claimed", stats.claimed);
        cJSON_AddNumberToObject(data, "returned", stats.returned);
        cJSON_AddNumberToObject(data, "still_lost", stats.still_lost);
        cJSON_AddStringToObject(data, "most_common_category", stats.most_common_category);
        cJSON_AddStringToObject(data, "most_common_location", stats.most_common_location);
        cJSON_AddNumberToObject(data, "recovery_rate", stats.recovery_rate);
        cJSON_AddItemToObject(json, "data", data);

        set_json_response(res, 200, json);
        return;
    }

    /* 2. Seed / Reset Data: POST /api/seed */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/seed") == 0) {
        if (!is_admin_request(req)) {
            set_error_response(res, 403, "Access Denied: Administrator privileges required to reseed database");
            return;
        }
        if (!db_seed()) {
            set_error_response(res, 500, "Failed to seed database");
            return;
        }
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Database successfully seeded with realistic campus data");
        set_json_response(res, 200, json);
        return;
    }

    /* 3. Report Lost Item: POST /api/items/lost */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/items/lost") == 0) {
        cJSON* root = cJSON_Parse(req->body ? req->body : "");
        if (!root) {
            set_error_response(res, 400, "Invalid JSON payload");
            return;
        }

        Item item;
        parse_item_from_json(root, &item);
        cJSON_Delete(root);

        strcpy(item.item_type, "LOST");
        strcpy(item.status, "LOST");
        if (strlen(item.priority) == 0) strcpy(item.priority, "NORMAL");

        char err[256];
        if (!validate_item_fields(&item, err, sizeof(err))) {
            set_error_response(res, 400, err);
            return;
        }

        int new_id = 0;
        if (!db_create_item(&item, &new_id)) {
            set_error_response(res, 500, "Failed to insert lost item into database");
            return;
        }

        Item created;
        db_get_item(new_id, &created);

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Lost item reported successfully");
        cJSON_AddItemToObject(json, "item", item_to_json(&created));
        set_json_response(res, 201, json);
        return;
    }

    /* 4. Report Found Item: POST /api/items/found */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/items/found") == 0) {
        cJSON* root = cJSON_Parse(req->body ? req->body : "");
        if (!root) {
            set_error_response(res, 400, "Invalid JSON payload");
            return;
        }

        Item item;
        parse_item_from_json(root, &item);
        cJSON_Delete(root);

        strcpy(item.item_type, "FOUND");
        strcpy(item.status, "FOUND");
        if (strlen(item.priority) == 0) strcpy(item.priority, "NORMAL");

        char err[256];
        if (!validate_item_fields(&item, err, sizeof(err))) {
            set_error_response(res, 400, err);
            return;
        }

        int new_id = 0;
        if (!db_create_item(&item, &new_id)) {
            set_error_response(res, 500, "Failed to insert found item into database");
            return;
        }

        Item created;
        db_get_item(new_id, &created);

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Found item reported successfully");
        cJSON_AddItemToObject(json, "item", item_to_json(&created));
        set_json_response(res, 201, json);
        return;
    }

    /* 5. List Lost Items: GET /api/items/lost */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/items/lost") == 0) {
        Item* items = NULL;
        int count = 0;
        db_list_items_by_type("LOST", &items, &count);

        int is_admin = is_admin_request(req);
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddNumberToObject(json, "count", count);
        cJSON* arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON_AddItemToArray(arr, item_to_json_ext(&items[i], is_admin));
        }
        cJSON_AddItemToObject(json, "data", arr);
        db_free_items(items);

        set_json_response(res, 200, json);
        return;
    }

    /* 6. List Found Items: GET /api/items/found */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/items/found") == 0) {
        Item* items = NULL;
        int count = 0;
        db_list_items_by_type("FOUND", &items, &count);

        int is_admin = is_admin_request(req);
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddNumberToObject(json, "count", count);
        cJSON* arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON_AddItemToArray(arr, item_to_json_ext(&items[i], is_admin));
        }
        cJSON_AddItemToObject(json, "data", arr);
        db_free_items(items);

        set_json_response(res, 200, json);
        return;
    }

    /* 7. Search Items: GET /api/items/search */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/items/search") == 0) {
        ItemFilter filter = {0};
        get_query_param(req->query, "category", filter.category, sizeof(filter.category));
        get_query_param(req->query, "location", filter.location, sizeof(filter.location));
        get_query_param(req->query, "query", filter.query, sizeof(filter.query));
        get_query_param(req->query, "student_id", filter.student_id, sizeof(filter.student_id));
        get_query_param(req->query, "status", filter.status, sizeof(filter.status));
        get_query_param(req->query, "type", filter.item_type, sizeof(filter.item_type));
        get_query_param(req->query, "priority", filter.priority, sizeof(filter.priority));

        Item* items = NULL;
        int count = 0;
        db_search_items(&filter, &items, &count);

        int is_admin = is_admin_request(req);
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddNumberToObject(json, "count", count);
        cJSON* arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON_AddItemToArray(arr, item_to_json_ext(&items[i], is_admin));
        }
        cJSON_AddItemToObject(json, "data", arr);
        db_free_items(items);

        set_json_response(res, 200, json);
        return;
    }

    /* 8. Matches: GET /api/matches (all automated matches) & POST /api/matches (re-evaluate) */
    if (strcmp(req->path, "/api/matches") == 0) {
        if (strcmp(req->method, "GET") == 0) {
            DetailedMatch* matches = NULL;
            int count = 0;
            db_get_all_matches(&matches, &count, 50);

            int is_admin = is_admin_request(req);
            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "count", count);
            cJSON* arr = cJSON_CreateArray();
            for (int i = 0; i < count; i++) {
                cJSON_AddItemToArray(arr, match_to_json_ext(&matches[i], is_admin));
            }
            cJSON_AddItemToObject(json, "matches", arr);
            db_free_matches(matches);

            set_json_response(res, 200, json);
            return;
        } else if (strcmp(req->method, "POST") == 0) {
            if (!is_admin_request(req)) {
                set_error_response(res, 403, "Access Denied: Administrator privileges required to evaluate matches");
                return;
            }
            int matches_found = 0;
            db_evaluate_all_matches(&matches_found);

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "strong_matches_count", matches_found);
            cJSON_AddStringToObject(json, "message", "Matching engine evaluation completed across all records");
            set_json_response(res, 200, json);
            return;
        }
    }

    /* 9. Item Sub-routes: /api/items/{id}..., /api/items/{id}/matches, /api/items/{id}/return */
    if (strncmp(req->path, "/api/items/", 11) == 0) {
        const char* sub = req->path + 11;
        int item_id = atoi(sub);
        if (item_id > 0) {
            const char* slash = strchr(sub, '/');
            if (!slash) {
                /* GET /api/items/{id} */
                if (strcmp(req->method, "GET") == 0) {
                    Item item;
                    if (!db_get_item(item_id, &item)) {
                        set_error_response(res, 404, "Item not found");
                        return;
                    }
                    User auth_u;
                    int is_admin = is_admin_request(req);
                    int is_owner = (get_auth_user(req, &auth_u) && strcasecmp(auth_u.student_id, item.student_id) == 0);
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", 1);
                    cJSON_AddItemToObject(json, "item", item_to_json_ext(&item, is_admin || is_owner));
                    set_json_response(res, 200, json);
                    return;
                } else if (strcmp(req->method, "DELETE") == 0) {
                    /* DELETE /api/items/{id} */
                    User auth_u;
                    if (!get_auth_user(req, &auth_u)) {
                        set_error_response(res, 401, "Authentication required to delete item");
                        return;
                    }
                    Item item;
                    if (!db_get_item(item_id, &item)) {
                        set_error_response(res, 404, "Item not found");
                        return;
                    }
                    int is_admin = (strcasecmp(auth_u.role, "admin") == 0);
                    int is_owner = (strcasecmp(auth_u.student_id, item.student_id) == 0);
                    if (!is_admin && !is_owner) {
                        set_error_response(res, 403, "Access Denied: Administrator or item owner privileges required to delete item");
                        return;
                    }
                    if (!db_delete_item(item_id)) {
                        set_error_response(res, 400, "Failed to delete item from database");
                        return;
                    }
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", 1);
                    cJSON_AddStringToObject(json, "message", "Item deleted successfully");
                    set_json_response(res, 200, json);
                    return;
                } else if (strcmp(req->method, "PATCH") == 0) {
                    /* PATCH /api/items/{id} */
                    User auth_u;
                    if (!get_auth_user(req, &auth_u)) {
                        set_error_response(res, 401, "Authentication required to update item status");
                        return;
                    }
                    Item item;
                    if (!db_get_item(item_id, &item)) {
                        set_error_response(res, 404, "Item not found");
                        return;
                    }
                    int is_admin = (strcasecmp(auth_u.role, "admin") == 0);
                    int is_owner = (strcasecmp(auth_u.student_id, item.student_id) == 0);
                    if (!is_admin && !is_owner) {
                        set_error_response(res, 403, "Access Denied: Administrator or item owner privileges required to update item");
                        return;
                    }

                    cJSON* root = cJSON_Parse(req->body ? req->body : "");
                    char new_status[32] = {0};
                    if (root) {
                        cJSON* it = cJSON_GetObjectItem(root, "status");
                        if (it && cJSON_IsString(it)) {
                            strncpy(new_status, it->valuestring, sizeof(new_status) - 1);
                        }
                        cJSON_Delete(root);
                    }

                    if (strlen(new_status) == 0) {
                        set_error_response(res, 400, "Status is required");
                        return;
                    }

                    if (!db_update_item_status(item_id, new_status)) {
                        set_error_response(res, 400, "Invalid status transition or update failed");
                        return;
                    }

                    Item updated;
                    db_get_item(item_id, &updated);
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", 1);
                    cJSON_AddStringToObject(json, "message", "Item updated successfully");
                    cJSON_AddItemToObject(json, "item", item_to_json_ext(&updated, is_admin || is_owner));
                    set_json_response(res, 200, json);
                    return;
                }
            } else if (strcmp(slash, "/matches") == 0 && strcmp(req->method, "GET") == 0) {
                /* GET /api/items/{id}/matches */
                DetailedMatch* matches = NULL;
                int count = 0;
                db_get_matches_for_item(item_id, &matches, &count);

                int is_admin = is_admin_request(req);
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", 1);
                cJSON_AddNumberToObject(json, "count", count);
                cJSON* arr = cJSON_CreateArray();
                for (int i = 0; i < count; i++) {
                    cJSON_AddItemToArray(arr, match_to_json_ext(&matches[i], is_admin));
                }
                cJSON_AddItemToObject(json, "matches", arr);
                db_free_matches(matches);

                set_json_response(res, 200, json);
                return;
            } else if (strcmp(slash, "/return") == 0 && strcmp(req->method, "POST") == 0) {
                /* POST /api/items/{id}/return */
                if (!is_admin_request(req)) {
                    set_error_response(res, 403, "Access Denied: Administrator privileges required to record item return");
                    return;
                }
                cJSON* root = cJSON_Parse(req->body ? req->body : "");
                int claim_id = 0;
                char claimant_id[64] = "Student";
                char verified_by[128] = "Campus Administrator";
                char notes[256] = "Returned in person at campus lost & found desk";

                if (root) {
                    cJSON* it = cJSON_GetObjectItem(root, "claim_id");
                    if (it && cJSON_IsNumber(it)) claim_id = it->valueint;
                    it = cJSON_GetObjectItem(root, "claimant_id");
                    if (it && cJSON_IsString(it)) strncpy(claimant_id, it->valuestring, sizeof(claimant_id) - 1);
                    it = cJSON_GetObjectItem(root, "verified_by");
                    if (it && cJSON_IsString(it)) strncpy(verified_by, it->valuestring, sizeof(verified_by) - 1);
                    it = cJSON_GetObjectItem(root, "notes");
                    if (it && cJSON_IsString(it)) strncpy(notes, it->valuestring, sizeof(notes) - 1);
                    cJSON_Delete(root);
                }

                Item item_rec;
                if (!db_get_item(item_id, &item_rec)) {
                    set_error_response(res, 404, "Item record not found in database");
                    return;
                }
                if (strcasecmp(item_rec.status, "RETURNED") == 0) {
                    set_error_response(res, 400, "Item has already been officially returned");
                    return;
                }

                if (!db_record_return(item_id, claim_id, claimant_id, verified_by, notes)) {
                    set_error_response(res, 400, "Failed to mark item as returned (check status transition rules)");
                    return;
                }

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", 1);
                cJSON_AddStringToObject(json, "message", "Item successfully marked as RETURNED to rightful owner");
                set_json_response(res, 200, json);
                return;
            }
        }
    }

    /* 10. Submit Claim: POST /api/claims */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/claims") == 0) {
        cJSON* root = cJSON_Parse(req->body ? req->body : "");
        if (!root) {
            set_error_response(res, 400, "Invalid JSON payload");
            return;
        }

        Claim claim = {0};
        cJSON* it = cJSON_GetObjectItem(root, "found_item_id");
        if (it && cJSON_IsNumber(it)) claim.found_item_id = it->valueint;

        it = cJSON_GetObjectItem(root, "claimant_name");
        if (it && cJSON_IsString(it)) strncpy(claim.claimant_name, it->valuestring, sizeof(claim.claimant_name) - 1);

        it = cJSON_GetObjectItem(root, "claimant_id");
        if (it && cJSON_IsString(it)) strncpy(claim.claimant_id, it->valuestring, sizeof(claim.claimant_id) - 1);

        it = cJSON_GetObjectItem(root, "claimant_contact");
        if (it && cJSON_IsString(it)) strncpy(claim.claimant_contact, it->valuestring, sizeof(claim.claimant_contact) - 1);

        it = cJSON_GetObjectItem(root, "verification_response");
        if (it && cJSON_IsString(it)) strncpy(claim.verification_response, it->valuestring, sizeof(claim.verification_response) - 1);

        cJSON_Delete(root);

        char err[256];
        if (!validate_claim_fields(&claim, err, sizeof(err))) {
            set_error_response(res, 400, err);
            return;
        }

        Item target;
        if (!db_get_item(claim.found_item_id, &target)) {
            set_error_response(res, 404, "Found item record not found in database");
            return;
        }
        if (strcasecmp(target.item_type, "FOUND") != 0) {
            set_error_response(res, 400, "Claims can only be filed on found items");
            return;
        }
        if (strcasecmp(target.status, "RETURNED") == 0) {
            set_error_response(res, 400, "This item has already been officially returned to its owner");
            return;
        }

        int new_claim_id = 0;
        if (!db_create_claim(&claim, &new_claim_id)) {
            set_error_response(res, 500, "Failed to submit claim to database");
            return;
        }

        Claim created;
        db_get_claim(new_claim_id, &created);

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Ownership claim submitted and placed in verification queue");
        cJSON_AddItemToObject(json, "claim", claim_to_json(&created));
        set_json_response(res, 201, json);
        return;
    }

    /* 11. List Claims: GET /api/claims */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/claims") == 0) {
        if (!is_admin_request(req)) {
            set_error_response(res, 403, "Access Denied: Administrator privileges required to access claims list");
            return;
        }

        Claim* claims = NULL;
        int count = 0;
        db_list_claims(&claims, &count);

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddNumberToObject(json, "count", count);
        cJSON* arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON_AddItemToArray(arr, claim_to_json(&claims[i]));
        }
        cJSON_AddItemToObject(json, "claims", arr);
        db_free_claims(claims);

        set_json_response(res, 200, json);
        return;
    }

    /* 12. Claims Sub-routes: GET /api/claims/{id}, PATCH /api/claims/{id} */
    if (strncmp(req->path, "/api/claims/", 12) == 0) {
        int claim_id = atoi(req->path + 12);
        if (claim_id > 0) {
            if (strcmp(req->method, "GET") == 0) {
                Claim claim;
                if (!db_get_claim(claim_id, &claim)) {
                    set_error_response(res, 404, "Claim not found");
                    return;
                }
                User auth_u;
                int is_admin = is_admin_request(req);
                int is_claimant = (get_auth_user(req, &auth_u) && strcasecmp(auth_u.student_id, claim.claimant_id) == 0);
                if (!is_admin && !is_claimant) {
                    set_error_response(res, 403, "Access Denied: You do not have permission to view this claim");
                    return;
                }

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", 1);
                cJSON_AddItemToObject(json, "claim", claim_to_json_ext(&claim, is_admin));
                set_json_response(res, 200, json);
                return;
            } else if (strcmp(req->method, "PATCH") == 0) {
                /* Admin review claim */
                if (!is_admin_request(req)) {
                    set_error_response(res, 403, "Access Denied: Administrator privileges required to review claims");
                    return;
                }

                cJSON* root = cJSON_Parse(req->body ? req->body : "");
                if (!root) {
                    set_error_response(res, 400, "Invalid JSON payload");
                    return;
                }

                char status[32] = "APPROVED";
                char reviewer[128] = "Campus Admin";
                char notes[512] = "";

                cJSON* it = cJSON_GetObjectItem(root, "status");
                if (it && cJSON_IsString(it)) strncpy(status, it->valuestring, sizeof(status) - 1);
                it = cJSON_GetObjectItem(root, "reviewed_by");
                if (it && cJSON_IsString(it)) strncpy(reviewer, it->valuestring, sizeof(reviewer) - 1);
                it = cJSON_GetObjectItem(root, "admin_notes");
                if (it && cJSON_IsString(it)) strncpy(notes, it->valuestring, sizeof(notes) - 1);

                cJSON_Delete(root);

                if (!db_review_claim(claim_id, status, reviewer, notes)) {
                    set_error_response(res, 400, "Failed to update claim review status");
                    return;
                }

                Claim updated;
                db_get_claim(claim_id, &updated);

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", 1);
                cJSON_AddStringToObject(json, "message", "Claim review updated successfully");
                cJSON_AddItemToObject(json, "claim", claim_to_json(&updated));
                set_json_response(res, 200, json);
                return;
            }
        }
    }

    /* 13. User Authentication Login: POST /api/auth/login */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/auth/login") == 0) {
        cJSON* root = cJSON_Parse(req->body ? req->body : "");
        char identifier[128] = {0};
        char password[128] = {0};
        if (root) {
            cJSON* it = cJSON_GetObjectItem(root, "identifier");
            if (!it) it = cJSON_GetObjectItem(root, "student_id");
            if (!it) it = cJSON_GetObjectItem(root, "email");
            if (!it) it = cJSON_GetObjectItem(root, "username");
            if (it && cJSON_IsString(it)) {
                strncpy(identifier, it->valuestring, sizeof(identifier) - 1);
            }
            cJSON* pw = cJSON_GetObjectItem(root, "password");
            if (!pw) pw = cJSON_GetObjectItem(root, "pass");
            if (pw && cJSON_IsString(pw)) {
                strncpy(password, pw->valuestring, sizeof(password) - 1);
            }
            cJSON_Delete(root);
        }

        if (strlen(identifier) == 0) {
            set_error_response(res, 400, "Identifier (username, student ID, or email) is required");
            return;
        }

        User user;
        if (!db_get_user_by_identifier(identifier, &user)) {
            set_error_response(res, 401, "Invalid credentials: No registered account found for the provided identifier");
            return;
        }

        /* If account has a password, verify password */
        if (user.password[0] != '\0') {
            if (strlen(password) == 0 || strcmp(user.password, password) != 0) {
                set_error_response(res, 401, "Invalid credentials: Incorrect password");
                return;
            }
        }

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "User authenticated successfully");
        cJSON_AddItemToObject(json, "user", user_to_json(&user));
        set_json_response(res, 200, json);
        return;
    }

    /* 14. Users CRUD: GET /api/users, POST /api/users, DELETE /api/users/{id} */
    if (strcmp(req->path, "/api/users") == 0) {
        if (strcmp(req->method, "GET") == 0) {
            if (!is_admin_request(req)) {
                set_error_response(res, 403, "Access Denied: Administrator privileges required to view user list");
                return;
            }

            User* users = NULL;
            int count = 0;
            db_list_users(&users, &count);

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "count", count);
            cJSON* arr = cJSON_CreateArray();
            for (int i = 0; i < count; i++) {
                cJSON_AddItemToArray(arr, user_to_json(&users[i]));
            }
            cJSON_AddItemToObject(json, "users", arr);
            db_free_users(users);

            set_json_response(res, 200, json);
            return;
        } else if (strcmp(req->method, "POST") == 0) {
            cJSON* root = cJSON_Parse(req->body ? req->body : "");
            if (!root) {
                set_error_response(res, 400, "Invalid JSON payload");
                return;
            }

            User u = {0};
            cJSON* it = cJSON_GetObjectItem(root, "name");
            if (it && cJSON_IsString(it)) strncpy(u.name, it->valuestring, sizeof(u.name) - 1);
            it = cJSON_GetObjectItem(root, "student_id");
            if (it && cJSON_IsString(it)) strncpy(u.student_id, it->valuestring, sizeof(u.student_id) - 1);
            it = cJSON_GetObjectItem(root, "email");
            if (it && cJSON_IsString(it)) strncpy(u.email, it->valuestring, sizeof(u.email) - 1);
            it = cJSON_GetObjectItem(root, "role");
            if (it && cJSON_IsString(it)) strncpy(u.role, it->valuestring, sizeof(u.role) - 1);
            it = cJSON_GetObjectItem(root, "password");
            if (!it) it = cJSON_GetObjectItem(root, "pass");
            if (it && cJSON_IsString(it)) strncpy(u.password, it->valuestring, sizeof(u.password) - 1);
            cJSON_Delete(root);

            if (strlen(u.name) == 0 || strlen(u.student_id) == 0 || strlen(u.email) == 0) {
                set_error_response(res, 400, "Name, student/staff ID, and email are required fields");
                return;
            }

            if (strlen(u.role) == 0) {
                strncpy(u.role, "student", sizeof(u.role) - 1);
            }

            if (strcasecmp(u.role, "admin") == 0) {
                if (!is_admin_request(req)) {
                    set_error_response(res, 403, "Access Denied: Administrator privileges required to create admin accounts");
                    return;
                }
            }

            int new_id = 0;
            if (!db_create_user(&u, &new_id)) {
                set_error_response(res, 400, "Failed to create user: ID or email may already be registered");
                return;
            }

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "user_id", new_id);
            cJSON_AddStringToObject(json, "message", "User registered successfully");
            set_json_response(res, 201, json);
            return;
        }
    }

    if (strncmp(req->path, "/api/users/", 11) == 0 && strcmp(req->method, "DELETE") == 0) {
        if (!is_admin_request(req)) {
            set_error_response(res, 403, "Access Denied: Administrator privileges required to delete users");
            return;
        }

        int user_id = atoi(req->path + 11);
        if (user_id <= 0) {
            set_error_response(res, 400, "Invalid user ID");
            return;
        }

        if (!db_delete_user(user_id)) {
            set_error_response(res, 400, "Failed to remove user: account not found or cannot delete sole administrator");
            return;
        }

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "User removed from system successfully");
        set_json_response(res, 200, json);
        return;
    }

    /* 15. Locations: GET /api/locations, POST /api/locations, DELETE /api/locations/{id} */
    if (strcmp(req->path, "/api/locations") == 0) {
        if (strcmp(req->method, "GET") == 0) {
            Location* locs = NULL;
            int count = 0;
            db_list_locations(&locs, &count);

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "count", count);
            cJSON* arr = cJSON_CreateArray();
            for (int i = 0; i < count; i++) {
                cJSON_AddItemToArray(arr, location_to_json(&locs[i]));
            }
            cJSON_AddItemToObject(json, "locations", arr);
            db_free_locations(locs);

            set_json_response(res, 200, json);
            return;
        } else if (strcmp(req->method, "POST") == 0) {
            if (!is_admin_request(req)) {
                set_error_response(res, 403, "Access Denied: Administrator privileges required to manage campus locations");
                return;
            }

            cJSON* root = cJSON_Parse(req->body ? req->body : "");
            if (!root) {
                set_error_response(res, 400, "Invalid JSON payload");
                return;
            }

            Location loc = {0};
            cJSON* it = cJSON_GetObjectItem(root, "name");
            if (it && cJSON_IsString(it)) strncpy(loc.name, it->valuestring, sizeof(loc.name) - 1);
            it = cJSON_GetObjectItem(root, "description");
            if (it && cJSON_IsString(it)) strncpy(loc.description, it->valuestring, sizeof(loc.description) - 1);
            cJSON_Delete(root);

            if (strlen(loc.name) == 0) {
                set_error_response(res, 400, "Place / location name is required");
                return;
            }

            int new_id = 0;
            if (!db_create_location(&loc, &new_id)) {
                set_error_response(res, 400, "Failed to create place: name may already exist");
                return;
            }

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "location_id", new_id);
            cJSON_AddStringToObject(json, "message", "Campus location added successfully");
            set_json_response(res, 201, json);
            return;
        }
    }

    if (strncmp(req->path, "/api/locations/", 15) == 0 && strcmp(req->method, "DELETE") == 0) {
        if (!is_admin_request(req)) {
            set_error_response(res, 403, "Access Denied: Administrator privileges required to manage campus locations");
            return;
        }

        int loc_id = atoi(req->path + 15);
        if (loc_id <= 0 || !db_delete_location(loc_id)) {
            set_error_response(res, 400, "Failed to delete location");
            return;
        }
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Campus location removed successfully");
        set_json_response(res, 200, json);
        return;
    }

    /* 16. Categories: GET /api/categories, POST /api/categories, DELETE /api/categories/{id} */
    if (strcmp(req->path, "/api/categories") == 0) {
        if (strcmp(req->method, "GET") == 0) {
            Category* cats = NULL;
            int count = 0;
            db_list_categories(&cats, &count);

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "count", count);
            cJSON* arr = cJSON_CreateArray();
            for (int i = 0; i < count; i++) {
                cJSON_AddItemToArray(arr, category_to_json(&cats[i]));
            }
            cJSON_AddItemToObject(json, "categories", arr);
            db_free_categories(cats);

            set_json_response(res, 200, json);
            return;
        } else if (strcmp(req->method, "POST") == 0) {
            if (!is_admin_request(req)) {
                set_error_response(res, 403, "Access Denied: Administrator privileges required to manage categories");
                return;
            }

            cJSON* root = cJSON_Parse(req->body ? req->body : "");
            if (!root) {
                set_error_response(res, 400, "Invalid JSON payload");
                return;
            }

            Category cat = {0};
            cJSON* it = cJSON_GetObjectItem(root, "name");
            if (it && cJSON_IsString(it)) strncpy(cat.name, it->valuestring, sizeof(cat.name) - 1);
            it = cJSON_GetObjectItem(root, "icon");
            if (it && cJSON_IsString(it)) strncpy(cat.icon, it->valuestring, sizeof(cat.icon) - 1);
            cJSON_Delete(root);

            if (strlen(cat.name) == 0) {
                set_error_response(res, 400, "Category name is required");
                return;
            }
            if (strlen(cat.icon) == 0) strcpy(cat.icon, "📦");

            int new_id = 0;
            if (!db_create_category(&cat, &new_id)) {
                set_error_response(res, 400, "Failed to create category: name may already exist");
                return;
            }

            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "success", 1);
            cJSON_AddNumberToObject(json, "category_id", new_id);
            cJSON_AddStringToObject(json, "message", "Category added successfully");
            set_json_response(res, 201, json);
            return;
        }
    }

    if (strncmp(req->path, "/api/categories/", 16) == 0 && strcmp(req->method, "DELETE") == 0) {
        if (!is_admin_request(req)) {
            set_error_response(res, 403, "Access Denied: Administrator privileges required to manage categories");
            return;
        }

        int cat_id = atoi(req->path + 16);
        if (cat_id <= 0 || !db_delete_category(cat_id)) {
            set_error_response(res, 400, "Failed to delete category");
            return;
        }
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON_AddStringToObject(json, "message", "Category removed successfully");
        set_json_response(res, 200, json);
        return;
    }

    /* 17. C Core Architecture & Telemetry: GET /api/c-info */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/c-info") == 0) {
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);

        cJSON* data = cJSON_CreateObject();

        #ifdef __VERSION__
        cJSON_AddStringToObject(data, "compiler", "GCC " __VERSION__);
        #elif defined(__clang_version__)
        cJSON_AddStringToObject(data, "compiler", "Clang " __clang_version__);
        #else
        cJSON_AddStringToObject(data, "compiler", "Standard C Compiler");
        #endif

        #if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201710L)
        cJSON_AddStringToObject(data, "c_standard", "ISO/IEC 9899:2018 (C17)");
        #elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
        cJSON_AddStringToObject(data, "c_standard", "ISO/IEC 9899:2011 (C11)");
        #elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
        cJSON_AddStringToObject(data, "c_standard", "ISO/IEC 9899:1999 (C99)");
        #else
        cJSON_AddStringToObject(data, "c_standard", "ANSI X3.159-1989 (C89/C90)");
        #endif

        #ifdef _WIN32
        cJSON_AddStringToObject(data, "platform", "Windows x86_64 (Winsock2)");
        #elif defined(__linux__)
        cJSON_AddStringToObject(data, "platform", "Linux x86_64 (POSIX.1-2008)");
        #elif defined(__APPLE__)
        cJSON_AddStringToObject(data, "platform", "macOS Darwin (POSIX)");
        #else
        cJSON_AddStringToObject(data, "platform", "Unix-like POSIX System");
        #endif

        cJSON_AddStringToObject(data, "concurrency", "POSIX Threads (pthread) - Multi-threaded Worker Pool");
        cJSON_AddStringToObject(data, "networking", "BSD / POSIX Sockets (HTTP/1.1 Non-blocking Dispatcher)");
        cJSON_AddStringToObject(data, "database_engine", db_get_sqlite_version());

        cJSON* structs = cJSON_CreateObject();
        cJSON_AddNumberToObject(structs, "Item", sizeof(Item));
        cJSON_AddNumberToObject(structs, "MatchScore", sizeof(MatchScore));
        cJSON_AddNumberToObject(structs, "DetailedMatch", sizeof(DetailedMatch));
        cJSON_AddNumberToObject(structs, "Claim", sizeof(Claim));
        cJSON_AddNumberToObject(structs, "User", sizeof(User));
        cJSON_AddNumberToObject(structs, "Location", sizeof(Location));
        cJSON_AddNumberToObject(structs, "Category", sizeof(Category));
        cJSON_AddNumberToObject(structs, "SystemStatistics", sizeof(SystemStatistics));
        cJSON_AddItemToObject(data, "struct_bytes", structs);

        cJSON* algo = cJSON_CreateObject();
        cJSON_AddNumberToObject(algo, "max_category_score", 40);
        cJSON_AddNumberToObject(algo, "max_location_score", 30);
        cJSON_AddNumberToObject(algo, "max_description_score", 20);
        cJSON_AddNumberToObject(algo, "max_date_score", 10);
        cJSON_AddNumberToObject(algo, "match_threshold", 70);
        cJSON_AddStringToObject(algo, "formula", "Total = S_category(max 40) + S_location(max 30) + S_description(max 20) + S_date(max 10)");
        cJSON_AddItemToObject(data, "matching_engine", algo);

        cJSON_AddItemToObject(json, "data", data);
        set_json_response(res, 200, json);
        return;
    }

    /* 18. C Matching Algorithm Realtime Tester: POST /api/test-matching */
    if (strcmp(req->method, "POST") == 0 && strcmp(req->path, "/api/test-matching") == 0) {
        cJSON* root = cJSON_Parse(req->body ? req->body : "");
        if (!root) {
            set_error_response(res, 400, "Invalid JSON payload");
            return;
        }

        cJSON* j_lost = cJSON_GetObjectItem(root, "lost");
        cJSON* j_found = cJSON_GetObjectItem(root, "found");
        if (!j_lost || !j_found) {
            cJSON_Delete(root);
            set_error_response(res, 400, "Both 'lost' and 'found' objects are required");
            return;
        }

        Item lost_item;
        Item found_item;
        parse_item_from_json(j_lost, &lost_item);
        parse_item_from_json(j_found, &found_item);
        cJSON_Delete(root);

        MatchScore score;
        int is_match = calculate_match_score(&lost_item, &found_item, &score);

        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", 1);
        cJSON* res_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(res_data, "category_score", score.category_score);
        cJSON_AddNumberToObject(res_data, "location_score", score.location_score);
        cJSON_AddNumberToObject(res_data, "description_score", score.description_score);
        cJSON_AddNumberToObject(res_data, "date_score", score.date_score);
        cJSON_AddNumberToObject(res_data, "total_score", score.total_score);
        cJSON_AddBoolToObject(res_data, "is_possible_match", is_match);
        cJSON_AddStringToObject(res_data, "match_status", score.match_status);
        cJSON_AddItemToObject(json, "data", res_data);

        set_json_response(res, 200, json);
        return;
    }

    /* Fallthrough: route not found */
    set_error_response(res, 404, "Endpoint not found");
}
