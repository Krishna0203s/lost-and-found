/**
 * Campus Lost & Found — Dynamic UI & Telemetry Animations
 * Features:
 * - Fluid cubic-bezier number count-up transitions with metric bump feedback
 * - SVG radial progress meter animations with synchronized percentage
 * - Modern notification toast system with SVG status icons, progress bar, & hover pause
 * - Cascading card entrance stagger helper
 * - Phone performance optimizations & prefers-reduced-motion compliance
 */

// Number Count-Up Transition Helper with formatting and bump feedback
function animateNumber(element, target, duration = 650) {
    if (!element) return;
    const end = parseInt(target) || 0;

    // Immediate update if reduced motion is requested
    if (window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
        element.textContent = end.toLocaleString();
        return;
    }

    const start = parseInt(element.textContent.replace(/[^0-9]/g, '')) || 0;
    if (start === end) {
        element.textContent = end.toLocaleString();
        return;
    }

    const startTime = performance.now();
    function update(time) {
        const elapsed = time - startTime;
        const progress = Math.min(elapsed / duration, 1);
        // Smooth ease-out cubic
        const ease = 1 - Math.pow(1 - progress, 3);
        const current = Math.round(start + (end - start) * ease);
        element.textContent = current.toLocaleString();

        if (progress < 1) {
            requestAnimationFrame(update);
        } else {
            element.textContent = end.toLocaleString();
            element.classList.remove('stat-pop');
            void element.offsetWidth; // Trigger reflow for re-triggering css animation
            element.classList.add('stat-pop');
            setTimeout(() => element.classList.remove('stat-pop'), 400);
        }
    }
    requestAnimationFrame(update);
}

// Radial Progress Dial Helper
function updateRecoveryDial(percentage) {
    const ring = document.getElementById('recoveryRing');
    const label = document.getElementById('statRecoveryRate');
    if (!ring || !label) return;

    const radius = ring.r.baseVal ? ring.r.baseVal.value : 40;
    const circumference = 2 * Math.PI * radius; // ~251.32
    ring.style.strokeDasharray = `${circumference} ${circumference}`;

    const safePct = Math.min(Math.max(percentage || 0, 0), 100);
    const offset = circumference - (safePct / 100) * circumference;

    ring.style.transition = 'stroke-dashoffset 0.85s cubic-bezier(0.16, 1, 0.3, 1)';
    ring.style.strokeDashoffset = offset;

    // Smooth label count-up
    animateNumber(label, safePct, 650);
    setTimeout(() => {
        if (label) label.textContent = `${Math.round(safePct)}%`;
    }, 680);
}

// Modern Toast Notification System
function showToast(message, type = 'info', duration = 3800) {
    let container = document.getElementById('toastContainer');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toastContainer';
        container.className = 'toast-container';
        document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;

    let iconSvg = '';
    if (type === 'success') {
        iconSvg = `<svg class="toast-svg text-emerald" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path><polyline points="22 4 12 14.01 9 11.01"></polyline></svg>`;
    } else if (type === 'error') {
        iconSvg = `<svg class="toast-svg text-crimson" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line></svg>`;
    } else if (type === 'warning') {
        iconSvg = `<svg class="toast-svg text-amber" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line></svg>`;
    } else {
        iconSvg = `<svg class="toast-svg text-cyan" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>`;
    }

    toast.innerHTML = `
        <div class="toast-body">
            <span class="toast-icon-wrap">${iconSvg}</span>
            <span class="toast-message">${message}</span>
            <button class="toast-close-btn" aria-label="Dismiss notification">&times;</button>
        </div>
        <div class="toast-progress-track">
            <div class="toast-progress-bar" style="animation-duration: ${duration}ms;"></div>
        </div>
    `;

    container.appendChild(toast);

    let isDismissed = false;
    let dismissTimer = null;

    function dismiss() {
        if (isDismissed) return;
        isDismissed = true;
        toast.classList.add('toast-exit');
        setTimeout(() => toast.remove(), 260);
    }

    dismissTimer = setTimeout(dismiss, duration);

    const closeBtn = toast.querySelector('.toast-close-btn');
    if (closeBtn) {
        closeBtn.addEventListener('click', () => {
            clearTimeout(dismissTimer);
            dismiss();
        });
    }

    // Pause dismissal and progress animation on hover (desktop only)
    if (window.matchMedia && window.matchMedia('(hover: hover)').matches) {
        toast.addEventListener('mouseenter', () => {
            clearTimeout(dismissTimer);
            const bar = toast.querySelector('.toast-progress-bar');
            if (bar) bar.style.animationPlayState = 'paused';
        });

        toast.addEventListener('mouseleave', () => {
            const bar = toast.querySelector('.toast-progress-bar');
            if (bar) bar.style.animationPlayState = 'running';
            dismissTimer = setTimeout(dismiss, 1400);
        });
    }
}

// Card Cascading Entrance Stagger Helper
function animateCardStagger(container) {
    if (!container) return;
    if (window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    const cards = container.querySelectorAll('.item-card, .auto-match-card, .claim-card, .stat-box');
    cards.forEach((card, index) => {
        card.style.animationDelay = `${Math.min(index * 25, 250)}ms`;
    });
}

window.animateNumber = animateNumber;
window.updateRecoveryDial = updateRecoveryDial;
window.showToast = showToast;
window.animateCardStagger = animateCardStagger;
