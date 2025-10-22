/* Header hide/show on scroll */
(function() {
    const header = document.getElementById('site-header');
    let prevY = window.scrollY;
    let ticking = false;
    const onScroll = () => {
        const y = window.scrollY;
        const goingDown = y > prevY && y > 24;
        header.classList.toggle('header--hidden', goingDown);
        prevY = y;
        ticking = false;
    };
    window.addEventListener('scroll', () => {
        if (!ticking) { requestAnimationFrame(onScroll); ticking = true; }
    }, { passive: true });
})();

/* Theme: dark by default, respect system preference, allow toggle */
(function() {
    const root = document.documentElement;
    const btn = document.getElementById('themeToggle');
    const storageKey = 'hatef-theme';

    function getSystemTheme() {
        return window.matchMedia && window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
    }

    function applyTheme(theme) {
        root.setAttribute('data-theme', theme);
        // Update button icon and labels
        if (btn) {
            const isDark = theme !== 'light';
            btn.textContent = isDark ? '🌙' : '☀️';
            btn.setAttribute('aria-label', isDark ? 'Switch to light mode' : 'Switch to dark mode');
            btn.setAttribute('aria-pressed', isDark ? 'true' : 'false');
        }
    }

    const saved = localStorage.getItem(storageKey);
    const initial = saved || getSystemTheme(); // default dark unless prefers light
    applyTheme(initial);

    btn?.addEventListener('click', () => {
        const current = root.getAttribute('data-theme') || initial;
        const next = current === 'light' ? 'dark' : 'light';
        applyTheme(next);
        localStorage.setItem(storageKey, next);
    });

    // React to system changes if user hasn't set a manual preference
    const mq = window.matchMedia('(prefers-color-scheme: light)');
    mq.addEventListener?.('change', (e) => {
        const manual = localStorage.getItem(storageKey);
        if (!manual) applyTheme(e.matches ? 'light' : 'dark');
    });
})();

/* Tier data: loaded from translations when present */
/* Utilities */
const money0 = new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: 0 });
function formatUSD(n) { return money0.format(n); }

/* Tiers are now rendered server-side in the template. We only wire up the reveal interaction. */
(function wireSeeAllReveal(){
    const grid = document.getElementById('tiersGrid');
    if (!grid) return;
    const btn = document.getElementById('seeAllTiers');
    if (!btn) return;
    const hiddenCards = Array.from(grid.querySelectorAll('.extra-tier.hide'));
    btn.addEventListener('click', (e) => {
        e.preventDefault();
        hiddenCards.forEach((c, idx) => {
            c.classList.remove('hide');
            c.classList.add('reveal');
            c.style.animationDelay = (idx * 60) + 'ms';
            c.addEventListener('animationend', () => c.classList.remove('reveal'), { once: true });
        });
        btn.remove();
    });
})();

/* Build IRR tier select: now server-rendered; no JS population needed */

function preselectTier(tierId) {
    const sel = document.getElementById('tierSelect');
    if (!sel) return;
    sel.value = tierId;
}

/* Payment modals */
const irrModal = document.getElementById('irr-modal');
const btcModal = document.getElementById('btc-modal');
let lastFocus = null;
function openModal(id) {
    const dlg = document.getElementById(id);
    if (!dlg) return;
    lastFocus = document.activeElement;
    if (typeof dlg.showModal === 'function') dlg.showModal(); else dlg.setAttribute('open', '');
    const focusable = dlg.querySelector('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
    (focusable || dlg).focus();
}
function closeModal(dlg) {
    if (!dlg) return;
    if (typeof dlg.close === 'function') dlg.close(); else dlg.removeAttribute('open');
    if (lastFocus) lastFocus.focus();
}
document.addEventListener('click', (e) => {
    const openId = e.target.closest?.('[data-open]')?.getAttribute('data-open');
    if (openId) {
        e.preventDefault();
        openModal(openId);
    }
    if (e.target.matches?.('[data-close]')) {
        const dlg = e.target.closest('.modal');
        closeModal(dlg);
    }
});
document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        if (irrModal?.open) closeModal(irrModal);
        if (btcModal?.open) closeModal(btcModal);
    }
});

/* Copy BTC address */
(function() {
    const btn = document.getElementById('copyBtc');
    const addr = document.getElementById('btcAddr');
    btn?.addEventListener('click', async () => {
        try {
            await navigator.clipboard.writeText(addr.textContent.trim());
            const grid = document.getElementById('tiersGrid');
            btn.textContent = (grid?.dataset?.copied) || 'Copied';
            setTimeout(() => (btn.textContent = (grid?.dataset?.copy || 'Copy')), 1200);
        } catch {
            const grid = document.getElementById('tiersGrid');
            btn.textContent = (grid?.dataset?.copyFailed) || 'Copy failed';
        }
    });
})();

/* Accordion */
(function() {
    const acc = document.getElementById('faqAcc');
    acc.querySelectorAll('.acc-item').forEach(item => {
        const btn = item.querySelector('.acc-btn');
        const panel = document.getElementById(btn.getAttribute('aria-controls'));
        btn.addEventListener('click', () => {
            const expanded = item.getAttribute('aria-expanded') === 'true';
            // Close any other open items (optional: keep multiple open? comment next 4 lines to allow multiple)
            // acc.querySelectorAll('.acc-item[aria-expanded="true"]').forEach(openItem => {
            //     if (openItem !== item) {
            //         openItem.setAttribute('aria-expanded', 'false');
            //         const openPanel = document.getElementById(openItem.querySelector('.acc-btn').getAttribute('aria-controls'));
            //         if (openPanel) openPanel.style.maxHeight = '0px';
            //     }
            // });

            item.setAttribute('aria-expanded', String(!expanded));
            btn.setAttribute('aria-expanded', String(!expanded));
            if (!expanded) {
                // opening
                panel.style.maxHeight = panel.scrollHeight + 'px';
            } else {
                // closing
                panel.style.maxHeight = '0px';
            }
        });
    });
})();

/* Footer year */
document.getElementById('year').textContent = new Date().getFullYear();

/* Call time preference toggle */
(function() {
    const specificTimeRadio = document.getElementById('specificTimeRadio');
    const specificTimeField = document.getElementById('specificTimeField');
    const callTimeRadios = document.querySelectorAll('input[name="call_time"]');
    
    if (!specificTimeRadio || !specificTimeField || !callTimeRadios.length) return;
    
    callTimeRadios.forEach(radio => {
        radio.addEventListener('change', () => {
            if (radio.value === 'specific' && radio.checked) {
                specificTimeField.classList.add('show');
                specificTimeField.querySelector('input').focus();
            } else if (radio.value === 'anytime' && radio.checked) {
                specificTimeField.classList.remove('show');
                // Clear the field after animation starts
                setTimeout(() => {
                    specificTimeField.querySelector('input').value = '';
                }, 150);
            }
        });
    });
})();

// TIER_DATA removed; tiers are rendered server-side

/* IRR Form submission */
(function() {
    const form = document.getElementById('irrForm');
    if (!form) return;
    
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const formData = new FormData(form);
        const data = Object.fromEntries(formData.entries());
        
        // Validate required fields
        if (!data.name || !data.email || !data.mobile || !data.tier || !data.amount) {
            showNotification('لطفاً تمامی فیلدهای ضروری را پر کنید.', 'error');
            return;
        }
        
        // Disable submit button
        const submitBtn = form.querySelector('button[type="submit"]');
        const originalText = submitBtn.textContent;
        submitBtn.disabled = true;
        submitBtn.textContent = 'در حال ارسال...';
        
        try {
            const response = await fetch('/api/v2/sponsor-submit', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(data)
            });
            
            const result = await response.json();
            
            if (response.ok && result.success) {
                // Show success message with bank info
                showBankInfo(result.bankInfo, result.note);
                form.reset();
                closeModal(document.getElementById('irr-modal'));
            } else {
                showNotification(result.message || 'خطا در ارسال فرم', 'error');
            }
            
        } catch (error) {
            console.error('Form submission error:', error);
            showNotification('خطا در ارتباط با سرور', 'error');
        } finally {
            // Re-enable submit button
            submitBtn.disabled = false;
            submitBtn.textContent = originalText;
        }
    });
})();



/* Notification system */
function showNotification(message, type = 'info') {
    // Remove existing notifications
    const existing = document.querySelector('.notification');
    if (existing) existing.remove();
    
    const notification = document.createElement('div');
    notification.className = `notification notification--${type}`;
    notification.innerHTML = `
        <div class="notification-content">
            <span>${message}</span>
            <button class="notification-close" onclick="this.parentElement.parentElement.remove()">×</button>
        </div>
    `;
    
    document.body.appendChild(notification);
    
    // Auto-remove after 5 seconds
    setTimeout(() => {
        if (notification.parentElement) {
            notification.remove();
        }
    }, 5000);
}

/* Bank info display */
function showBankInfo(bankInfo, note) {
    const modal = document.getElementById('bank-info-modal') || createBankInfoModal();
    
    // Update bank info content
    const content = modal.querySelector('.bank-info-content');
    content.innerHTML = `
        <h3>اطلاعات حساب بانکی</h3>
        <div class="bank-details">
            <div class="bank-field">
                <label>نام بانک:</label>
                <span>${bankInfo.bankName}</span>
            </div>
            <div class="bank-field">
                <label>شماره کارت:</label>
                <div class="field-with-copy">
                    <span class="copyable" data-copy-text="${bankInfo.cardNumber}">${bankInfo.cardNumber}</span>
                    <button class="copy-btn" data-copy-text="${bankInfo.cardNumber}" title="کپی شماره کارت">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
                            <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
                        </svg>
                    </button>
                </div>
            </div>
            <div class="bank-field">
                <label>شماره حساب:</label>
                <div class="field-with-copy">
                    <span class="copyable" data-copy-text="${bankInfo.accountNumber}">${bankInfo.accountNumber}</span>
                    <button class="copy-btn" data-copy-text="${bankInfo.accountNumber}" title="کپی شماره حساب">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
                            <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
                        </svg>
                    </button>
                </div>
            </div>
            <div class="bank-field">
                <label>شماره شبا:</label>
                <div class="field-with-copy">
                    <span class="copyable" data-copy-text="${bankInfo.iban}">${bankInfo.iban}</span>
                    <button class="copy-btn" data-copy-text="${bankInfo.iban}" title="کپی شماره شبا">
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
                            <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
                        </svg>
                    </button>
                </div>
            </div>
            <div class="bank-field">
                <label>نام صاحب حساب:</label>
                <span>${bankInfo.accountHolder}</span>
            </div>
            <!-- Temporarily hidden SWIFT code section
            <div class="bank-field">
                <label>کد SWIFT:</label>
                <span class="copyable" onclick="copyToClipboard('${bankInfo.swift}')">${bankInfo.swift}</span>
            </div>
            -->
        </div>
        <div class="bank-note">
            <p>${note}</p>
        </div>
    `;
    
    openModal('bank-info-modal');
    
    // Add event listeners for copy functionality
    const copyElements = modal.querySelectorAll('.copyable, .copy-btn');
    copyElements.forEach(element => {
        element.addEventListener('click', function() {
            const textToCopy = this.getAttribute('data-copy-text');
            if (textToCopy) {
                copyToClipboard(textToCopy);
            }
        });
    });
}

function createBankInfoModal() {
    const modal = document.createElement('dialog');
    modal.id = 'bank-info-modal';
    modal.className = 'modal';
    modal.setAttribute('aria-modal', 'true');
    modal.setAttribute('aria-labelledby', 'bank-info-title');
    
    modal.innerHTML = `
        <div class="modal-backdrop" data-close></div>
        <div class="modal-card" role="dialog">
            <div class="modal-header">
                <h3 id="bank-info-title" class="modal-title">اطلاعات پرداخت</h3>
                <button class="modal-close" type="button" data-close aria-label="Close">✕</button>
            </div>
            <div class="bank-info-content"></div>
            <div class="modal-actions">
                <button class="btn btn-primary" data-close>متوجه شدم</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    return modal;
}

function copyToClipboard(text) {
    navigator.clipboard.writeText(text).then(() => {
        showNotification('کپی شد!', 'success');
    }).catch(() => {
        showNotification('خطا در کپی کردن', 'error');
    });
}