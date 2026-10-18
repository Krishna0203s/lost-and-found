/**
 * Campus Lost & Found — API Client Module
 * Provides unified interface for communication with C HTTP Backend.
 */

const API = {
    baseUrl: window.location.origin.includes('http') ? window.location.origin : 'http://localhost:8080',

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        const headers = {
            'Content-Type': 'application/json',
            ...(options.headers || {})
        };

        try {
            const userStr = localStorage.getItem('campus_lostfound_user');
            if (userStr && !headers['Authorization']) {
                const u = JSON.parse(userStr);
                if (u && u.student_id && u.role) {
                    headers['Authorization'] = `Bearer ${u.role}:${u.student_id}:${u.user_id}`;
                }
            }
        } catch (_) {}

        try {
            const response = await fetch(url, { ...options, headers });
            let data;
            try {
                data = await response.json();
            } catch (_) {
                // ponytail: plain-text error fallback; upgrade to real auth tokens when needed
                throw new Error(`Server error (HTTP ${response.status}). Is the campus server running?`);
            }
            if (!response.ok) {
                throw new Error(data.error || `HTTP error ${response.status}`);
            }
            return data;
        } catch (err) {
            console.error(`API Error on ${endpoint}:`, err);
            throw err;
        }
    },

    // 1. Statistics
    async getStatistics() {
        const res = await this.request('/api/statistics');
        return res.data;
    },

    // 2. Items Search & Lists
    async searchItems(params = {}) {
        const queryParams = new URLSearchParams();
        for (const [key, val] of Object.entries(params)) {
            if (val !== undefined && val !== null && val !== '') {
                queryParams.append(key, val);
            }
        }
        const qs = queryParams.toString();
        const endpoint = qs ? `/api/items/search?${qs}` : '/api/items/search';
        const res = await this.request(endpoint);
        return res.data || [];
    },

    async getLostItems() {
        const res = await this.request('/api/items/lost');
        return res.data || [];
    },

    async getFoundItems() {
        const res = await this.request('/api/items/found');
        return res.data || [];
    },

    async getItem(id) {
        const res = await this.request(`/api/items/${id}`);
        return res.item;
    },

    // 3. Reporting
    async reportLostItem(itemData) {
        return await this.request('/api/items/lost', {
            method: 'POST',
            body: JSON.stringify(itemData)
        });
    },

    async reportFoundItem(itemData) {
        return await this.request('/api/items/found', {
            method: 'POST',
            body: JSON.stringify(itemData)
        });
    },

    // 4. Matching Engine
    async getItemMatches(itemId) {
        const res = await this.request(`/api/items/${itemId}/matches`);
        return res.matches || [];
    },

    async getAllMatches() {
        const res = await this.request('/api/matches');
        return res.matches || [];
    },

    async refreshAllMatches() {
        return await this.request('/api/matches', {
            method: 'POST'
        });
    },

    // 5. Item Management (Admin)
    async deleteItem(itemId) {
        return await this.request(`/api/items/${itemId}`, {
            method: 'DELETE'
        });
    },

    async updateItem(itemId, itemData) {
        return await this.request(`/api/items/${itemId}`, {
            method: 'PATCH',
            body: JSON.stringify(itemData)
        });
    },

    // 6. Places / Locations (Admin)
    async getLocations() {
        const res = await this.request('/api/locations');
        return res.locations || [];
    },

    async createLocation(locationData) {
        return await this.request('/api/locations', {
            method: 'POST',
            body: JSON.stringify(locationData)
        });
    },

    async deleteLocation(locationId) {
        return await this.request(`/api/locations/${locationId}`, {
            method: 'DELETE'
        });
    },

    // 7. Categories (Admin)
    async getCategories() {
        const res = await this.request('/api/categories');
        return res.categories || [];
    },

    async createCategory(categoryData) {
        return await this.request('/api/categories', {
            method: 'POST',
            body: JSON.stringify(categoryData)
        });
    },

    async deleteCategory(categoryId) {
        return await this.request(`/api/categories/${categoryId}`, {
            method: 'DELETE'
        });
    },

    // 8. Ownership Claims
    async submitClaim(claimData) {
        return await this.request('/api/claims', {
            method: 'POST',
            body: JSON.stringify(claimData)
        });
    },

    async getClaims() {
        const res = await this.request('/api/claims');
        return res.claims || [];
    },

    async reviewClaim(claimId, status, notes = '', reviewer = 'Campus Admin') {
        return await this.request(`/api/claims/${claimId}`, {
            method: 'PATCH',
            body: JSON.stringify({
                status,
                admin_notes: notes,
                reviewed_by: reviewer
            })
        });
    },

    // 9. Return Handover
    async confirmReturn(itemId, returnData) {
        return await this.request(`/api/items/${itemId}/return`, {
            method: 'POST',
            body: JSON.stringify(returnData)
        });
    },

    // 10. Seed Database Records
    async seedDatabase() {
        return await this.request('/api/seed', {
            method: 'POST'
        });
    },

    // 11. User Authentication
    async login(identifier, password = '') {
        return await this.request('/api/auth/login', {
            method: 'POST',
            body: JSON.stringify({ identifier, password })
        });
    },

    async getUsers() {
        const res = await this.request('/api/users');
        return res.users || [];
    },

    async createUser(userData) {
        return await this.request('/api/users', {
            method: 'POST',
            body: JSON.stringify(userData)
        });
    },

    async deleteUser(userId) {
        return await this.request(`/api/users/${userId}`, {
            method: 'DELETE'
        });
    },

    // 12. C Core Telemetry & Direct Algorithm Testing
    async getCInfo() {
        const res = await this.request('/api/c-info');
        return res.data;
    },

    async testMatchingAlgorithm(lostData, foundData) {
        const res = await this.request('/api/test-matching', {
            method: 'POST',
            body: JSON.stringify({ lost: lostData, found: foundData })
        });
        return res.data;
    }
};

window.API = API;
