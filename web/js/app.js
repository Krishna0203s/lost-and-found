/**
 * Campus Lost & Found — Main Application Logic
 * Integrates API Client, Canvas Animations, Filtering, Matching, and Admin Workflows.
 */

document.addEventListener('DOMContentLoaded', () => {
    // Application State
    const AppState = {
        items: [],
        claims: [],
        matches: [],
        categories: [],
        locations: [],
        stats: null,
        activeTab: 'dashboard',
        adminMode: false,
        filters: {
            query: '',
            type: 'ALL',
            category: '',
            location: '',
            status: '',
            priority: ''
        },
    };

    window.AppState = AppState;
    window.renderItems = function(items) { applyFiltersAndRender(); };

    // =========================================================================
    // THEME MANAGER (Light & Dark Mode)
    // =========================================================================
    const ThemeManager = {
        STORAGE_KEY: 'campus_lostfound_theme',

        init() {
            const savedTheme = localStorage.getItem(this.STORAGE_KEY);
            const prefersDark = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
            const theme = savedTheme || (prefersDark ? 'dark' : 'light');

            this.applyTheme(theme);
            this.setupPicker();
        },

        applyTheme(theme) {
            document.documentElement.setAttribute('data-theme', theme);
            if (theme === 'light') {
                document.body.classList.add('light-theme');
            } else {
                document.body.classList.remove('light-theme');
            }
            localStorage.setItem(this.STORAGE_KEY, theme);
            this.updateActiveOption(theme);
        },

        updateActiveOption(theme) {
            document.querySelectorAll('.theme-dropdown-option').forEach(opt => {
                const isSelected = opt.dataset.theme === theme;
                opt.classList.toggle('active', isSelected);
                opt.setAttribute('aria-selected', isSelected ? 'true' : 'false');
            });
        },

        setupPicker() {
            const toggleBtn = document.getElementById('btnThemeToggle');
            const dropdown = document.getElementById('themeDropdown');
            if (!toggleBtn || !dropdown) return;

            toggleBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                const isHidden = dropdown.style.display === 'none' || !dropdown.style.display;
                this.closeAllDropdowns();
                if (isHidden) {
                    dropdown.style.display = 'block';
                    toggleBtn.setAttribute('aria-expanded', 'true');
                    const currentTheme = document.documentElement.getAttribute('data-theme') || 'dark';
                    this.updateActiveOption(currentTheme);
                } else {
                    dropdown.style.display = 'none';
                    toggleBtn.setAttribute('aria-expanded', 'false');
                }
            });

            dropdown.querySelectorAll('.theme-dropdown-option').forEach(opt => {
                opt.addEventListener('click', (e) => {
                    e.stopPropagation();
                    const chosen = opt.dataset.theme;
                    this.applyTheme(chosen);
                    dropdown.style.display = 'none';
                    toggleBtn.setAttribute('aria-expanded', 'false');
                    showToast(chosen === 'light' ? 'Light mode activated ☀️' : 'Dark mode activated 🌙', 'info');
                });
            });

            document.addEventListener('click', (e) => {
                if (!dropdown.contains(e.target) && e.target !== toggleBtn && !toggleBtn.contains(e.target)) {
                    dropdown.style.display = 'none';
                    toggleBtn.setAttribute('aria-expanded', 'false');
                }
            });

            document.addEventListener('keydown', (e) => {
                if (e.key === 'Escape' && dropdown.style.display === 'block') {
                    dropdown.style.display = 'none';
                    toggleBtn.setAttribute('aria-expanded', 'false');
                    toggleBtn.focus();
                }
            });
        },

        closeAllDropdowns() {
            const authDropdown = document.getElementById('authDropdown');
            if (authDropdown) authDropdown.style.display = 'none';
        }
    };

    ThemeManager.init();
    window.ThemeManager = ThemeManager;

    // Initialize Identity & Authentication Manager
    if (typeof AuthManager !== 'undefined') {
        AuthManager.init();
    }

    // =========================================================================
    // NAVIGATION & TAB SWITCHING
    // =========================================================================
    const navTabs = document.querySelectorAll('.nav-tab');
    navTabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const tabId = tab.dataset.tab;
            switchTab(tabId);
        });
    });

    window.switchTab = function(tabId) {
        if (tabId === 'admin' && !AppState.adminMode) {
            showToast('Access Denied: Administrator sign-in required', 'error');
            if (typeof AuthManager !== 'undefined') {
                AuthManager.openAdminLogin();
            }
            return;
        }
        if (tabId === 'claims' && typeof AuthManager !== 'undefined' && !AuthManager.currentUser) {
            showToast('Sign-in required: Please sign in to access Claims Queue', 'info');
            setTimeout(() => { window.location.href = 'login.html?role=student'; }, 400);
            return;
        }
        AppState.activeTab = tabId;
        navTabs.forEach(t => t.classList.toggle('active', t.dataset.tab === tabId));
        document.querySelectorAll('.tab-pane').forEach(p => {
            p.classList.toggle('active', p.id === `tab-${tabId}`);
        });

        // Update active tab title on minimal navigation button
        const activeLabel = document.getElementById('navActiveLabel');
        if (activeLabel) {
            const labels = {
                dashboard: 'Dashboard',
                items: 'Explore Items',
                matcher: 'Auto-Matcher',
                claims: 'Claims Queue',
                admin: 'Admin Control'
            };
            activeLabel.textContent = labels[tabId] || 'Navigation';
        }

        // Close dropdown menu upon navigation
        const navMenuDropdown = document.getElementById('navMenuDropdown');
        if (navMenuDropdown) {
            navMenuDropdown.style.display = 'none';
            const btnNavMenu = document.getElementById('btnNavMenu');
            if (btnNavMenu) {
                btnNavMenu.classList.remove('active');
                btnNavMenu.setAttribute('aria-expanded', 'false');
            }
        }

        if (tabId === 'items') {
            applyFiltersAndRender();
        } else if (tabId === 'claims') {
            loadClaims();
        } else if (tabId === 'matcher') {
            loadAutomatedMatchesFeed();
            populateMatcherDropdown();
        } else if (tabId === 'admin') {
            loadAdminPanelData();
        }
    };

    // =========================================================================
    // DATA FETCHING & SYNCHRONIZATION
    // =========================================================================
    async function loadAllData() {
        try {
            await Promise.all([
                loadStatistics(),
                loadCategoriesAndLocations(),
                loadItems(),
                loadClaims(),
                loadAutomatedMatchesFeed()
            ]);
            checkStudentMatchAlerts();
            if (AppState.activeTab === 'admin') {
                loadAdminPanelData();
            }
        } catch (err) {
            console.error('Data load error:', err);
            showToast('Unable to connect to C backend server', 'error');
        }
    }
    window.loadAllData = loadAllData;

    async function loadStatistics() {
        try {
            const stats = await API.getStatistics();
            AppState.stats = stats;

            animateNumber(document.getElementById('statTotalLost'), stats.total_lost);
            animateNumber(document.getElementById('statTotalFound'), stats.total_found);
            animateNumber(document.getElementById('statMatches'), stats.possible_matches);
            animateNumber(document.getElementById('statReturned'), stats.returned);

            const badgeMatches = document.getElementById('badgeMatches');
            if (badgeMatches) badgeMatches.textContent = stats.possible_matches;

            const statStillLost = document.getElementById('statStillLost');
            if (statStillLost) statStillLost.textContent = `${stats.still_lost} items`;

            const statTopCategory = document.getElementById('statTopCategory');
            if (statTopCategory) statTopCategory.textContent = stats.most_common_category;

            const statTopLocation = document.getElementById('statTopLocation');
            if (statTopLocation) statTopLocation.textContent = stats.most_common_location;

            updateRecoveryDial(stats.recovery_rate);
        } catch (e) {
            console.error('Failed to load statistics', e);
        }
    }

    async function loadItems() {
        try {
            const items = await API.searchItems();
            AppState.items = items;

            const totalBadge = document.getElementById('badgeTotalItems');
            if (totalBadge) totalBadge.textContent = items.length;

            // Update Urgent Alert Marquee
            updateUrgentTicker(items);

            // Populate Stream Grid
            renderStreamCards(items);

            // Render Explore Grid if active
            applyFiltersAndRender();

            // Populate Target Matcher dropdown
            populateMatcherDropdown();

            // Refresh student match alerts banner
            checkStudentMatchAlerts();
        } catch (e) {
            console.error('Failed to load items', e);
        }
    }

    async function loadClaims() {
        const u = typeof AuthManager !== 'undefined' ? AuthManager.currentUser : null;
        if (!u || u.role !== 'admin') {
            AppState.claims = [];
            const badgeClaims = document.getElementById('badgeClaims');
            if (badgeClaims) badgeClaims.textContent = '0';
            renderClaimsList([]);
            return;
        }
        try {
            const claims = await API.getClaims();
            AppState.claims = claims;

            const badgeClaims = document.getElementById('badgeClaims');
            if (badgeClaims) badgeClaims.textContent = claims.length;

            renderClaimsList(claims);
        } catch (e) {
            console.error('Failed to load claims', e);
        }
    }

    async function loadCategoriesAndLocations() {
        try {
            const [cats, locs] = await Promise.all([
                API.getCategories().catch(() => []),
                API.getLocations().catch(() => [])
            ]);

            AppState.categories = cats;
            AppState.locations = locs;

            // Populate Category dropdowns
            const catSelects = [
                document.getElementById('filterCategory'),
                document.getElementById('lostCategory'),
                document.getElementById('foundCategory')
            ];

            catSelects.forEach(sel => {
                if (!sel) return;
                const currentVal = sel.value;
                const isFilter = sel.id === 'filterCategory';
                sel.innerHTML = isFilter ? '<option value="">All Categories</option>' : '<option value="">Select Category</option>';
                cats.forEach(c => {
                    const opt = document.createElement('option');
                    opt.value = c.name;
                    opt.textContent = `${c.icon ? c.icon + ' ' : ''}${c.name}`;
                    sel.appendChild(opt);
                });
                if (currentVal) sel.value = currentVal;
            });

            // Populate Location select in filter
            const filterLoc = document.getElementById('filterLocation');
            if (filterLoc) {
                const currentVal = filterLoc.value;
                filterLoc.innerHTML = '<option value="">All Locations</option>';
                locs.forEach(l => {
                    const opt = document.createElement('option');
                    opt.value = l.name;
                    opt.textContent = l.name;
                    filterLoc.appendChild(opt);
                });
                if (currentVal) filterLoc.value = currentVal;
            }

            // Populate locations datalist for inputs
            const datalist = document.getElementById('locationsDatalist');
            if (datalist) {
                datalist.innerHTML = locs.map(l => `<option value="${escapeHTML(l.name)}">${escapeHTML(l.description || '')}</option>`).join('');
            }
        } catch (err) {
            console.error('Failed to load categories/locations', err);
        }
    }
    window.loadCategoriesAndLocations = loadCategoriesAndLocations;

    // =========================================================================
    // URGENT PRIORITY TICKER
    // =========================================================================
    function updateUrgentTicker(items) {
        const ticker = document.getElementById('urgentTicker');
        const tickerContent = document.getElementById('tickerContent');
        if (!ticker || !tickerContent) return;

        const urgentItems = items.filter(it => it.priority === 'VERY_IMPORTANT' && it.status !== 'RETURNED');
        if (urgentItems.length === 0) {
            ticker.style.display = 'none';
            return;
        }

        ticker.style.display = 'flex';
        const notices = urgentItems.map(it => {
            return `🚨 [URGENT ${it.category.toUpperCase()}] ${it.item_name} reported ${it.item_type} at ${it.location} (${it.report_date}) — Reference #${it.item_id}`;
        }).join(' &nbsp;&nbsp;&nbsp;•&nbsp;&nbsp;&nbsp; ');

        tickerContent.innerHTML = notices;
        tickerContent.onclick = () => {
            switchTab('items');
            const prioSelect = document.getElementById('filterPriority');
            if (prioSelect) {
                prioSelect.value = 'VERY_IMPORTANT';
                prioSelect.dispatchEvent(new Event('change'));
            }
        };
    }

    const tickerClose = document.getElementById('tickerClose');
    if (tickerClose) {
        tickerClose.addEventListener('click', () => {
            document.getElementById('urgentTicker').style.display = 'none';
        });
    }

    // =========================================================================
    // STREAM & ITEM CARDS RENDERING
    // =========================================================================
    function renderStreamCards(items) {
        const container = document.getElementById('recentStreamGrid');
        if (!container) return;

        // Show recent 4 items
        const recent = items.slice(0, 4);
        if (recent.length === 0) {
            container.innerHTML = '<div class="empty-state-card"><p>No items reported yet.</p></div>';
            return;
        }

        container.innerHTML = recent.map(it => createItemCardHTML(it)).join('');
        attachCardActionListeners(container);
        if (typeof animateCardStagger === 'function') animateCardStagger(container);
    }

    function createItemCardHTML(item) {
        const isLost = item.item_type === 'LOST';
        const typeBadge = `<span class="badge ${isLost ? 'badge-lost' : 'badge-found'}">${item.item_type}</span>`;
        const statusBadge = `<span class="badge badge-${item.status.toLowerCase()}">${item.status}</span>`;

        let prioBadge = '';
        if (item.priority === 'VERY_IMPORTANT') {
            prioBadge = '<span class="badge prio-very-important">🚨 Very Important</span>';
        } else if (item.priority === 'IMPORTANT') {
            prioBadge = '<span class="badge prio-important">⚠️ Important</span>';
        }

        const dateStr = item.report_date || 'Unknown';
        const reporterStr = item.reported_by || 'Anonymous';

        return `
            <div class="item-card glass-panel" data-id="${item.item_id}">
                <div>
                    <div class="card-top-row">
                        <div class="badge-group">
                            ${typeBadge}
                            ${statusBadge}
                            ${prioBadge}
                        </div>
                        <span class="card-category-tag">${escapeHTML(item.category)}</span>
                    </div>

                    <h3 class="item-card-title">${escapeHTML(item.item_name)}</h3>
                    <p class="item-card-desc">${escapeHTML(item.description)}</p>

                    <div class="card-meta-list">
                        <div class="meta-row">
                            <span class="meta-k">Location:</span>
                            <span class="meta-v">📍 ${escapeHTML(item.location)}</span>
                        </div>
                        <div class="meta-row">
                            <span class="meta-k">Date:</span>
                            <span class="meta-v">📅 ${escapeHTML(dateStr)}</span>
                        </div>
                        <div class="meta-row">
                            <span class="meta-k">Reported By:</span>
                            <span class="meta-v">👤 ${escapeHTML(reporterStr)}</span>
                        </div>
                        ${item.storage_location ? `
                        <div class="meta-row">
                            <span class="meta-k">Storage:</span>
                            <span class="meta-v">📦 ${escapeHTML(item.storage_location)}</span>
                        </div>` : ''}
                    </div>
                </div>

                <div class="card-actions-bar">
                    <button class="btn-sm btn-ghost btn-card-action btn-inspect" data-id="${item.item_id}">
                        Details
                    </button>
                    <button class="btn-sm btn-outline btn-card-action btn-match-eval" data-id="${item.item_id}">
                        Match
                    </button>
                    ${(!isLost && item.status !== 'CLAIMED' && item.status !== 'RETURNED') ? `
                    <button class="btn-sm btn-found btn-card-action btn-claim-item" data-id="${item.item_id}">
                        Claim
                    </button>` : ''}
                    ${AppState.adminMode && item.status !== 'RETURNED' ? `
                    <button class="btn-sm btn-outline btn-card-action btn-admin-return" data-id="${item.item_id}">
                        Handover
                    </button>` : ''}
                    ${AppState.adminMode ? `
                    <button class="btn-sm btn-ghost btn-card-action btn-admin-delete" style="color:var(--crimson-alert)" data-id="${item.item_id}" title="Delete Item">
                        Delete
                    </button>` : ''}
                </div>
            </div>
        `;
    }

    function attachCardActionListeners(container) {
        container.querySelectorAll('.btn-inspect').forEach(btn => {
            btn.addEventListener('click', () => openItemDetailsModal(parseInt(btn.dataset.id)));
        });

        container.querySelectorAll('.btn-match-radar, .btn-match-eval').forEach(btn => {
            btn.addEventListener('click', () => {
                const id = parseInt(btn.dataset.id);
                switchTab('matcher');
                const select = document.getElementById('matchTargetSelect');
                if (select) {
                    select.value = id;
                    document.getElementById('btnRunTargetMatch').click();
                }
            });
        });

        container.querySelectorAll('.btn-claim-item').forEach(btn => {
            btn.addEventListener('click', () => openClaimModal(parseInt(btn.dataset.id)));
        });

        container.querySelectorAll('.btn-admin-return').forEach(btn => {
            btn.addEventListener('click', () => openReturnModal(parseInt(btn.dataset.id)));
        });

        container.querySelectorAll('.btn-admin-delete').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                e.stopPropagation();
                const id = parseInt(btn.dataset.id);
                if (confirm(`Are you sure you want to permanently delete item #${id}?`)) {
                    try {
                        await API.deleteItem(id);
                        showToast(`Item #${id} deleted successfully`, 'success');
                        await loadAllData();
                    } catch (err) {
                        showToast(err.message || 'Failed to delete item', 'error');
                    }
                }
            });
        });
    }

    // =========================================================================
    // SEARCH & FILTER LOGIC
    // =========================================================================
    const searchInput = document.getElementById('searchInput');
    const btnClearSearch = document.getElementById('btnClearSearch');
    const filterCategory = document.getElementById('filterCategory');
    const filterLocation = document.getElementById('filterLocation');
    const filterStatus = document.getElementById('filterStatus');
    const filterPriority = document.getElementById('filterPriority');
    const typePills = document.querySelectorAll('.type-pill-selector .pill-btn');
    const btnResetFilters = document.getElementById('btnResetFilters');

    let searchDebounceTimer = null;
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            clearTimeout(searchDebounceTimer);
            const val = e.target.value.trim();
            btnClearSearch.style.display = val ? 'block' : 'none';
            searchDebounceTimer = setTimeout(() => {
                AppState.filters.query = val;
                applyFiltersAndRender();
            }, 250);
        });

        // '/' keyboard shortcut to focus search
        window.addEventListener('keydown', (e) => {
            if (e.key === '/' && document.activeElement !== searchInput && !isModalOpen()) {
                e.preventDefault();
                switchTab('items');
                searchInput.focus();
            }
        });
    }

    if (btnClearSearch) {
        btnClearSearch.addEventListener('click', () => {
            searchInput.value = '';
            btnClearSearch.style.display = 'none';
            AppState.filters.query = '';
            applyFiltersAndRender();
        });
    }

    typePills.forEach(pill => {
        pill.addEventListener('click', () => {
            typePills.forEach(p => p.classList.remove('active'));
            pill.classList.add('active');
            AppState.filters.type = pill.dataset.type;
            applyFiltersAndRender();
        });
    });

    [filterCategory, filterLocation, filterStatus, filterPriority].forEach(sel => {
        if (sel) {
            sel.addEventListener('change', () => {
                AppState.filters.category = filterCategory.value;
                AppState.filters.location = filterLocation.value;
                AppState.filters.status = filterStatus.value;
                AppState.filters.priority = filterPriority.value;
                applyFiltersAndRender();
            });
        }
    });

    if (btnResetFilters) {
        btnResetFilters.addEventListener('click', () => {
            searchInput.value = '';
            btnClearSearch.style.display = 'none';
            filterCategory.value = '';
            filterLocation.value = '';
            filterStatus.value = '';
            filterPriority.value = '';
            typePills.forEach(p => p.classList.toggle('active', p.dataset.type === 'ALL'));
            AppState.filters = { query: '', type: 'ALL', category: '', location: '', status: '', priority: '' };
            applyFiltersAndRender();
        });
    }

    function applyFiltersAndRender() {
        const grid = document.getElementById('itemsGrid');
        const countLabel = document.getElementById('resultsCount');
        if (!grid) return;

        let filtered = AppState.items.filter(item => {
            const f = AppState.filters;
            if (f.type !== 'ALL' && item.item_type !== f.type) return false;
            if (f.category && item.category !== f.category) return false;
            if (f.location && !item.location.toLowerCase().includes(f.location.toLowerCase())) return false;
            if (f.status && item.status !== f.status) return false;
            if (f.priority && item.priority !== f.priority) return false;
            if (f.query) {
                const q = f.query.toLowerCase();
                const matchName = item.item_name.toLowerCase().includes(q);
                const matchDesc = item.description.toLowerCase().includes(q);
                const matchRep = item.reported_by && item.reported_by.toLowerCase().includes(q);
                const matchStu = item.student_id && item.student_id.toLowerCase().includes(q);
                const matchLoc = item.location.toLowerCase().includes(q);
                if (!matchName && !matchDesc && !matchRep && !matchStu && !matchLoc) return false;
            }
            return true;
        });

        if (countLabel) countLabel.textContent = `Showing ${filtered.length} of ${AppState.items.length} items`;

        if (filtered.length === 0) {
            grid.innerHTML = `
                <div class="empty-state-card glass-panel" style="grid-column: 1 / -1;">
                    <div class="empty-icon">🔍</div>
                    <h3>No Matching Belongings Found</h3>
                    <p>Try adjusting your search criteria or clearing active category and location filters.</p>
                </div>
            `;
            return;
        }

        grid.innerHTML = filtered.map(it => createItemCardHTML(it)).join('');
        attachCardActionListeners(grid);
        if (typeof animateCardStagger === 'function') animateCardStagger(grid);
    }

    // =========================================================================
    // MATCHING ENGINE & AUTOMATION FEED
    // =========================================================================
    function populateMatcherDropdown() {
        const select = document.getElementById('matchTargetSelect');
        if (!select) return;

        const currentVal = select.value;
        select.innerHTML = '<option value="">-- Choose Lost or Found Item --</option>';

        AppState.items.forEach(it => {
            const opt = document.createElement('option');
            opt.value = it.item_id;
            opt.textContent = `[${it.item_type}] #${it.item_id} — ${it.item_name} (${it.category} • ${it.location})`;
            select.appendChild(opt);
        });

        if (currentVal) select.value = currentVal;
    }

    const btnRunTargetMatch = document.getElementById('btnRunTargetMatch');
    if (btnRunTargetMatch) {
        btnRunTargetMatch.addEventListener('click', async () => {
            const select = document.getElementById('matchTargetSelect');
            const targetId = parseInt(select.value);
            if (!targetId) {
                showToast('Please select an item to match first', 'error');
                return;
            }

            const emptyState = document.getElementById('matchEmptyState');
            const resultsContainer = document.getElementById('matchCardsContainer');

            btnRunTargetMatch.disabled = true;
            btnRunTargetMatch.innerHTML = '<svg class="btn-svg spinner" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10" stroke-opacity="0.25"/><path d="M12 2a10 10 0 0 1 10 10" stroke-linecap="round"/></svg> <span>Evaluating...</span>';

            try {
                const matches = await API.getItemMatches(targetId);
                btnRunTargetMatch.disabled = false;
                btnRunTargetMatch.innerHTML = '<svg class="btn-svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><polygon points="10 8 16 12 10 16 10 8"></polygon></svg> <span>Run Simulation</span>';

                if (emptyState) emptyState.style.display = 'none';
                if (resultsContainer) {
                    resultsContainer.style.display = 'flex';
                    renderMatchResults(matches);
                }

                showToast(`Simulation complete: evaluated against candidate records`, 'success');
            } catch (err) {
                btnRunTargetMatch.disabled = false;
                btnRunTargetMatch.innerHTML = '<svg class="btn-svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><polygon points="10 8 16 12 10 16 10 8"></polygon></svg> <span>Run Simulation</span>';
                showToast('Error during matching evaluation', 'error');
            }
        });
    }

    function renderMatchResults(matches) {
        const container = document.getElementById('matchCardsContainer');
        if (!container) return;

        if (matches.length === 0) {
            container.innerHTML = `
                <div class="empty-state-card glass-panel">
                    <div class="empty-icon">📡</div>
                    <h3>No Opposite Candidates Found</h3>
                    <p>There are currently no items in the opposite database to compare against.</p>
                </div>
            `;
            return;
        }

        container.innerHTML = matches.map(m => {
            const isStrong = m.is_possible_match || m.total_score >= 70;
            const lost = m.lost_item;
            const found = m.found_item;

            return `
                <div class="match-comparison-card glass-panel ${isStrong ? 'high-match' : ''}">
                    <div class="match-score-header">
                        <div class="match-score-badge-large">
                            <div class="score-circle-dial ${!isStrong ? 'score-low' : ''}">
                                ${m.total_score}%
                            </div>
                            <div>
                                <h4 class="score-details-title">${isStrong ? 'STRONG POSSIBLE MATCH' : 'Low Compatibility Match'}</h4>
                                <span class="score-details-sub">${isStrong ? 'Algorithmic confidence exceeds the 70% threshold. Verify details below.' : 'Score below 70% matching threshold.'}</span>
                            </div>
                        </div>

                        ${isStrong && found.status !== 'CLAIMED' && found.status !== 'RETURNED' ? `
                        <button class="btn btn-sm btn-found" onclick="openClaimModal(${found.item_id})">
                            Claim Belonging
                        </button>` : ''}
                    </div>

                    <!-- 4-Factor Breakdown Bars -->
                    <div class="breakdown-bars-grid">
                        <div class="breakdown-item">
                            <div class="breakdown-item-header">
                                <span class="breakdown-name">Category (${m.category_score}/40)</span>
                                <span class="breakdown-score">+${m.category_score} pts</span>
                            </div>
                            <div class="breakdown-bar-bg">
                                <div class="breakdown-bar-fill" style="width: ${(m.category_score / 40) * 100}%"></div>
                            </div>
                        </div>

                        <div class="breakdown-item">
                            <div class="breakdown-item-header">
                                <span class="breakdown-name">Location (${m.location_score}/30)</span>
                                <span class="breakdown-score">+${m.location_score} pts</span>
                            </div>
                            <div class="breakdown-bar-bg">
                                <div class="breakdown-bar-fill" style="width: ${(m.location_score / 30) * 100}%"></div>
                            </div>
                        </div>

                        <div class="breakdown-item">
                            <div class="breakdown-item-header">
                                <span class="breakdown-name">Description (${m.description_score}/20)</span>
                                <span class="breakdown-score">+${m.description_score} pts</span>
                            </div>
                            <div class="breakdown-bar-bg">
                                <div class="breakdown-bar-fill" style="width: ${(m.description_score / 20) * 100}%"></div>
                            </div>
                        </div>

                        <div class="breakdown-item">
                            <div class="breakdown-item-header">
                                <span class="breakdown-name">Date Match (${m.date_score}/10)</span>
                                <span class="breakdown-score">+${m.date_score} pts</span>
                            </div>
                            <div class="breakdown-bar-bg">
                                <div class="breakdown-bar-fill" style="width: ${(m.date_score / 10) * 100}%"></div>
                            </div>
                        </div>
                    </div>

                    <!-- Side-by-side comparison -->
                    <div class="comparison-split-cols">
                        <div class="comparison-box">
                            <h4>🚩 Lost Item: ${escapeHTML(lost.item_name)} (#${lost.item_id})</h4>
                            <p><strong>Category:</strong> ${escapeHTML(lost.category)}</p>
                            <p><strong>Location:</strong> 📍 ${escapeHTML(lost.location)}</p>
                            <p><strong>Date:</strong> 📅 ${escapeHTML(lost.report_date)}</p>
                            <p><strong>Description:</strong> ${escapeHTML(lost.description)}</p>
                            <p><strong>Reported by:</strong> ${escapeHTML(lost.reported_by)}</p>
                        </div>

                        <div class="comparison-box">
                            <h4>💎 Found Item: ${escapeHTML(found.item_name)} (#${found.item_id})</h4>
                            <p><strong>Category:</strong> ${escapeHTML(found.category)}</p>
                            <p><strong>Location:</strong> 📍 ${escapeHTML(found.location)}</p>
                            <p><strong>Date:</strong> 📅 ${escapeHTML(found.report_date)}</p>
                            <p><strong>Description:</strong> ${escapeHTML(found.description)}</p>
                            <p><strong>Storage:</strong> 📦 ${escapeHTML(found.storage_location || 'Campus Front Desk')}</p>
                        </div>
                    </div>
                </div>
            `;
        }).join('');
    }

    // =========================================================================
    // AUTOMATED MATCH FEED & STUDENT ALERTS
    // =========================================================================
    async function loadAutomatedMatchesFeed() {
        const container = document.getElementById('autoMatchesContainer');
        try {
            const matches = await API.getAllMatches();
            AppState.matches = matches || [];

            // Update badge on telemetry or nav if present
            const badgeMatches = document.getElementById('badgeMatches');
            if (badgeMatches && matches) {
                const highConfCount = matches.filter(m => m.is_possible_match || m.total_score >= 70).length;
                badgeMatches.textContent = highConfCount;
            }

            if (!container) return;

            if (!matches || matches.length === 0) {
                container.innerHTML = `
                    <div class="empty-state-card glass-panel" style="padding: 32px; text-align: center;">
                        <div class="empty-icon" style="font-size: 2.2rem; margin-bottom: 12px;">⚡</div>
                        <h3 style="margin-bottom: 8px;">Autonomous Match Engine Active</h3>
                        <p style="color: var(--text-muted); max-width: 500px; margin: 0 auto;">No automated match candidates detected yet. When matching items are reported across campus, they appear here automatically without admin intervention.</p>
                    </div>
                `;
                return;
            }

            container.innerHTML = matches.map(m => {
                const isHighConfidence = m.is_possible_match || m.total_score >= 70;
                const lost = m.lost_item || {};
                const found = m.found_item || {};
                const scoreClass = isHighConfidence ? 'score-high' : 'score-medium';
                const badgeClass = isHighConfidence ? 'badge-auto-pulse' : 'badge-matched';
                const badgeText = isHighConfidence ? '⚡ HIGH-CONFIDENCE MATCH (≥70)' : 'COMPATIBILITY CANDIDATE';

                return `
                    <div class="auto-match-card glass-panel ${isHighConfidence ? 'high-confidence-glow' : ''}" data-match-id="${lost.item_id}-${found.item_id}">
                        <div class="auto-match-header">
                            <div class="match-score-summary">
                                <div class="score-circle-dial ${scoreClass}">
                                    <span class="score-number">${m.total_score}%</span>
                                    <span class="score-label">MATCH</span>
                                </div>
                                <div class="match-title-group">
                                    <div class="badge-row">
                                        <span class="${badgeClass}">${badgeText}</span>
                                        <span class="match-pair-ids">Lost #${lost.item_id} ↔ Found #${found.item_id}</span>
                                    </div>
                                    <h3 class="match-pair-title">
                                        ${escapeHTML(lost.item_name)} <span class="pair-arrow">↔</span> ${escapeHTML(found.item_name)}
                                    </h3>
                                    <p class="match-summary-sub">
                                        Autonomous dual-trigger pairing evaluated across category, location, and description attributes.
                                    </p>
                                </div>
                            </div>

                            <div class="match-action-buttons">
                                ${found.status !== 'CLAIMED' && found.status !== 'RETURNED' ? `
                                <button class="btn btn-sm btn-found" onclick="openClaimModal(${found.item_id})">
                                    Claim Belonging
                                </button>` : ''}
                                ${AppState.adminMode && found.status !== 'RETURNED' ? `
                                <button class="btn btn-sm btn-outline" onclick="openReturnModal(${found.item_id})">
                                    Handover
                                </button>` : ''}
                            </div>
                        </div>

                        <!-- 4-Factor Breakdown Progress Bars -->
                        <div class="breakdown-bars-grid">
                            <div class="breakdown-item">
                                <div class="breakdown-item-header">
                                    <span class="breakdown-name">Category (${m.category_score}/40)</span>
                                    <span class="breakdown-score">+${m.category_score} pts</span>
                                </div>
                                <div class="breakdown-bar-bg">
                                    <div class="breakdown-bar-fill fill-category" style="width: ${(m.category_score / 40) * 100}%"></div>
                                </div>
                            </div>

                            <div class="breakdown-item">
                                <div class="breakdown-item-header">
                                    <span class="breakdown-name">Location (${m.location_score}/30)</span>
                                    <span class="breakdown-score">+${m.location_score} pts</span>
                                </div>
                                <div class="breakdown-bar-bg">
                                    <div class="breakdown-bar-fill fill-location" style="width: ${(m.location_score / 30) * 100}%"></div>
                                </div>
                            </div>

                            <div class="breakdown-item">
                                <div class="breakdown-item-header">
                                    <span class="breakdown-name">Description (${m.description_score}/20)</span>
                                    <span class="breakdown-score">+${m.description_score} pts</span>
                                </div>
                                <div class="breakdown-bar-bg">
                                    <div class="breakdown-bar-fill fill-desc" style="width: ${(m.description_score / 20) * 100}%"></div>
                                </div>
                            </div>

                            <div class="breakdown-item">
                                <div class="breakdown-item-header">
                                    <span class="breakdown-name">Date Proximity (${m.date_score}/10)</span>
                                    <span class="breakdown-score">+${m.date_score} pts</span>
                                </div>
                                <div class="breakdown-bar-bg">
                                    <div class="breakdown-bar-fill fill-date" style="width: ${(m.date_score / 10) * 100}%"></div>
                                </div>
                            </div>
                        </div>

                        <!-- Side-by-Side Comparison -->
                        <div class="comparison-split-cols">
                            <div class="comparison-box box-lost">
                                <div class="comp-header">
                                    <span class="badge badge-lost">LOST REPORT #${lost.item_id}</span>
                                    <span class="comp-date">📅 ${escapeHTML(lost.report_date || 'Unknown')}</span>
                                </div>
                                <h4 class="comp-title">${escapeHTML(lost.item_name)}</h4>
                                <p class="comp-desc">${escapeHTML(lost.description || 'No description provided')}</p>
                                <div class="comp-meta">
                                    <div><strong>Category:</strong> ${escapeHTML(lost.category)}</div>
                                    <div><strong>Lost At:</strong> 📍 ${escapeHTML(lost.location)}</div>
                                    <div><strong>Reporter:</strong> 👤 ${escapeHTML(lost.reported_by || 'Anonymous')} ${lost.student_id ? `(${escapeHTML(lost.student_id)})` : ''}</div>
                                    <div><strong>Contact:</strong> ✉️ ${escapeHTML(lost.contact_info || 'N/A')}</div>
                                </div>
                            </div>

                            <div class="comparison-box box-found">
                                <div class="comp-header">
                                    <span class="badge badge-found">FOUND RECORD #${found.item_id}</span>
                                    <span class="comp-date">📅 ${escapeHTML(found.report_date || 'Unknown')}</span>
                                </div>
                                <h4 class="comp-title">${escapeHTML(found.item_name)}</h4>
                                <p class="comp-desc">${escapeHTML(found.description || 'No description provided')}</p>
                                <div class="comp-meta">
                                    <div><strong>Category:</strong> ${escapeHTML(found.category)}</div>
                                    <div><strong>Found At:</strong> 📍 ${escapeHTML(found.location)}</div>
                                    <div><strong>Safe Storage:</strong> 📦 ${escapeHTML(found.storage_location || 'Campus Front Desk')}</div>
                                    <div><strong>Finder:</strong> 👤 ${escapeHTML(found.reported_by || 'Good Samaritan')}</div>
                                </div>
                            </div>
                        </div>
                    </div>
                `;
            }).join('');
            if (typeof animateCardStagger === 'function') animateCardStagger(container);
        } catch (err) {
            console.error('Failed to load automated matches feed', err);
            if (container) {
                container.innerHTML = `
                    <div class="empty-state-card glass-panel">
                        <p>Unable to retrieve automated matches feed.</p>
                    </div>
                `;
            }
        }
    }
    window.loadAutomatedMatchesFeed = loadAutomatedMatchesFeed;

    function checkStudentMatchAlerts() {
        const banner = document.getElementById('studentMatchNotification');
        const titleEl = document.getElementById('studentMatchTitle');
        const textEl = document.getElementById('studentMatchText');
        const claimBtn = document.getElementById('btnStudentClaimMatch');
        if (!banner) return;

        const user = typeof AuthManager !== 'undefined' ? AuthManager.currentUser : null;
        if (!user || user.role === 'admin' || !AppState.matches || AppState.matches.length === 0) {
            banner.style.display = 'none';
            return;
        }

        // Find if any match belongs to this student and is unreturned/unclaimed
        const studentMatch = AppState.matches.find(m => {
            if (!m.lost_item || !m.found_item) return false;
            if (m.total_score < 70) return false;
            if (m.found_item.status === 'RETURNED' || m.found_item.status === 'CLAIMED') return false;

            const stuId = (m.lost_item.student_id || '').toLowerCase().trim();
            const repName = (m.lost_item.reported_by || '').toLowerCase().trim();
            const contact = (m.lost_item.contact_info || '').toLowerCase().trim();

            const userStuId = (user.student_id || '').toLowerCase().trim();
            const userName = (user.name || '').toLowerCase().trim();
            const userEmail = (user.email || '').toLowerCase().trim();

            return (userStuId && stuId === userStuId) ||
                   (userName && repName === userName) ||
                   (userEmail && contact === userEmail);
        });

        if (studentMatch) {
            banner.style.display = 'flex';
            if (titleEl) {
                titleEl.textContent = `🎯 Automated Match for "${studentMatch.lost_item.item_name}" (${studentMatch.total_score}% Score)!`;
            }
            if (textEl) {
                textEl.textContent = `Found item "${studentMatch.found_item.item_name}" at ${studentMatch.found_item.location} matches your lost report. Safe storage: ${studentMatch.found_item.storage_location || 'Campus Front Desk'}.`;
            }
            if (claimBtn) {
                claimBtn.onclick = () => {
                    openClaimModal(studentMatch.found_item.item_id);
                };
            }
        } else {
            banner.style.display = 'none';
        }
    }
    window.checkStudentMatchAlerts = checkStudentMatchAlerts;

    const btnRefreshAutoMatches = document.getElementById('btnRefreshAutoMatches');
    if (btnRefreshAutoMatches) {
        btnRefreshAutoMatches.addEventListener('click', async () => {
            btnRefreshAutoMatches.disabled = true;
            try {
                await loadAutomatedMatchesFeed();
                checkStudentMatchAlerts();
                showToast('Automated matches feed refreshed', 'info');
            } finally {
                btnRefreshAutoMatches.disabled = false;
            }
        });
    }

    // =========================================================================
    // CLAIMS QUEUE & ADMIN VERIFICATION WORKFLOW
    // =========================================================================
    const claimPills = document.querySelectorAll('.claim-filter-pills .claim-pill');
    let activeClaimFilter = 'ALL';

    claimPills.forEach(pill => {
        pill.addEventListener('click', () => {
            claimPills.forEach(p => p.classList.remove('active'));
            pill.classList.add('active');
            activeClaimFilter = pill.dataset.claimFilter;
            renderClaimsList(AppState.claims);
        });
    });

    function renderClaimsList(claims) {
        const container = document.getElementById('claimsListContainer');
        if (!container) return;

        let filtered = claims;
        if (activeClaimFilter !== 'ALL') {
            filtered = claims.filter(c => c.claim_status === activeClaimFilter);
        }

        if (filtered.length === 0) {
            container.innerHTML = `
                <div class="empty-state-card glass-panel">
                    <div class="empty-icon">📑</div>
                    <h3>No Claims in Queue</h3>
                    <p>There are no ownership claims matching the selected filter.</p>
                </div>
            `;
            return;
        }

        container.innerHTML = filtered.map(c => {
            const isPending = c.claim_status === 'PENDING';
            const isApproved = c.claim_status === 'APPROVED';
            const isRejected = c.claim_status === 'REJECTED';

            const item = c.found_item || (AppState.items && AppState.items.find(i => i.item_id === c.found_item_id));
            const isReturned = (item && item.status === 'RETURNED') || (c.found_item && c.found_item.status === 'RETURNED');
            const itemName = item ? item.item_name : (c.found_item ? c.found_item.item_name : `Found Item #${c.found_item_id}`);
            const currentUser = typeof AuthManager !== 'undefined' ? AuthManager.currentUser : null;
            const isOwnerOrAdmin = AppState.adminMode || (currentUser && (
                (c.claimant_id && currentUser.student_id && c.claimant_id.toLowerCase() === currentUser.student_id.toLowerCase()) ||
                (c.claimant_contact && currentUser.email && c.claimant_contact.toLowerCase() === currentUser.email.toLowerCase())
            ));

            let statusBadge = '';
            if (isReturned) {
                statusBadge = '<span class="badge badge-returned">HANDED OVER</span>';
            } else if (isApproved) {
                statusBadge = '<span class="badge badge-returned">APPROVED</span>';
            } else if (isPending) {
                statusBadge = '<span class="badge badge-claimed">PENDING REVIEW</span>';
            } else {
                statusBadge = '<span class="badge prio-very-important">REJECTED</span>';
            }

            let proofContent = '';
            if (isOwnerOrAdmin) {
                proofContent = `"${escapeHTML(c.verification_response)}"`;
            } else {
                proofContent = '<span style="font-style:italic; opacity:0.6;">🔒 Verification proof hidden for student privacy (Admin & Claimant access only)</span>';
            }

            let actionsHtml = '';
            if (AppState.adminMode) {
                if (isPending) {
                    actionsHtml = `
                    <div class="claim-admin-actions">
                        <button class="btn-sm btn-ghost" onclick="handleClaimReview(${c.claim_id}, 'REJECTED')">
                            Reject Claim
                        </button>
                        <button class="btn-sm btn-found" onclick="handleClaimReview(${c.claim_id}, 'APPROVED')">
                            Approve Claim
                        </button>
                    </div>`;
                } else if (isApproved && !isReturned) {
                    actionsHtml = `
                    <div class="claim-admin-actions">
                        <button class="btn-sm btn-found" onclick="openReturnModal(${c.found_item_id}, ${c.claim_id}, '${escapeHTML(c.claimant_id)}')">
                            🤝 Complete Handover
                        </button>
                    </div>`;
                } else if (isReturned) {
                    actionsHtml = `
                    <div class="claim-admin-actions" style="justify-content: flex-end;">
                        <span style="font-size:0.85rem; color:var(--emerald-success); display:flex; align-items:center; gap:6px;">
                            ✓ Handover Finalized & Logged
                        </span>
                    </div>`;
                }
            }

            return `
                <div class="claim-card glass-panel" data-claim-id="${c.claim_id}">
                    <div class="claim-top">
                        <div>
                            <span class="claim-id-badge">CLAIM REF #${c.claim_id}</span>
                            <h4 style="margin-top:4px; font-size:1.1rem">${escapeHTML(itemName)}</h4>
                        </div>
                        ${statusBadge}
                    </div>

                    <div class="claim-details-grid">
                        <div>
                            <p><strong>Claimant:</strong> ${escapeHTML(c.claimant_name)} (${escapeHTML(c.claimant_id)})</p>
                            <p><strong>Contact:</strong> ${escapeHTML(c.claimant_contact)}</p>
                            <p><strong>Submitted:</strong> ${escapeHTML(c.submitted_at || 'Recent')}</p>
                            ${c.reviewed_by ? `<p><strong>Reviewed by:</strong> ${escapeHTML(c.reviewed_by)}</p>` : ''}
                        </div>
                        <div>
                            <p><strong>Claimant Proof & Secret Details:</strong></p>
                            <div class="claim-verification-text">
                                ${proofContent}
                            </div>
                        </div>
                    </div>

                    ${actionsHtml}
                </div>
            `;
        }).join('');
        if (typeof animateCardStagger === 'function') animateCardStagger(container);
    }

    window.handleClaimReview = async function(claimId, status) {
        try {
            const notes = status === 'APPROVED' ? 'Proof matches physical item markings.' : 'Details did not match item properties.';
            const reviewerName = (typeof AuthManager !== 'undefined' && AuthManager.currentUser && AuthManager.currentUser.name)
                ? AuthManager.currentUser.name
                : 'Campus Administrator';
            await API.reviewClaim(claimId, status, notes, reviewerName);
            showToast(`Claim #${claimId} marked as ${status}`, 'success');
            await loadAllData();
        } catch (err) {
            showToast('Failed to update claim review', 'error');
        }
    };

    // =========================================================================
    // MODAL DIALOGS & HANDLERS
    // =========================================================================
    function isModalOpen() {
        return !!document.querySelector('.modal-backdrop.active');
    }

    function openModal(id) {
        const m = document.getElementById(id);
        if (m) m.classList.add('active');
    }
    window.openModal = openModal;

    function closeModal(id) {
        const m = document.getElementById(id);
        if (m) m.classList.remove('active');
    }
    window.closeModal = closeModal;

    document.querySelectorAll('[data-close]').forEach(btn => {
        btn.addEventListener('click', () => closeModal(btn.dataset.close));
    });

    document.querySelectorAll('.modal-backdrop').forEach(backdrop => {
        backdrop.addEventListener('click', (e) => {
            if (e.target === backdrop) backdrop.classList.remove('active');
        });
    });

    // Report Lost Button & Form
    document.getElementById('btnOpenLostModal').addEventListener('click', () => {
        if (typeof AuthManager !== 'undefined' && !AuthManager.currentUser) {
            showToast('Sign-in required: Please sign in to report a lost belonging', 'info');
            setTimeout(() => { window.location.href = 'login.html?role=student'; }, 400);
            return;
        }
        document.getElementById('lostDate').valueAsDate = new Date();
        if (typeof AuthManager !== 'undefined' && AuthManager.autofillForms) {
            AuthManager.autofillForms(true);
        }
        openModal('modalLost');
    });

    document.getElementById('formReportLost').addEventListener('submit', async (e) => {
        e.preventDefault();
        const data = {
            item_name: document.getElementById('lostName').value.trim(),
            category: document.getElementById('lostCategory').value,
            priority: document.getElementById('lostPriority').value,
            location: document.getElementById('lostLocation').value.trim(),
            report_date: document.getElementById('lostDate').value,
            description: document.getElementById('lostDescription').value.trim(),
            identifying_details: document.getElementById('lostIdentifying').value.trim(),
            reported_by: document.getElementById('lostReporter').value.trim(),
            student_id: document.getElementById('lostStudentId').value.trim(),
            contact_info: document.getElementById('lostContact').value.trim()
        };

        try {
            const res = await API.reportLostItem(data);
            showToast('Lost item published to campus registry!', 'success');
            closeModal('modalLost');
            document.getElementById('formReportLost').reset();
            await loadAllData();

            // Check if matches were found for the newly reported item
            if (res && res.item && res.item.item_id) {
                try {
                    const matches = await API.getItemMatches(res.item.item_id);
                    if (matches && matches.length > 0) {
                        setTimeout(() => {
                            showToast(`🎯 Found ${matches.length} potential matching item(s) in registry! Check the Matches tab.`, 'info');
                        }, 600);
                    }
                } catch (mErr) {}
            }
        } catch (err) {
            showToast(err.message || 'Failed to report lost item', 'error');
        }
    });

    // Report Found Button & Form
    document.getElementById('btnOpenFoundModal').addEventListener('click', () => {
        if (typeof AuthManager !== 'undefined' && !AuthManager.currentUser) {
            showToast('Sign-in required: Please sign in to register a found item', 'info');
            setTimeout(() => { window.location.href = 'login.html?role=student'; }, 400);
            return;
        }
        document.getElementById('foundDate').valueAsDate = new Date();
        if (typeof AuthManager !== 'undefined' && AuthManager.autofillForms) {
            AuthManager.autofillForms(true);
        }
        openModal('modalFound');
    });

    document.getElementById('formReportFound').addEventListener('submit', async (e) => {
        e.preventDefault();
        const currentUser = typeof AuthManager !== 'undefined' ? AuthManager.currentUser : null;
        const data = {
            item_name: document.getElementById('foundName').value.trim(),
            category: document.getElementById('foundCategory').value,
            priority: document.getElementById('foundPriority').value,
            location: document.getElementById('foundLocation').value.trim(),
            report_date: document.getElementById('foundDate').value,
            description: document.getElementById('foundDescription').value.trim(),
            storage_location: document.getElementById('foundStorage').value.trim(),
            identifying_details: document.getElementById('foundIdentifying').value.trim(),
            reported_by: document.getElementById('foundReporter').value.trim(),
            student_id: currentUser ? (currentUser.student_id || '') : '',
            contact_info: document.getElementById('foundContact').value.trim()
        };

        try {
            const res = await API.reportFoundItem(data);
            showToast('Found item registered in campus registry!', 'success');
            closeModal('modalFound');
            document.getElementById('formReportFound').reset();
            await loadAllData();

            // Check if matches were found for the newly reported found item
            if (res && res.item && res.item.item_id) {
                try {
                    const matches = await API.getItemMatches(res.item.item_id);
                    if (matches && matches.length > 0) {
                        setTimeout(() => {
                            showToast(`🎯 Found ${matches.length} matching lost report(s)! Check the Matches tab.`, 'info');
                        }, 600);
                    }
                } catch (mErr) {}
            }
        } catch (err) {
            showToast(err.message || 'Failed to report found item', 'error');
        }
    });

    // Claim Modal
    window.openClaimModal = async function(foundItemId) {
        if (typeof AuthManager !== 'undefined' && !AuthManager.currentUser) {
            showToast('Sign-in required: Please sign in to submit an ownership claim', 'info');
            setTimeout(() => { window.location.href = 'login.html?role=student'; }, 400);
            return;
        }

        let item = AppState.items ? AppState.items.find(i => i.item_id === foundItemId) : null;
        if (!item) {
            try {
                item = await API.getItem(foundItemId);
            } catch (e) {}
        }
        if (!item) {
            showToast('Found item record could not be loaded', 'error');
            return;
        }

        if (item.status === 'RETURNED') {
            showToast('This item has already been returned to its owner.', 'info');
            return;
        }

        document.getElementById('claimFoundItemId').value = foundItemId;
        const preview = document.getElementById('claimItemPreview');
        if (preview) {
            preview.innerHTML = `
                <h4>Claiming Item: ${escapeHTML(item.item_name)} (#${item.item_id})</h4>
                <p style="font-size:0.85rem; color:var(--text-muted)">Location: 📍 ${escapeHTML(item.location)} • Storage: 📦 ${escapeHTML(item.storage_location || 'Campus Front Desk')}</p>
            `;
        }

        if (typeof AuthManager !== 'undefined' && AuthManager.autofillForms) {
            AuthManager.autofillForms(true);
        }

        openModal('modalClaim');
    };

    document.getElementById('formSubmitClaim').addEventListener('submit', async (e) => {
        e.preventDefault();
        const verification = document.getElementById('claimVerification').value.trim();
        if (verification.length < 10) {
            showToast('Verification proof must be at least 10 characters long to prove ownership', 'error');
            return;
        }

        const data = {
            found_item_id: parseInt(document.getElementById('claimFoundItemId').value),
            claimant_name: document.getElementById('claimantName').value.trim(),
            claimant_id: document.getElementById('claimantId').value.trim(),
            claimant_contact: document.getElementById('claimantContact').value.trim(),
            verification_response: verification
        };

        try {
            await API.submitClaim(data);
            showToast('Ownership claim submitted for administrative review!', 'success');
            closeModal('modalClaim');
            document.getElementById('formSubmitClaim').reset();
            await loadAllData();
            switchTab('claims');
        } catch (err) {
            showToast(err.message || 'Failed to submit claim', 'error');
        }
    });

    // Details Inspector Modal
    async function openItemDetailsModal(itemId) {
        try {
            const item = await API.getItem(itemId);
            if (!item) return;

            document.getElementById('detailsModalIcon').textContent = item.item_type === 'LOST' ? '🚩' : '💎';
            document.getElementById('detailsModalTitle').textContent = item.item_name;
            document.getElementById('detailsModalSubtitle').textContent = `${item.item_type} Item • Reference #${item.item_id}`;

            const isLost = item.item_type === 'LOST';
            const body = document.getElementById('detailsModalBody');
            body.innerHTML = `
                <div class="card-meta-list" style="font-size: 0.95rem; gap: 10px; padding: 16px;">
                    <div class="meta-row"><span class="meta-k">Status:</span><span class="meta-v badge badge-${item.status.toLowerCase()}">${item.status}</span></div>
                    <div class="meta-row"><span class="meta-k">Priority:</span><span class="meta-v">${item.priority}</span></div>
                    <div class="meta-row"><span class="meta-k">Category:</span><span class="meta-v">${escapeHTML(item.category)}</span></div>
                    <div class="meta-row"><span class="meta-k">Location:</span><span class="meta-v">📍 ${escapeHTML(item.location)}</span></div>
                    <div class="meta-row"><span class="meta-k">Report Date:</span><span class="meta-v">📅 ${escapeHTML(item.report_date)}</span></div>
                    <div class="meta-row"><span class="meta-k">Reported By:</span><span class="meta-v">👤 ${escapeHTML(item.reported_by)}</span></div>
                    ${item.student_id ? `<div class="meta-row"><span class="meta-k">Student ID:</span><span class="meta-v">${escapeHTML(item.student_id)}</span></div>` : ''}
                    <div class="meta-row"><span class="meta-k">Contact:</span><span class="meta-v">✉️ ${escapeHTML(item.contact_info)}</span></div>
                    ${item.storage_location ? `<div class="meta-row"><span class="meta-k">Storage Box:</span><span class="meta-v">📦 ${escapeHTML(item.storage_location)}</span></div>` : ''}
                </div>

                <div style="margin-top: 16px;">
                    <h4>Public Description</h4>
                    <p style="color:var(--text-muted); margin-top: 6px;">${escapeHTML(item.description)}</p>
                </div>

                ${item.identifying_details ? `
                <div style="margin-top: 16px; background: rgba(0, 212, 255, 0.05); padding: 14px; border-radius: var(--radius-sm); border: 1px solid rgba(0, 212, 255, 0.2);">
                    <h4>Identifying Secret Marks</h4>
                    <p style="color: var(--cyan-primary); font-size: 0.88rem; margin-top: 4px;">${escapeHTML(item.identifying_details)}</p>
                </div>` : ''}

                <div style="margin-top: 20px; padding-top: 14px; border-top: 1px solid var(--border-subtle); display: flex; gap: 8px; justify-content: flex-end;">
                    ${AppState.adminMode && item.status !== 'RETURNED' ? `
                    <button class="btn btn-sm btn-outline" onclick="closeModal('modalDetails'); openReturnModal(${item.item_id});">
                        🤝 Handover Item
                    </button>
                    ` : ''}
                    ${(!isLost && item.status !== 'RETURNED' && item.status !== 'CLAIMED') ? `
                        ${(typeof AuthManager !== 'undefined' && AuthManager.currentUser) ? `
                        <button class="btn btn-sm btn-found" onclick="closeModal('modalDetails'); openClaimModal(${item.item_id});">
                            Claim This Belonging →
                        </button>
                        ` : `
                        <button class="btn btn-sm btn-primary" onclick="closeModal('modalDetails'); openClaimModal(${item.item_id});">
                            🔒 Sign In to Claim Belonging →
                        </button>
                        `}
                    ` : ''}
                </div>
            `;

            openModal('modalDetails');
        } catch (e) {
            showToast('Failed to load item details', 'error');
        }
    }

    window.openItemDetailsModal = openItemDetailsModal;

    // Return Modal
    window.openReturnModal = async function(itemId, claimId = null, claimantId = '') {
        let item = AppState.items ? AppState.items.find(i => i.item_id === itemId) : null;
        if (!item && AppState.claims) {
            const clm = AppState.claims.find(c => c.found_item_id === itemId || c.claim_id === claimId);
            if (clm && clm.found_item) item = clm.found_item;
        }
        if (!item) {
            try {
                item = await API.getItem(itemId);
            } catch (e) {}
        }
        if (!item) {
            showToast('Item record not found for handover', 'error');
            return;
        }

        if (item.status === 'RETURNED') {
            showToast('Item has already been marked as RETURNED', 'info');
            return;
        }

        document.getElementById('returnItemId').value = itemId;
        document.getElementById('returnClaimId').value = claimId || '';
        document.getElementById('returnClaimantId').value = claimantId || item.student_id || '';
        document.getElementById('returnItemSummary').textContent = `Finalizing handover for: ${item.item_name} (#${item.item_id})`;

        const notes = document.getElementById('returnNotes');
        if (notes && !notes.value) {
            notes.value = 'Identity verified in person at campus desk. Item safely returned to rightful owner.';
        }

        const verBy = document.getElementById('returnVerifiedBy');
        if (verBy) {
            verBy.value = (typeof AuthManager !== 'undefined' && AuthManager.currentUser && AuthManager.currentUser.name)
                ? AuthManager.currentUser.name
                : 'Campus Administrator';
        }

        openModal('modalReturn');
    };

    document.getElementById('formConfirmReturn').addEventListener('submit', async (e) => {
        e.preventDefault();
        const itemId = parseInt(document.getElementById('returnItemId').value);
        const claimId = parseInt(document.getElementById('returnClaimId').value) || 0;
        const data = {
            claim_id: claimId,
            claimant_id: document.getElementById('returnClaimantId').value.trim(),
            verified_by: document.getElementById('returnVerifiedBy').value.trim(),
            notes: document.getElementById('returnNotes').value.trim()
        };

        try {
            await API.confirmReturn(itemId, data);
            showToast('Item officially marked as RETURNED!', 'success');
            closeModal('modalReturn');
            await loadAllData();
        } catch (err) {
            showToast(err.message || 'Failed to complete return', 'error');
        }
    });

    // Re-scan Button on Hero
    const btnReScan = document.getElementById('btnReScan');
    if (btnReScan) {
        btnReScan.addEventListener('click', async () => {
            btnReScan.innerHTML = '↻ Scanning...';
            try {
                await API.refreshAllMatches();
                showToast('Item matching analysis updated across all campus nodes', 'success');
                await loadAllData();
            } finally {
                btnReScan.innerHTML = '↻ Rescan';
            }
        });
    }

    // =========================================================================
    // ADMIN CONTROL CENTER (CRUD Locations, Categories, Inventory Overrides, Users)
    // =========================================================================
    async function loadAdminPanelData() {
        if (!AppState.adminMode) return;
        await Promise.all([
            loadAdminLocations(),
            loadAdminCategories(),
            loadAdminItems(),
            loadAdminUsers()
        ]);
    }
    window.loadAdminPanelData = loadAdminPanelData;

    async function loadAdminLocations() {
        const tbody = document.getElementById('tbodyLocations');
        const countBadge = document.getElementById('adminLocationCount');
        if (!tbody) return;

        try {
            const locations = await API.getLocations();
            AppState.locations = locations;
            if (countBadge) countBadge.textContent = `${locations.length} Places`;

            if (locations.length === 0) {
                tbody.innerHTML = '<tr><td colspan="3" style="text-align:center; color:var(--text-muted); padding:16px;">No campus places defined yet.</td></tr>';
                return;
            }

            tbody.innerHTML = locations.map(loc => `
                <tr>
                    <td style="font-weight: 600; color: #fff;">${escapeHTML(loc.name)}</td>
                    <td style="color: var(--text-muted); font-size: 0.88rem;">${escapeHTML(loc.description || '—')}</td>
                    <td style="text-align: right;">
                        <button class="btn-sm btn-ghost btn-admin-del-loc" data-id="${loc.location_id}" style="color: var(--status-lost)" title="Delete place">
                            🗑️ Remove
                        </button>
                    </td>
                </tr>
            `).join('');

            tbody.querySelectorAll('.btn-admin-del-loc').forEach(btn => {
                btn.addEventListener('click', async () => {
                    const id = parseInt(btn.dataset.id);
                    if (confirm(`Remove this location from campus places?`)) {
                        try {
                            await API.deleteLocation(id);
                            showToast('Location removed from campus registry', 'success');
                            await loadCategoriesAndLocations();
                            await loadAdminLocations();
                        } catch (err) {
                            showToast(err.message || 'Failed to remove location', 'error');
                        }
                    }
                });
            });
        } catch (err) {
            console.error('Failed to load admin locations', err);
        }
    }

    async function loadAdminCategories() {
        const tbody = document.getElementById('tbodyCategories');
        const countBadge = document.getElementById('adminCategoryCount');
        if (!tbody) return;

        try {
            const categories = await API.getCategories();
            AppState.categories = categories;
            if (countBadge) countBadge.textContent = `${categories.length} Categories`;

            if (categories.length === 0) {
                tbody.innerHTML = '<tr><td colspan="3" style="text-align:center; color:var(--text-muted); padding:16px;">No categories defined yet.</td></tr>';
                return;
            }

            tbody.innerHTML = categories.map(cat => `
                <tr>
                    <td style="font-size: 1.2rem; text-align: center;">${escapeHTML(cat.icon || '📦')}</td>
                    <td style="font-weight: 600; color: #fff;">${escapeHTML(cat.name)}</td>
                    <td style="text-align: right;">
                        <button class="btn-sm btn-ghost btn-admin-del-cat" data-id="${cat.category_id}" style="color: var(--status-lost)" title="Delete category">
                            🗑️ Remove
                        </button>
                    </td>
                </tr>
            `).join('');

            tbody.querySelectorAll('.btn-admin-del-cat').forEach(btn => {
                btn.addEventListener('click', async () => {
                    const id = parseInt(btn.dataset.id);
                    if (confirm(`Remove this category?`)) {
                        try {
                            await API.deleteCategory(id);
                            showToast('Category removed from campus registry', 'success');
                            await loadCategoriesAndLocations();
                            await loadAdminCategories();
                        } catch (err) {
                            showToast(err.message || 'Failed to remove category', 'error');
                        }
                    }
                });
            });
        } catch (err) {
            console.error('Failed to load admin categories', err);
        }
    }

    async function loadAdminItems() {
        const tbody = document.getElementById('tbodyAdminItems');
        if (!tbody) return;
        renderAdminItemsTable(AppState.items || []);
    }

    function renderAdminItemsTable(items) {
        const tbody = document.getElementById('tbodyAdminItems');
        if (!tbody) return;

        const searchInput = document.getElementById('adminItemSearch');
        const q = (searchInput && searchInput.value) ? searchInput.value.toLowerCase().trim() : '';

        let filtered = items;
        if (q) {
            filtered = items.filter(it =>
                (it.item_name && it.item_name.toLowerCase().includes(q)) ||
                (it.location && it.location.toLowerCase().includes(q)) ||
                (it.reported_by && it.reported_by.toLowerCase().includes(q)) ||
                (it.student_id && it.student_id.toLowerCase().includes(q)) ||
                (it.category && it.category.toLowerCase().includes(q)) ||
                String(it.item_id).includes(q)
            );
        }

        if (filtered.length === 0) {
            tbody.innerHTML = '<tr><td colspan="8" style="text-align:center; color:var(--text-muted); padding:20px;">No items match search.</td></tr>';
            return;
        }

        tbody.innerHTML = filtered.map(it => {
            const isLost = it.item_type === 'LOST';
            const typeBadge = `<span class="badge ${isLost ? 'badge-lost' : 'badge-found'}">${it.item_type}</span>`;
            return `
                <tr>
                    <td style="color:var(--text-muted); font-size:0.85rem; font-family:var(--font-mono);">#${it.item_id}</td>
                    <td>${typeBadge}</td>
                    <td style="font-weight:600; color:#fff;">${escapeHTML(it.item_name)}</td>
                    <td>${escapeHTML(it.category)}</td>
                    <td>📍 ${escapeHTML(it.location)}</td>
                    <td>👤 ${escapeHTML(it.reported_by || 'Anonymous')}</td>
                    <td>
                        <select class="custom-select admin-status-select" data-id="${it.item_id}">
                            <option value="LOST" ${it.status === 'LOST' ? 'selected' : ''}>Lost</option>
                            <option value="FOUND" ${it.status === 'FOUND' ? 'selected' : ''}>Found</option>
                            <option value="MATCHED" ${it.status === 'MATCHED' ? 'selected' : ''}>Matched</option>
                            <option value="CLAIMED" ${it.status === 'CLAIMED' ? 'selected' : ''}>Claimed</option>
                            <option value="RETURNED" ${it.status === 'RETURNED' ? 'selected' : ''}>Returned</option>
                        </select>
                    </td>
                    <td style="text-align: right; white-space: nowrap;">
                        <button class="btn-sm btn-ghost btn-admin-view-item" data-id="${it.item_id}" title="View details">👁️</button>
                        <button class="btn-sm btn-ghost btn-admin-del-item" data-id="${it.item_id}" style="color:var(--status-lost)" title="Delete item">🗑️</button>
                    </td>
                </tr>
            `;
        }).join('');

        tbody.querySelectorAll('.admin-status-select').forEach(sel => {
            sel.addEventListener('change', async (e) => {
                const id = parseInt(sel.dataset.id);
                const newStatus = sel.value;
                try {
                    await API.updateItem(id, { status: newStatus });
                    showToast(`Item #${id} status updated to ${newStatus}`, 'success');
                    await loadAllData();
                } catch (err) {
                    showToast(err.message || 'Failed to update item status', 'error');
                }
            });
        });

        tbody.querySelectorAll('.btn-admin-view-item').forEach(btn => {
            btn.addEventListener('click', () => {
                openItemDetailsModal(parseInt(btn.dataset.id));
            });
        });

        tbody.querySelectorAll('.btn-admin-del-item').forEach(btn => {
            btn.addEventListener('click', async () => {
                const id = parseInt(btn.dataset.id);
                if (confirm(`Permanently remove item #${id} from inventory?`)) {
                    try {
                        await API.deleteItem(id);
                        showToast(`Item #${id} deleted`, 'success');
                        await loadAllData();
                    } catch (err) {
                        showToast(err.message || 'Failed to delete item', 'error');
                    }
                }
            });
        });
    }

    async function loadAdminUsers() {
        const tbody = document.getElementById('tbodyUsers');
        const countBadge = document.getElementById('adminUserCount');
        if (!tbody) return;

        try {
            const users = await API.getUsers();
            if (countBadge) countBadge.textContent = `${users.length} Users`;

            if (!users || users.length === 0) {
                tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:var(--text-muted); padding:16px;">No campus users found.</td></tr>';
                return;
            }

            const currentUserId = (typeof AuthManager !== 'undefined' && AuthManager.currentUser) ? AuthManager.currentUser.user_id : null;

            tbody.innerHTML = users.map(u => {
                const isAdmin = u.role === 'admin';
                const isSelf = currentUserId !== null && u.user_id === currentUserId;
                const roleBadge = `<span class="role-badge ${isAdmin ? 'role-admin' : 'role-student'}">${escapeHTML(u.role.toUpperCase())}</span>`;

                return `
                    <tr>
                        <td style="font-weight: 600; color: #fff;">
                            ${escapeHTML(u.name)}
                            ${isSelf ? '<span style="font-size: 0.72rem; color: var(--cyan-primary); margin-left: 6px;">(You)</span>' : ''}
                        </td>
                        <td style="font-family: var(--font-mono); font-size: 0.88rem; color: var(--text-secondary);">${escapeHTML(u.student_id)}</td>
                        <td style="color: var(--text-muted); font-size: 0.88rem;">${escapeHTML(u.email)}</td>
                        <td>${roleBadge}</td>
                        <td style="color: var(--text-muted); font-size: 0.85rem;">${escapeHTML(u.created_at || '—')}</td>
                        <td style="text-align: right;">
                            ${isSelf ? `
                                <button class="btn-sm btn-ghost" disabled style="opacity: 0.35; cursor: not-allowed;" title="Active session (cannot self-delete)">
                                    Active
                                </button>
                            ` : `
                                <button class="btn-sm btn-ghost btn-admin-del-user" data-id="${u.user_id}" data-name="${escapeHTML(u.name)}" style="color: var(--crimson-alert)" title="Remove user access">
                                    🗑️ Remove
                                </button>
                            `}
                        </td>
                    </tr>
                `;
            }).join('');

            tbody.querySelectorAll('.btn-admin-del-user').forEach(btn => {
                btn.addEventListener('click', async () => {
                    const id = parseInt(btn.dataset.id);
                    const name = btn.dataset.name || `User #${id}`;
                    if (confirm(`Are you sure you want to permanently remove access for ${name}?`)) {
                        try {
                            await API.deleteUser(id);
                            showToast(`User account "${name}" removed from campus registry`, 'success');
                            await loadAdminUsers();
                        } catch (err) {
                            showToast(err.message || 'Failed to remove user account', 'error');
                        }
                    }
                });
            });
        } catch (err) {
            console.error('Failed to load admin users', err);
            tbody.innerHTML = '<tr><td colspan="6" style="text-align:center; color:var(--crimson-alert); padding:16px;">Failed to load user directory.</td></tr>';
        }
    }

    // Forms and actions for Admin Desk
    const formAddLocation = document.getElementById('formAddLocation');
    if (formAddLocation) {
        formAddLocation.addEventListener('submit', async (e) => {
            e.preventDefault();
            const nameInput = document.getElementById('newLocationName');
            const descInput = document.getElementById('newLocationDesc');
            const name = nameInput.value.trim();
            const description = descInput ? descInput.value.trim() : '';
            if (!name) return;

            try {
                await API.createLocation({ name, description });
                showToast(`Location "${name}" added to campus places`, 'success');
                formAddLocation.reset();
                await loadCategoriesAndLocations();
                await loadAdminLocations();
            } catch (err) {
                showToast(err.message || 'Failed to add location', 'error');
            }
        });
    }

    const formAddCategory = document.getElementById('formAddCategory');
    if (formAddCategory) {
        formAddCategory.addEventListener('submit', async (e) => {
            e.preventDefault();
            const iconInput = document.getElementById('newCategoryIcon');
            const nameInput = document.getElementById('newCategoryName');
            const name = nameInput.value.trim();
            const icon = (iconInput && iconInput.value.trim()) ? iconInput.value.trim() : '📦';
            if (!name) return;

            try {
                await API.createCategory({ name, icon });
                showToast(`Category "${name}" added to campus registry`, 'success');
                formAddCategory.reset();
                if (iconInput) iconInput.value = '📦';
                await loadCategoriesAndLocations();
                await loadAdminCategories();
            } catch (err) {
                showToast(err.message || 'Failed to add category', 'error');
            }
        });
    }

    const formAddUser = document.getElementById('formAddUser');
    if (formAddUser) {
        formAddUser.addEventListener('submit', async (e) => {
            e.preventDefault();
            const nameInput = document.getElementById('newUserName');
            const idInput = document.getElementById('newUserId');
            const emailInput = document.getElementById('newUserEmail');
            const roleSelect = document.getElementById('newUserRole');

            const name = nameInput.value.trim();
            const student_id = idInput.value.trim();
            const email = emailInput.value.trim();
            const role = roleSelect ? roleSelect.value : 'student';
            const passwordInput = document.getElementById('newUserPassword');
            const password = passwordInput ? passwordInput.value.trim() : '';

            if (!name || !student_id || !email) return;

            try {
                await API.createUser({ name, student_id, email, role, password });
                showToast(`User "${name}" (${role}) registered successfully!`, 'success');
                formAddUser.reset();
                await loadAdminUsers();
            } catch (err) {
                showToast(err.message || 'Failed to register user', 'error');
            }
        });
    }

    const adminItemSearch = document.getElementById('adminItemSearch');
    if (adminItemSearch) {
        adminItemSearch.addEventListener('input', () => {
            renderAdminItemsTable(AppState.items || []);
        });
    }

    const btnAdminForceMatch = document.getElementById('btnAdminForceMatch');
    if (btnAdminForceMatch) {
        btnAdminForceMatch.addEventListener('click', async () => {
            btnAdminForceMatch.disabled = true;
            btnAdminForceMatch.innerHTML = '<span>⚡</span> Evaluating...';
            try {
                await API.refreshAllMatches();
                showToast('Autonomous matching engine re-evaluated across all pairs', 'success');
                await loadAllData();
            } catch (err) {
                showToast('Failed to evaluate matches', 'error');
            } finally {
                btnAdminForceMatch.disabled = false;
                btnAdminForceMatch.innerHTML = '⚡ Re-evaluate Match Engine';
            }
        });
    }

    // Helper: Escape HTML
    function escapeHTML(str) {
        if (!str) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    // Card Tilt Micro-interactions (Desktop hover-only to prevent phone touch lag)
    function initCardTilt() {
        if (!window.matchMedia('(hover: hover) and (pointer: fine)').matches) {
            return;
        }
        const cards = document.querySelectorAll('.stat-box, .item-card, .glass-panel');
        cards.forEach(card => {
            card.addEventListener('mousemove', (e) => {
                const rect = card.getBoundingClientRect();
                const x = e.clientX - rect.left;
                const y = e.clientY - rect.top;
                const centerX = rect.width / 2;
                const centerY = rect.height / 2;
                const rotateX = ((centerY - y) / centerY) * 3;
                const rotateY = ((x - centerX) / centerX) * 3;
                card.style.transform = `perspective(800px) rotateX(${rotateX}deg) rotateY(${rotateY}deg) translateY(-2px)`;
            });
            card.addEventListener('mouseleave', () => {
                card.style.transform = '';
            });
        });
    }

    // =========================================================================
    // NAVIGATION DROPDOWN (HAMBURGER MENU BESIDE USER PERSONA TAG)
    // =========================================================================
    const btnNavMenu = document.getElementById('btnNavMenu');
    const navMenuDropdown = document.getElementById('navMenuDropdown');

    if (btnNavMenu && navMenuDropdown) {
        btnNavMenu.addEventListener('click', (e) => {
            e.stopPropagation();
            const isOpen = navMenuDropdown.style.display === 'block';
            navMenuDropdown.style.display = isOpen ? 'none' : 'block';
            btnNavMenu.classList.toggle('active', !isOpen);
            btnNavMenu.setAttribute('aria-expanded', !isOpen);

            // Close auth dropdown if open
            const authDropdown = document.getElementById('authDropdown');
            if (authDropdown) authDropdown.style.display = 'none';
        });
    }

    // Close dropdowns on click outside
    window.addEventListener('click', (e) => {
        if (navMenuDropdown && !navMenuDropdown.contains(e.target) && e.target !== btnNavMenu && !btnNavMenu?.contains(e.target)) {
            navMenuDropdown.style.display = 'none';
            if (btnNavMenu) {
                btnNavMenu.classList.remove('active');
                btnNavMenu.setAttribute('aria-expanded', 'false');
            }
        }
    });

    // Close on Escape key
    window.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            if (navMenuDropdown) {
                navMenuDropdown.style.display = 'none';
                if (btnNavMenu) {
                    btnNavMenu.classList.remove('active');
                    btnNavMenu.setAttribute('aria-expanded', 'false');
                }
            }
            const authDropdown = document.getElementById('authDropdown');
            if (authDropdown) authDropdown.style.display = 'none';
        }
    });

    // Kick off Initial Load
    loadAllData().then(() => {
        setTimeout(initCardTilt, 400);
        const urlParams = new URLSearchParams(window.location.search);
        const tabParam = urlParams.get('tab') || (window.location.hash ? window.location.hash.substring(1) : null);
        if (tabParam && ['dashboard', 'items', 'matcher', 'claims', 'admin'].includes(tabParam)) {
            switchTab(tabParam);
        }
    });
});
