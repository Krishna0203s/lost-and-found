/**
 * Campus Lost & Found — Separated Authentication & Role Manager
 * Enforces strict separation between Student Portal and Administrator Access.
 */

const AuthManager = {
    currentUser: null,

    init() {
        const stored = localStorage.getItem('campus_lostfound_user');
        if (stored) {
            try {
                this.currentUser = JSON.parse(stored);
                if (this.currentUser && (
                    this.currentUser.name === 'Alex Rivera' ||
                    this.currentUser.name === 'Prof. Sarah Jenkins' ||
                    this.currentUser.student_id === 'ADM-2024-099' ||
                    this.currentUser.student_id === 'STU-2024-001'
                )) {
                    this.currentUser = null;
                    localStorage.removeItem('campus_lostfound_user');
                }
            } catch (e) {
                this.currentUser = null;
            }
        }

        this.applyAuthState();
        this.bindEvents();
    },

    async login(identifier, password = '', portalType = null) {
        try {
            const res = await API.login(identifier, password);
            if (res && res.success && res.user) {
                const user = res.user;

                // Enforce portal role restriction
                if (portalType === 'admin' && user.role !== 'admin') {
                    showToast('Access Denied: This account lacks administrative privileges', 'error');
                    return false;
                }

                this.currentUser = user;
                localStorage.setItem('campus_lostfound_user', JSON.stringify(this.currentUser));
                this.applyAuthState();

                const roleName = user.role === 'admin' ? 'Campus Administrator' : 'Student';
                showToast(`Signed into ${roleName} Portal: ${user.name}`, 'success');

                this.closeAllLoginModals();
                this.closeDropdown();

                // If logged in as admin, optionally switch to admin tab or refresh data
                if (user.role === 'admin' && window.switchTab) {
                    window.switchTab('admin');
                } else if (window.switchTab) {
                    window.switchTab('dashboard');
                }

                return true;
            }
        } catch (err) {
            showToast(err.message || 'Authentication failed: Verify credentials', 'error');
            return false;
        }
    },

    logout() {
        localStorage.removeItem('campus_lostfound_user');
        this.currentUser = null;
        this.closeDropdown();
        this.applyAuthState();

        // If on admin tab, redirect to dashboard
        if (window.AppState && window.AppState.activeTab === 'admin') {
            window.switchTab('dashboard');
        }

        showToast('Signed out from Campus Lost & Found', 'info');
        setTimeout(() => {
            window.location.href = 'login.html';
        }, 350);
    },

    applyAuthState() {
        const user = this.currentUser;
        const profilePill = document.getElementById('userProfilePill');
        const profileName = document.getElementById('userProfileName');
        const profileRole = document.getElementById('userProfileRole');
        const profileAvatar = document.getElementById('userProfileAvatar');
        const navTabAdmin = document.getElementById('navTabAdmin');

        const isAdmin = user && user.role === 'admin';

        if (profilePill) {
            if (user) {
                if (profileName) profileName.textContent = user.name;
                if (profileRole) {
                    profileRole.textContent = user.role.toUpperCase();
                    profileRole.className = `role-badge ${isAdmin ? 'role-admin' : 'role-student'}`;
                }
                if (profileAvatar) {
                    profileAvatar.textContent = isAdmin ? '🛡️' : '🎓';
                }
            } else {
                if (profileName) profileName.textContent = 'Sign In';
                if (profileRole) {
                    profileRole.textContent = 'GUEST';
                    profileRole.className = 'role-badge role-guest';
                }
                if (profileAvatar) profileAvatar.textContent = '👤';
            }
        }

        // Show/Hide Admin Control tab in navigation
        if (navTabAdmin) {
            navTabAdmin.style.display = isAdmin ? 'flex' : 'none';
        }
        const adminNavDivider = document.getElementById('adminNavDivider');
        if (adminNavDivider) {
            adminNavDivider.style.display = isAdmin ? 'block' : 'none';
        }

        // Update AppState
        if (window.AppState) {
            window.AppState.adminMode = isAdmin;
            if (typeof renderItems === 'function' && window.AppState.items) {
                renderItems(window.AppState.items);
            }
            if (window.AppState.activeTab === 'claims' && typeof loadClaims === 'function') {
                loadClaims();
            }
            if (window.AppState.activeTab === 'admin' && typeof loadAdminPanelData === 'function') {
                loadAdminPanelData();
            }
        }

        // Update student automated match alerts banner
        if (typeof checkStudentMatchAlerts === 'function') {
            checkStudentMatchAlerts();
        }

        // Toggle Browser-Only Mode banner
        const bannerBrowserOnly = document.getElementById('bannerBrowserOnly');
        if (bannerBrowserOnly) {
            bannerBrowserOnly.style.display = user ? 'none' : 'flex';
        }

        // Auto-fill forms
        this.autofillForms();
    },

    autofillForms(force = false) {
        if (!this.currentUser) return;
        const u = this.currentUser;

        // Lost form
        const lostRep = document.getElementById('lostReporter');
        const lostStu = document.getElementById('lostStudentId');
        const lostCon = document.getElementById('lostContact');
        if (lostRep && (!lostRep.value || lostRep.dataset.autofilled || force)) {
            lostRep.value = u.name;
            lostRep.dataset.autofilled = 'true';
        }
        if (lostStu && (!lostStu.value || lostStu.dataset.autofilled || force)) {
            lostStu.value = u.student_id;
            lostStu.dataset.autofilled = 'true';
        }
        if (lostCon && (!lostCon.value || lostCon.dataset.autofilled || force)) {
            lostCon.value = u.email;
            lostCon.dataset.autofilled = 'true';
        }

        // Found form
        const fndRep = document.getElementById('foundReporter');
        const fndCon = document.getElementById('foundContact');
        if (fndRep && (!fndRep.value || fndRep.dataset.autofilled || force)) {
            fndRep.value = u.name;
            fndRep.dataset.autofilled = 'true';
        }
        if (fndCon && (!fndCon.value || fndCon.dataset.autofilled || force)) {
            fndCon.value = u.email;
            fndCon.dataset.autofilled = 'true';
        }

        // Claim form
        const clmName = document.getElementById('claimantName');
        const clmId = document.getElementById('claimantId');
        const clmCon = document.getElementById('claimantContact');
        if (clmName && (!clmName.value || clmName.dataset.autofilled || force)) {
            clmName.value = u.name;
            clmName.dataset.autofilled = 'true';
        }
        if (clmId && (!clmId.value || clmId.dataset.autofilled || force)) {
            clmId.value = u.student_id;
            clmId.dataset.autofilled = 'true';
        }
        if (clmCon && (!clmCon.value || clmCon.dataset.autofilled || force)) {
            clmCon.value = u.email;
            clmCon.dataset.autofilled = 'true';
        }

        // Return form
        const retVer = document.getElementById('returnVerifiedBy');
        if (retVer && (!retVer.value || retVer.dataset.autofilled || force)) {
            retVer.value = u.name || 'Campus Administrator';
            retVer.dataset.autofilled = 'true';
        }
    },

    openStudentLogin() {
        window.location.href = 'login.html?role=student';
    },

    openAdminLogin() {
        window.location.href = 'login.html?role=admin';
    },

    closeAllLoginModals() {
        // Obsolete in-page modal handler kept as safe no-op
    },

    toggleDropdown() {
        const dd = document.getElementById('authDropdown');
        const themeDd = document.getElementById('themeDropdown');
        if (themeDd) themeDd.style.display = 'none';
        if (dd) {
            dd.style.display = dd.style.display === 'none' || dd.style.display === '' ? 'block' : 'none';
        }
    },

    closeDropdown() {
        const dd = document.getElementById('authDropdown');
        if (dd) dd.style.display = 'none';
    },

    bindEvents() {
        // Profile pill click toggles auth dropdown or navigates to login if guest
        const pill = document.getElementById('userProfilePill');
        if (pill) {
            pill.addEventListener('click', (e) => {
                e.stopPropagation();
                if (!this.currentUser) {
                    window.location.href = 'login.html';
                    return;
                }
                this.toggleDropdown();
            });
        }

        // Close dropdown when clicking outside
        document.addEventListener('click', (e) => {
            if (!e.target.closest('.nav-auth-buttons')) {
                this.closeDropdown();
            }
        });

        // Dropdown actions: Route to dedicated login page
        const btnSwitchStudent = document.getElementById('btnSwitchToStudent');
        if (btnSwitchStudent) {
            btnSwitchStudent.addEventListener('click', () => {
                this.closeDropdown();
                window.location.href = 'login.html?role=student';
            });
        }

        const btnSwitchAdmin = document.getElementById('btnSwitchToAdmin');
        if (btnSwitchAdmin) {
            btnSwitchAdmin.addEventListener('click', () => {
                this.closeDropdown();
                window.location.href = 'login.html?role=admin';
            });
        }

        const btnLogout = document.getElementById('btnLogout');
        if (btnLogout) {
            btnLogout.addEventListener('click', () => this.logout());
        }
    }
};

window.AuthManager = AuthManager;
