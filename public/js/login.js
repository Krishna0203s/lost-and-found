/**
 * Campus Lost & Found — Dedicated Login Gateway
 * Synchronized with C Backend API and Application State.
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Theme Management (Synced with Main Application)
    const STORAGE_THEME = 'campus_lostfound_theme';
    const STORAGE_USER = 'campus_lostfound_user';

    const savedTheme = localStorage.getItem(STORAGE_THEME) || 'dark';
    applyTheme(savedTheme);

    const btnThemeToggle = document.getElementById('btnThemeToggle');
    const themeDropdown = document.getElementById('themeDropdown');

    function applyTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        if (theme === 'light') {
            document.body.classList.add('light-theme');
        } else {
            document.body.classList.remove('light-theme');
        }
        localStorage.setItem(STORAGE_THEME, theme);
        updateActiveOption(theme);
    }

    function updateActiveOption(theme) {
        document.querySelectorAll('.theme-dropdown-option').forEach(opt => {
            const isSelected = opt.dataset.theme === theme;
            opt.classList.toggle('active', isSelected);
            opt.setAttribute('aria-selected', isSelected ? 'true' : 'false');
        });
    }

    if (btnThemeToggle && themeDropdown) {
        btnThemeToggle.addEventListener('click', (e) => {
            e.stopPropagation();
            const isHidden = themeDropdown.style.display === 'none' || !themeDropdown.style.display;
            if (isHidden) {
                themeDropdown.style.display = 'block';
                btnThemeToggle.setAttribute('aria-expanded', 'true');
                const current = document.documentElement.getAttribute('data-theme') || 'dark';
                updateActiveOption(current);
            } else {
                themeDropdown.style.display = 'none';
                btnThemeToggle.setAttribute('aria-expanded', 'false');
            }
        });

        themeDropdown.querySelectorAll('.theme-dropdown-option').forEach(opt => {
            opt.addEventListener('click', (e) => {
                e.stopPropagation();
                const chosen = opt.dataset.theme;
                applyTheme(chosen);
                themeDropdown.style.display = 'none';
                btnThemeToggle.setAttribute('aria-expanded', 'false');
                showToast(chosen === 'light' ? 'Light mode activated ☀️' : 'Dark mode activated 🌙', 'info');
            });
        });

        document.addEventListener('click', (e) => {
            if (!themeDropdown.contains(e.target) && e.target !== btnThemeToggle && !btnThemeToggle.contains(e.target)) {
                themeDropdown.style.display = 'none';
                btnThemeToggle.setAttribute('aria-expanded', 'false');
            }
        });

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && themeDropdown.style.display === 'block') {
                themeDropdown.style.display = 'none';
                btnThemeToggle.setAttribute('aria-expanded', 'false');
                btnThemeToggle.focus();
            }
        });
    }

    // 2. Check Active Session
    let activeUser = null;
    try {
        const stored = localStorage.getItem(STORAGE_USER);
        if (stored) {
            activeUser = JSON.parse(stored);
            // Purge outdated demo accounts if any
            if (activeUser && (
                activeUser.name === 'Alex Rivera' ||
                activeUser.name === 'Prof. Sarah Jenkins' ||
                activeUser.student_id === 'ADM-2024-099' ||
                activeUser.student_id === 'STU-2024-001'
            )) {
                activeUser = null;
                localStorage.removeItem(STORAGE_USER);
            }
        }
    } catch (e) {
        activeUser = null;
    }

    const sessionBanner = document.getElementById('activeSessionBanner');
    const sessionDetails = document.getElementById('activeSessionDetails');
    const btnContinueDashboard = document.getElementById('btnContinueDashboard');
    const btnSignOutCurrent = document.getElementById('btnSignOutCurrent');

    if (activeUser && sessionBanner) {
        sessionBanner.style.display = 'block';
        if (sessionDetails) {
            const isAdmin = activeUser.role === 'admin';
            sessionDetails.innerHTML = `
                Currently signed in as <strong>${escapeHTML(activeUser.name)}</strong>
                <span class="role-badge ${isAdmin ? 'role-admin' : 'role-student'}">${escapeHTML(activeUser.role.toUpperCase())}</span>
            `;
        }
        if (btnContinueDashboard) {
            btnContinueDashboard.addEventListener('click', () => {
                window.location.href = 'index.html';
            });
        }
        if (btnSignOutCurrent) {
            btnSignOutCurrent.addEventListener('click', () => {
                localStorage.removeItem(STORAGE_USER);
                activeUser = null;
                sessionBanner.style.display = 'none';
                showToast('Signed out successfully. You can sign in below.', 'info');
            });
        }
    }

    // 3. Role Tab Switching (Student vs. Administrator)
    const tabStudent = document.getElementById('tabStudent');
    const tabAdmin = document.getElementById('tabAdmin');
    const panelStudent = document.getElementById('panelStudent');
    const panelAdmin = document.getElementById('panelAdmin');
    const cardIcon = document.getElementById('loginCardIcon');
    const cardSubtitle = document.getElementById('loginCardSubtitle');

    function selectRole(role) {
        if (role === 'admin') {
            tabAdmin.classList.add('active', 'tab-admin');
            tabStudent.classList.remove('active');
            panelAdmin.style.display = 'block';
            panelStudent.style.display = 'none';
            if (cardIcon) cardIcon.textContent = '🛡️';
            if (cardSubtitle) cardSubtitle.textContent = 'Authorized campus staff & security desk controls';
            const adminInput = document.getElementById('adminUsername');
            if (adminInput) adminInput.focus();
        } else {
            tabStudent.classList.add('active');
            tabAdmin.classList.remove('active', 'tab-admin');
            panelStudent.style.display = 'block';
            panelAdmin.style.display = 'none';
            if (cardIcon) cardIcon.textContent = '🎓';
            if (cardSubtitle) cardSubtitle.textContent = 'Report belongings, track claims & view automated matches';
            const studentInput = document.getElementById('studentIdentifier');
            if (studentInput) studentInput.focus();
        }
    }

    if (tabStudent) tabStudent.addEventListener('click', () => selectRole('student'));
    if (tabAdmin) tabAdmin.addEventListener('click', () => selectRole('admin'));

    // Check URL parameters for initial role (?role=admin or ?role=student)
    const urlParams = new URLSearchParams(window.location.search);
    const initialRole = urlParams.get('role');
    if (initialRole === 'admin') {
        selectRole('admin');
    } else {
        selectRole('student');
    }

    // 4. Student Login Submission
    const formStudent = document.getElementById('formStudent');
    if (formStudent) {
        formStudent.addEventListener('submit', async (e) => {
            e.preventDefault();
            const identifier = document.getElementById('studentIdentifier').value.trim();
            const submitBtn = formStudent.querySelector('button[type="submit"]');

            if (!identifier) {
                showToast('Please enter your Student ID or campus email', 'error');
                return;
            }

            if (submitBtn) {
                submitBtn.disabled = true;
                submitBtn.innerHTML = '<span>Verifying...</span>';
            }

            try {
                let res = null;
                try {
                    res = await API.login(identifier, '');
                } catch (loginErr) {
                    if (loginErr && loginErr.message && (
                        loginErr.message.includes('Incorrect password') ||
                        loginErr.message.includes('Access Denied')
                    )) {
                        throw loginErr;
                    }
                    // Auto-register student if account not created yet
                    const isEmail = identifier.includes('@');
                    const stuId = isEmail ? identifier.split('@')[0].toUpperCase() : identifier;
                    const email = isEmail ? identifier : `${identifier.toLowerCase()}@campus.edu`;
                    const name = isEmail ? identifier.split('@')[0] : `Student ${identifier}`;
                    await API.createUser({
                        name: name,
                        student_id: stuId,
                        email: email,
                        role: 'student',
                        password: ''
                    });
                    res = await API.login(identifier, '');
                }

                if (res && res.success && res.user) {
                    localStorage.setItem(STORAGE_USER, JSON.stringify(res.user));
                    showToast(`Welcome, ${res.user.name}! Redirecting...`, 'success');
                    setTimeout(() => {
                        window.location.href = 'index.html';
                    }, 500);
                } else {
                    throw new Error(res?.error || 'Authentication failed');
                }
            } catch (err) {
                if (submitBtn) {
                    submitBtn.disabled = false;
                    submitBtn.innerHTML = '<span>Sign In to Student Portal →</span>';
                }
                showToast(err.message || 'Authentication failed. Please verify your Student ID.', 'error');
            }
        });
    }

    // 5. Administrator Login Submission
    const formAdmin = document.getElementById('formAdmin');
    if (formAdmin) {
        formAdmin.addEventListener('submit', async (e) => {
            e.preventDefault();
            const username = document.getElementById('adminUsername').value.trim();
            const password = document.getElementById('adminPassword').value;
            const submitBtn = formAdmin.querySelector('button[type="submit"]');

            if (!username) {
                showToast('Please enter administrator username or ID', 'error');
                return;
            }

            if (!password) {
                showToast('Please enter administrator password', 'error');
                return;
            }

            if (submitBtn) {
                submitBtn.disabled = true;
                submitBtn.innerHTML = '<span>Authenticating...</span>';
            }

            try {
                const res = await API.login(username, password);
                if (res && res.success && res.user) {
                    if (res.user.role !== 'admin') {
                        throw new Error('Access Denied: This account lacks administrator privileges');
                    }

                    localStorage.setItem(STORAGE_USER, JSON.stringify(res.user));
                    showToast(`Administrator authenticated: ${res.user.name}. Opening dashboard...`, 'success');
                    setTimeout(() => {
                        window.location.href = 'index.html';
                    }, 500);
                } else {
                    throw new Error(res?.error || 'Authentication failed');
                }
            } catch (err) {
                if (submitBtn) {
                    submitBtn.disabled = false;
                    submitBtn.innerHTML = '<span>Authenticate Administrator →</span>';
                }
                showToast(err.message || 'Administrator authentication failed. Check credentials.', 'error');
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
});
