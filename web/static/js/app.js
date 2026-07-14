// DOM Selection
const landingView = document.getElementById('landingView');
const dashboardView = document.getElementById('dashboardView');

const uploadArea = document.getElementById('uploadArea');
const fileInput = document.getElementById('fileInput');
const fileInfo = document.getElementById('fileInfo');
const fileName = document.getElementById('fileName');
const fileSize = document.getElementById('fileSize');
const uploadButton = document.getElementById('uploadButton');
const pauseButton = document.getElementById('pauseButton');
const cancelButton = document.getElementById('cancelButton');
const resultArea = document.getElementById('resultArea');
const downloadLink = document.getElementById('downloadLink');
const copyButton = document.getElementById('copyButton');
const errorMessage = document.getElementById('errorMessage');
const progressContainer = document.getElementById('progressContainer');
const progressBar = document.getElementById('progressBar');
const progressText = document.getElementById('progressText');
const landingThemeToggle = document.getElementById('landingThemeToggle');
const dashboardThemeToggle = document.getElementById('dashboardThemeToggle');

// Login Panel & Auth modal DOM
const loginUser = document.getElementById('loginUser');
const loginPassword = document.getElementById('loginPassword');
const confirmPassword = document.getElementById('confirmPassword');
const confirmPasswordField = document.getElementById('confirmPasswordField');
const loginButton = document.getElementById('loginButton');
const logoutButton = document.getElementById('logoutButton');
const loginModeButton = document.getElementById('loginModeButton');
const registerModeButton = document.getElementById('registerModeButton');
const authModalTitle = document.getElementById('authModalTitle');
const authModalSubtitle = document.getElementById('authModalSubtitle');
const loginStatus = document.getElementById('loginStatus');
const authErrorMessage = document.getElementById('authErrorMessage');
const authToggle = document.getElementById('authToggle');
const authOverlay = document.getElementById('authOverlay');
const authCloseBtn = document.getElementById('authCloseBtn');
const userProfileSummary = document.getElementById('userProfileSummary');

// Landing DOM
const landingLogo = document.getElementById('landingLogo');
const landingLoginBtn = document.getElementById('landingLoginBtn');
const landingCtaBtn = document.getElementById('landingCtaBtn');
const heroStartBtn = document.getElementById('heroStartBtn');

// Dashboard DOM
const sidebarLogo = document.getElementById('sidebarLogo');
const sidebarLogoutBtn = document.getElementById('sidebarLogoutBtn');
const userDisplayName = document.getElementById('userDisplayName');
const userDisplayRole = document.getElementById('userDisplayRole');
const fileCountBadge = document.getElementById('fileCountBadge');
const refreshFilesBtn = document.getElementById('refreshFilesBtn');
const filesTableBody = document.getElementById('filesTableBody');
const panelTitle = document.getElementById('panelTitle');
const shareOverlay = document.getElementById('shareOverlay');
const shareCloseBtn = document.getElementById('shareCloseBtn');
const shareFileName = document.getElementById('shareFileName');
const shareExpiry = document.getElementById('shareExpiry');
const createShareBtn = document.getElementById('createShareBtn');
const shareNewLink = document.getElementById('shareNewLink');
const shareLinkInput = document.getElementById('shareLinkInput');
const copyShareLinkBtn = document.getElementById('copyShareLinkBtn');
const shareList = document.getElementById('shareList');
const shareListStatus = document.getElementById('shareListStatus');

const THEME_STORAGE_KEY = 'fileshare.theme';
let currentUser = null;
let authMode = 'login';
let sharingFile = null;

let selectedFile = null;
let currentXhr = null;
let isCancelled = false;
let isPaused = false;

const MAX_UPLOAD_BYTES = 10 * 1024 * 1024 * 1024;

// SVG Icons
const sunIcon = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2"/><path d="M12 20v2"/><path d="m4.93 4.93 1.41 1.41"/><path d="m17.66 17.66 1.41 1.41"/><path d="M2 12h2"/><path d="M20 12h2"/><path d="m6.34 17.66-1.41 1.41"/><path d="m19.07 4.93-1.41 1.41"/></svg>`;
const moonIcon = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"/></svg>`;

let isDarkMode = false;
let isThemeTransitioning = false;

// Dynamic File Type Icons
function getFileIconSvg(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const iconStyle = `width="52" height="52" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"`;
    
    if (['jpg', 'jpeg', 'png', 'gif', 'svg', 'webp', 'bmp', 'ico'].includes(ext)) {
        return `<svg ${iconStyle}><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>`;
    }
    if (['mp4', 'mkv', 'avi', 'mov', 'webm', 'wmv'].includes(ext)) {
        return `<svg ${iconStyle}><polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2" ry="2"/></svg>`;
    }
    if (['mp3', 'wav', 'flac', 'ogg', 'm4a', 'aac'].includes(ext)) {
        return `<svg ${iconStyle}><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>`;
    }
    if (['html', 'css', 'js', 'ts', 'jsx', 'tsx', 'json', 'py', 'cpp', 'c', 'h', 'go', 'rs', 'java', 'sh', 'yaml', 'toml', 'sql'].includes(ext)) {
        return `<svg ${iconStyle}><polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/></svg>`;
    }
    if (['pdf', 'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'txt', 'md', 'epub'].includes(ext)) {
        return `<svg ${iconStyle}><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>`;
    }
    if (['zip', 'rar', '7z', 'tar', 'gz', 'bz2'].includes(ext)) {
        return `<svg ${iconStyle}><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><line x1="12" y1="3" x2="12" y2="21"/><line x1="8" y1="8" x2="16" y2="8"/><line x1="8" y1="12" x2="16" y2="12"/><line x1="8" y1="16" x2="16" y2="16"/></svg>`;
    }
    return `<svg ${iconStyle}><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/></svg>`;
}

function getDefaultUploadIconSvg() {
    return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M4 14.899A7 7 0 1 1 15.71 8h1.79a4.5 4.5 0 0 1 2.5 8.242"/><path d="M12 12v9"/><path d="m16 16-4-4-4 4"/></svg>`;
}

// Theme Operations
function loadThemePreference() {
    try {
        const v = localStorage.getItem(THEME_STORAGE_KEY);
        if (v === 'dark') return true;
        if (v === 'light') return false;
    } catch (_) { }
    return null;
}

function hasSavedThemePreference() {
    return loadThemePreference() !== null;
}

function saveThemePreference(isDark) {
    try {
        localStorage.setItem(THEME_STORAGE_KEY, isDark ? 'dark' : 'light');
    } catch (_) { }
}

function getSystemPrefersDark() {
    try {
        return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    } catch (_) {
        return false;
    }
}

function getThemeBackgroundGradient(theme) {
    if (theme === 'dark') {
        return 'linear-gradient(135deg, #080b10, #0f172a)';
    }
    return 'linear-gradient(135deg, #f8fafc, #e2e8f0)';
}

function setTheme(nextIsDark, persist = true) {
    isDarkMode = nextIsDark;
    if (isDarkMode) {
        document.documentElement.setAttribute('data-theme', 'dark');
        if (landingThemeToggle) landingThemeToggle.innerHTML = moonIcon;
        if (dashboardThemeToggle) dashboardThemeToggle.innerHTML = moonIcon;
    } else {
        document.documentElement.removeAttribute('data-theme');
        if (landingThemeToggle) landingThemeToggle.innerHTML = sunIcon;
        if (dashboardThemeToggle) dashboardThemeToggle.innerHTML = sunIcon;
    }

    if (persist) {
        saveThemePreference(isDarkMode);
    }
}

function toggleThemeWithDiagonalReveal() {
    if (isThemeTransitioning) return;
    isThemeTransitioning = true;

    const prevTheme = isDarkMode ? 'dark' : 'light';
    const nextIsDark = !isDarkMode;

    const revealOrigin = (!isDarkMode && nextIsDark) ? '0% 0%' : '100% 100%';

    setTheme(nextIsDark);

    const layer = document.createElement('div');
    layer.className = 'theme-transition-layer';
    layer.style.background = getThemeBackgroundGradient(prevTheme);
    layer.style.clipPath = `circle(150vmax at ${revealOrigin})`;
    document.body.appendChild(layer);

    requestAnimationFrame(() => {
        layer.style.clipPath = `circle(0vmax at ${revealOrigin})`;
    });

    const cleanup = () => {
        layer.removeEventListener('transitionend', cleanup);
        if (layer.parentNode) layer.parentNode.removeChild(layer);
        isThemeTransitioning = false;
    };
    layer.addEventListener('transitionend', cleanup);
    setTimeout(cleanup, 1100);
}

if (landingThemeToggle) {
    landingThemeToggle.addEventListener('click', toggleThemeWithDiagonalReveal);
}
if (dashboardThemeToggle) {
    dashboardThemeToggle.addEventListener('click', toggleThemeWithDiagonalReveal);
}

// Initial theme setup
const saved = loadThemePreference();
if (saved === null) {
    setTheme(getSystemPrefersDark(), false);
    try {
        const mq = window.matchMedia('(prefers-color-scheme: dark)');
        const onChange = (e) => {
            if (!hasSavedThemePreference()) {
                setTheme(!!e.matches, false);
            }
        };
        if (mq && mq.addEventListener) {
            mq.addEventListener('change', onChange);
        } else if (mq && mq.addListener) {
            mq.addListener(onChange);
        }
    } catch (_) { }
} else {
    setTheme(saved, false);
}

// Clipboard Helper
async function copyToClipboard(text) {
    try {
        if (navigator.clipboard && window.isSecureContext) {
            await navigator.clipboard.writeText(text);
            return true;
        }
    } catch (_) { }

    try {
        const ta = document.createElement('textarea');
        ta.value = text;
        ta.style.position = 'fixed';
        ta.style.left = '-9999px';
        document.body.appendChild(ta);
        ta.focus();
        ta.select();
        const ok = document.execCommand('copy');
        document.body.removeChild(ta);
        return ok;
    } catch (_) {
        return false;
    }
}

// Centralized Toast Notification System
function showToast(message, type = 'success') {
    let container = document.getElementById('toast-container');
    if (!container) {
        container = document.createElement('div');
        container.id = 'toast-container';
        document.body.appendChild(container);
    }
    
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    
    let iconSvg = '';
    if (type === 'success') {
        iconSvg = `<svg class="toast-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>`;
    } else if (type === 'error') {
        iconSvg = `<svg class="toast-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="8" x2="12" y2="12"></line><line x1="12" y1="16" x2="12.01" y2="16"></line></svg>`;
    } else {
        iconSvg = `<svg class="toast-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="12" x2="12" y2="16"></line><line x1="12" y1="8" x2="12.01" y2="8"></line></svg>`;
    }
    
    toast.innerHTML = `
        ${iconSvg}
        <span class="toast-msg">${escapeHtml(message)}</span>
    `;
    
    container.appendChild(toast);
    
    requestAnimationFrame(() => {
        toast.classList.add('show');
    });
    
    setTimeout(() => {
        toast.classList.remove('show');
        toast.classList.add('hide');
        toast.addEventListener('transitionend', () => {
            toast.remove();
            if (container.childNodes.length === 0) {
                container.remove();
            }
        });
    }, 3000);
}

// SPA Routing & Navigation
function navigateToDashboard() {
    landingView.hidden = true;
    dashboardView.classList.add('is-visible');
    
    // Default active panel
    switchPanel('upload');
    refreshFiles();
}

function navigateToLanding() {
    dashboardView.classList.remove('is-visible');
    landingView.hidden = false;
}

landingLogo.addEventListener('click', () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
});

sidebarLogo.addEventListener('click', () => {
    navigateToLanding();
});

landingCtaBtn.addEventListener('click', () => {
    navigateToDashboard();
});

heroStartBtn.addEventListener('click', () => {
    navigateToDashboard();
});

// Panel tabs toggler
const navItems = document.querySelectorAll('.nav-item');
const panels = document.querySelectorAll('.panel-container');

function switchPanel(panelId) {
    navItems.forEach(item => {
        item.classList.toggle('active', item.getAttribute('data-panel') === panelId);
    });
    
    panels.forEach(p => {
        p.classList.toggle('active', p.getAttribute('id') === `panel${panelId.charAt(0).toUpperCase() + panelId.slice(1)}`);
    });
    
    // Update panel title in navbar
    const titles = {
        'upload': '上传中心',
        'files': '我的文件',
        'api': 'API 配置'
    };
    panelTitle.textContent = titles[panelId] || '控制台';
    if (panelId === 'files') refreshFiles();
}

navItems.forEach(item => {
    item.addEventListener('click', () => {
        switchPanel(item.getAttribute('data-panel'));
    });
});

// Code Snippets Copy Buttons
document.querySelectorAll('.copy-code-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const code = btn.getAttribute('data-code');
        copyToClipboard(code).then(ok => {
            const orig = btn.textContent;
            btn.textContent = ok ? '已复制' : '失败';
            setTimeout(() => btn.textContent = orig, 1500);
            if (ok) showToast('API集成命令已复制到剪贴板', 'success');
        });
    });
});

// Upload Workspace UI updates
function resetUI() {
    resultArea.style.display = 'none';
    errorMessage.style.display = 'none';
    progressContainer.style.display = 'none';
    progressText.style.display = 'none';
    progressBar.style.width = '0%';
    progressText.textContent = '0%';

    uploadButton.style.display = 'block';
    uploadButton.textContent = '开始上传';

    pauseButton.style.display = 'none';
    cancelButton.style.display = 'none';

    updateAuthUI();
}

function resetUploadDownloadPageState() {
    if (currentXhr) {
        currentXhr.abort();
        currentXhr = null;
    }

    isCancelled = false;
    isPaused = false;
    selectedFile = null;
    fileInput.value = '';
    fileInfo.style.display = 'none';
    fileName.textContent = '';
    fileSize.textContent = '';

    downloadLink.href = '#';
    downloadLink.textContent = '';

    const uploadIconContainer = document.getElementById('uploadIconContainer');
    if (uploadIconContainer) {
        uploadIconContainer.innerHTML = getDefaultUploadIconSvg();
    }

    resetUI();
}

function showAuthError(msg) {
    const errorIcon = `<span class="error-icon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg></span>`;
    authErrorMessage.innerHTML = msg ? (errorIcon + `<span>${msg}</span>`) : '';
    authErrorMessage.style.display = msg ? 'flex' : 'none';
    if (msg) {
        authErrorMessage.classList.add('show');
    } else {
        authErrorMessage.classList.remove('show');
    }
}

function clearAuthError() {
    authErrorMessage.innerHTML = '';
    authErrorMessage.style.display = 'none';
    authErrorMessage.classList.remove('show');
}

function escapeHtml(value) {
    return value.replace(/[&<>"']/g, character => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    })[character]);
}

function renderFiles(files, emptyMessage) {
    if (!filesTableBody) return;
    if (fileCountBadge) fileCountBadge.textContent = files.length;

    if (files.length === 0) {
        filesTableBody.innerHTML = `<tr class="empty-row"><td colspan="2"><div class="empty-state">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
            <p>${emptyMessage}</p></div></td></tr>`;
        return;
    }

    filesTableBody.innerHTML = files.map(file => {
        const name = escapeHtml(file.name);
        const fileId = escapeHtml(file.file_id);
        return `<tr><td><div class="table-file-name-cell">${getFileIconSvg(file.name)}
            <span class="table-file-name-text" title="${name}">${name}</span></div></td>
            <td><div class="table-actions">
            <a href="/files/${fileId}/download" class="table-action-btn btn-download" title="下载">
                <svg class="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
                <span>下载</span>
            </a>
            <button class="table-action-btn share-file-btn btn-share" data-file-id="${fileId}" data-file-name="${name}" type="button" title="分享">
                <svg class="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-8"/><polyline points="16 6 12 2 8 6"/><line x1="12" y1="2" x2="12" y2="15"/></svg>
                <span>分享</span>
            </button>
            <button class="table-action-btn delete-file-btn btn-delete" data-file-id="${fileId}" type="button" title="删除">
                <svg class="btn-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/></svg>
                <span>删除</span>
            </button>
            </div></td></tr>`;
    }).join('');

    filesTableBody.querySelectorAll('.delete-file-btn').forEach(button => {
        button.addEventListener('click', () => deleteFile(button.dataset.fileId));
    });
    filesTableBody.querySelectorAll('.share-file-btn').forEach(button => {
        button.addEventListener('click', () => openShareModal(button.dataset.fileId, button.dataset.fileName));
    });
}

async function refreshFiles() {
    if (!currentUser) {
        renderFiles([], '登录后可查看自己的文件');
        return;
    }

    try {
        const response = await fetch('/files');
        if (response.status === 401) {
            currentUser = null;
            updateAuthUI();
            renderFiles([], '登录已过期，请重新登录');
            return;
        }
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const body = await response.json();
        renderFiles(Array.isArray(body.files) ? body.files : [], '暂无文件');
    } catch (_) {
        renderFiles([], '文件列表加载失败，请刷新重试');
    }
}

async function deleteFile(fileId) {
    if (!confirm('确定删除这个文件吗？此操作不可恢复。')) return;

    try {
        const response = await fetch(`/files/${fileId}`, {method: 'DELETE'});
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        await refreshFiles();
        showToast('文件已成功删除', 'success');
    } catch (_) {
        showToast('删除文件失败，请重试', 'error');
    }
}

if (refreshFilesBtn) {
    refreshFilesBtn.addEventListener('click', refreshFiles);
}

function updateAuthUI() {
    const isAuthenticated = currentUser !== null;
    const displayUsername = isAuthenticated ? currentUser.username : '访客用户';
    if (userDisplayName) userDisplayName.textContent = displayUsername;
    if (userDisplayRole) userDisplayRole.textContent = isAuthenticated ? 'Private Library' : 'Visitor';
    if (loginStatus) {
        const text = isAuthenticated ? `已登录: ${displayUsername}` : '访客未登录';
        loginStatus.innerHTML = `<span class="status-dot${isAuthenticated ? ' ok' : ''}"></span><span>${text}</span>`;
    }

    loginButton.disabled = isAuthenticated;
    uploadArea.classList.toggle('disabled', !isAuthenticated);
    uploadButton.disabled = !isAuthenticated;
    if (landingLoginBtn) landingLoginBtn.textContent = isAuthenticated ? `你好, ${displayUsername}` : '登录 / 注册';
}

function showAuthModal() {
    clearAuthError();
    authOverlay.classList.add('show');
    authOverlay.setAttribute('aria-hidden', 'false');
    setTimeout(updateAuthModeSlider, 50);
}

function setAuthMode(nextMode) {
    authMode = nextMode;
    const isRegistration = authMode === 'register';

    confirmPasswordField.hidden = !isRegistration;
    confirmPassword.value = '';
    loginPassword.autocomplete = isRegistration ? 'new-password' : 'current-password';
    loginButton.textContent = isRegistration ? '创建账户' : '登录';
    authModalTitle.textContent = isRegistration ? '创建 FileLink 账户' : '登录 FileLink';
    authModalSubtitle.textContent = isRegistration
        ? '创建后会自动登录，并拥有独立的私有文件库'
        : '登录后可管理自己的私有文件';

    loginModeButton.classList.toggle('active', !isRegistration);
    loginModeButton.setAttribute('aria-selected', String(!isRegistration));
    registerModeButton.classList.toggle('active', isRegistration);
    registerModeButton.setAttribute('aria-selected', String(isRegistration));
    clearAuthError();
    updateAuthModeSlider();
}

function hideAuthModal() {
    authOverlay.classList.remove('show');
    authOverlay.setAttribute('aria-hidden', 'true');
}

function showShareError(message) {
    shareList.innerHTML = `<p class="share-list-message">${escapeHtml(message)}</p>`;
}

function hideShareModal() {
    sharingFile = null;
    shareOverlay.classList.remove('show');
    shareOverlay.setAttribute('aria-hidden', 'true');
}

function formatShareExpiry(seconds) {
    return new Date(seconds * 1000).toLocaleString('zh-CN', {
        year: 'numeric', month: '2-digit', day: '2-digit',
        hour: '2-digit', minute: '2-digit'
    });
}

function renderShares(shares) {
    if (shares.length === 0) {
        shareList.innerHTML = '<p class="share-list-message">暂无有效分享链接</p>';
        return;
    }
    shareList.innerHTML = shares.map(share => `<div class="share-item">
        <div><div class="share-item-title">有效至 ${formatShareExpiry(share.expires_at)}</div>
        <div class="share-item-note">令牌仅在创建时展示，可随时撤销。</div></div>
        <button class="table-action-btn revoke-share-btn" data-share-id="${share.share_id}" type="button">撤销</button>
    </div>`).join('');
}

async function loadShares() {
    if (!sharingFile) return;
    shareListStatus.textContent = '加载中…';
    try {
        const response = await fetch(`/files/${sharingFile.id}/shares`);
        if (response.status === 401) throw new Error('登录已过期');
        if (!response.ok) throw new Error('无法读取分享链接');
        const body = await response.json();
        renderShares(Array.isArray(body.shares) ? body.shares : []);
        shareListStatus.textContent = '';
    } catch (error) {
        showShareError(error.message || '加载失败，请重试');
        shareListStatus.textContent = '';
    }
}

async function openShareModal(fileId, fileName) {
    sharingFile = {id: fileId, name: fileName};
    shareFileName.textContent = fileName;
    shareNewLink.hidden = true;
    shareLinkInput.value = '';
    shareOverlay.classList.add('show');
    shareOverlay.setAttribute('aria-hidden', 'false');
    
    // Set default segment select and position the slider
    const defaultSegment = document.querySelector('#shareExpirySegmented .segment-btn[data-value="604800"]');
    if (defaultSegment) {
        setTimeout(() => selectExpirySegment(defaultSegment), 50);
    }
    
    await loadShares();
}

async function createShare() {
    if (!sharingFile) return;
    createShareBtn.disabled = true;
    try {
        const response = await fetch(`/files/${sharingFile.id}/shares`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({expires_in_seconds: Number(shareExpiry.value)})
        });
        const body = await response.json().catch(() => ({}));
        if (!response.ok || !body.token) throw new Error(body.message || '创建失败，请重试');
        shareLinkInput.value = `${window.location.origin}/shares/${body.token}/download`;
        shareNewLink.hidden = false;
        await loadShares();
        showToast('公开分享链接已成功创建', 'success');
    } catch (error) {
        showShareError(error.message || '创建失败，请重试');
        showToast(error.message || '创建失败，请重试', 'error');
    } finally {
        createShareBtn.disabled = false;
    }
}

async function revokeShare(shareId) {
    if (!sharingFile || !confirm('撤销后该公开链接将立即失效，确定继续吗？')) return;
    try {
        const response = await fetch(`/files/${sharingFile.id}/shares/${shareId}`, {method: 'DELETE'});
        if (!response.ok) throw new Error('撤销失败，请重试');
        await loadShares();
        showToast('分享链接已成功撤销', 'success');
    } catch (error) {
        showShareError(error.message || '撤销失败，请重试');
        showToast(error.message || '撤销失败，请重试', 'error');
    }
}

landingLoginBtn.addEventListener('click', () => {
    if (currentUser) {
        navigateToDashboard();
    } else {
        showAuthModal();
    }
});

if (authToggle) {
    authToggle.addEventListener('click', () => {
        showAuthModal();
    });
}

if (userProfileSummary) {
    userProfileSummary.addEventListener('click', () => {
        if (!currentUser) {
            showAuthModal();
        }
    });
}

if (authCloseBtn) {
    authCloseBtn.addEventListener('click', () => {
        hideAuthModal();
    });
}

authOverlay.addEventListener('click', (e) => {
    if (e.target === authOverlay) hideAuthModal();
});

shareCloseBtn.addEventListener('click', hideShareModal);
shareOverlay.addEventListener('click', (e) => {
    if (e.target === shareOverlay) hideShareModal();
});
createShareBtn.addEventListener('click', createShare);
copyShareLinkBtn.addEventListener('click', async () => {
    const originalText = copyShareLinkBtn.textContent;
    const ok = await copyToClipboard(shareLinkInput.value);
    copyShareLinkBtn.textContent = ok ? '已复制' : '复制失败';
    setTimeout(() => { copyShareLinkBtn.textContent = originalText; }, 1500);
    if (ok) showToast('公开分享链接已复制到剪贴板', 'success');
});
shareList.addEventListener('click', (event) => {
    const button = event.target.closest('.revoke-share-btn');
    if (button) revokeShare(button.dataset.shareId);
});

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
        hideAuthModal();
        hideShareModal();
    }
});

// Dropzone interactions
uploadArea.addEventListener('click', () => {
    if (uploadArea.classList.contains('disabled')) {
        showToast('请先登录您的账户', 'error');
        showError('请先登录您的账户');
        return;
    }
    fileInput.click();
});

fileInput.addEventListener('click', (e) => {
    e.stopPropagation();
});

fileInput.addEventListener('change', (e) => {
    if (e.target.files.length > 0) handleFileSelect(e.target.files[0]);
});

uploadArea.addEventListener('dragover', (e) => {
    e.preventDefault();
    uploadArea.classList.add('dragover');
});

uploadArea.addEventListener('dragleave', () => uploadArea.classList.remove('dragover'));

uploadArea.addEventListener('drop', (e) => {
    e.preventDefault();
    uploadArea.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) handleFileSelect(e.dataTransfer.files[0]);
});

function handleFileSelect(file) {
    resetUI();
    if (file.size > MAX_UPLOAD_BYTES) {
        showError('文件大小不能超过 10GB');
        return;
    }

    selectedFile = file;
    fileName.textContent = file.name;
    fileSize.textContent = formatFileSize(file.size);
    fileInfo.style.display = 'block';
    uploadButton.style.display = 'block';

    const uploadIconContainer = document.getElementById('uploadIconContainer');
    if (uploadIconContainer) {
        uploadIconContainer.innerHTML = getFileIconSvg(file.name);
    }
}

uploadButton.addEventListener('click', () => {
    if (!selectedFile) return;
    if (!currentUser) {
        showError('请先登录您的账户');
        return;
    }

    resetUI();
    fileInfo.style.display = 'block';
    progressContainer.style.display = 'block';
    progressText.style.display = 'block';

    uploadButton.disabled = true;
    uploadButton.textContent = '正在上传...';
    uploadButton.style.display = 'none';
    pauseButton.style.display = 'block';
    cancelButton.style.display = 'block';

    isCancelled = false;
    isPaused = false;
    startTusUpload(selectedFile);
});

pauseButton.addEventListener('click', () => {
    isPaused = true;
    if (currentXhr) {
        currentXhr.abort();
        currentXhr = null;
    }
});

cancelButton.addEventListener('click', () => {
    isCancelled = true;
    if (currentXhr) {
        currentXhr.abort();
        currentXhr = null;
    }
    if (selectedFile) {
        const fingerprint = `tus:${selectedFile.name}-${selectedFile.size}-${selectedFile.lastModified}`;
        try {
            localStorage.removeItem(fingerprint);
        } catch (_) {}
    }
    resetUploadDownloadPageState();
});

function finishUpload() {
    currentXhr = null;
    const progress = parseFloat(progressBar.style.width || '0');
    if (progress > 0 && progress < 100) {
        uploadButton.textContent = '断点续传';
        cancelButton.style.display = 'block';
    } else {
        uploadButton.textContent = '开始上传';
        cancelButton.style.display = 'none';
    }
    uploadButton.disabled = false;
    uploadButton.style.display = 'block';
    pauseButton.style.display = 'none';
}

// Tus Protocol implementation
async function startTusUpload(file) {
    const fingerprint = `tus:${file.name}-${file.size}-${file.lastModified}`;
    let sessionUrl = null;
    
    try {
        sessionUrl = localStorage.getItem(fingerprint);
    } catch (_) {}

    let offset = 0;

    if (sessionUrl) {
        try {
            offset = await getTusSessionOffset(sessionUrl);
        } catch (e) {
            sessionUrl = null;
            try {
                localStorage.removeItem(fingerprint);
            } catch (_) {}
        }
    }

    if (!sessionUrl) {
        try {
            sessionUrl = await createTusSession(file);
            try {
                localStorage.setItem(fingerprint, sessionUrl);
            } catch (_) {}
            offset = 0;
        } catch (e) {
            showError('初始化上传会话失败: ' + e.message);
            finishUpload();
            return;
        }
    }

    const CHUNK_SIZE = 2 * 1024 * 1024; // 2MB chunk
    const totalSize = file.size;

    while (offset < totalSize && !isCancelled && !isPaused) {
        const chunkEnd = Math.min(offset + CHUNK_SIZE, totalSize);
        const chunk = file.slice(offset, chunkEnd);
        
        try {
            offset = await uploadTusChunk(sessionUrl, chunk, offset, totalSize);
            const percent = Math.round((offset / totalSize) * 100);
            progressBar.style.width = percent + '%';
            progressText.textContent = percent + '%';
        } catch (e) {
            if (isPaused) {
                finishUpload();
                isPaused = false;
                return;
            }
            if (isCancelled) {
                isCancelled = false;
                return;
            }
            showError('分片传输失败: ' + e.message);
            finishUpload();
            return;
        }
    }

    if (isPaused) {
        finishUpload();
        isPaused = false;
        return;
    }
    if (isCancelled) {
        isCancelled = false;
        return;
    }

    progressBar.style.width = '99%';
    progressText.textContent = '99% (正在进行哈希校验...)';
    
    try {
        const finalUrl = await pollTusStatus(sessionUrl);
        try {
            localStorage.removeItem(fingerprint);
        } catch (_) {}
        showSuccess(finalUrl);
        
        refreshFiles();
    } catch (e) {
        showError('归档终结失败: ' + e.message);
    } finally {
        finishUpload();
    }
}

function createTusSession(file) {
    return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/uploads', true);
        xhr.setRequestHeader('Tus-Resumable', '1.0.0');
        xhr.setRequestHeader('Upload-Length', file.size.toString());
        xhr.setRequestHeader('Upload-Metadata', `filename ${btoa(unescape(encodeURIComponent(file.name)))}`);
        xhr.onload = () => {
            if (xhr.status === 201) {
                const location = xhr.getResponseHeader('Location');
                if (location) {
                    resolve(location);
                } else {
                    reject(new Error('响应头中缺少 Location 指示'));
                }
            } else if (xhr.status === 401) {
                currentUser = null;
                updateAuthUI();
                reject(new Error('需要进行账户登录校验'));
            } else {
                reject(new Error(`服务器返回 ${xhr.status}: ${xhr.responseText}`));
            }
        };
        xhr.onerror = () => reject(new Error('网络请求异常'));
        xhr.send();
    });
}

function getTusSessionOffset(sessionUrl) {
    return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open('HEAD', sessionUrl, true);
        xhr.setRequestHeader('Tus-Resumable', '1.0.0');
        xhr.onload = () => {
            if (xhr.status === 200) {
                const offsetStr = xhr.getResponseHeader('Upload-Offset');
                if (offsetStr !== null) {
                    resolve(parseInt(offsetStr, 10));
                } else {
                    reject(new Error('响应头中缺少 Upload-Offset 偏置参数'));
                }
            } else {
                reject(new Error(`上传会话不存在或服务器返回 ${xhr.status}`));
            }
        };
        xhr.onerror = () => reject(new Error('网络请求异常'));
        xhr.send();
    });
}

function uploadTusChunk(sessionUrl, chunk, offset, totalSize) {
    return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        currentXhr = xhr;
        xhr.open('PATCH', sessionUrl, true);
        xhr.setRequestHeader('Tus-Resumable', '1.0.0');
        xhr.setRequestHeader('Upload-Offset', offset.toString());
        xhr.setRequestHeader('Content-Type', 'application/offset+octet-stream');
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const currentLoaded = offset + e.loaded;
                const percent = Math.round((currentLoaded / totalSize) * 100);
                progressBar.style.width = percent + '%';
                progressText.textContent = percent + '%';
            }
        };

        xhr.onload = () => {
            if (xhr.status === 204) {
                const newOffsetStr = xhr.getResponseHeader('Upload-Offset');
                if (newOffsetStr !== null) {
                    resolve(parseInt(newOffsetStr, 10));
                } else {
                    resolve(offset + chunk.size);
                }
            } else {
                reject(new Error(`服务器返回 ${xhr.status}: ${xhr.responseText}`));
            }
        };
        xhr.onerror = () => reject(new Error('网络请求异常'));
        xhr.send(chunk);
    });
}

function pollTusStatus(sessionUrl) {
    return new Promise((resolve, reject) => {
        let attempts = 0;
        const maxAttempts = 100;
        
        const check = () => {
            if (isCancelled || isPaused) {
                reject(new Error(isCancelled ? '上传已取消' : '上传已暂停'));
                return;
            }
            
            const xhr = new XMLHttpRequest();
            xhr.open('GET', sessionUrl, true);
            xhr.onload = () => {
                if (xhr.status === 200) {
                    try {
                        const res = JSON.parse(xhr.responseText);
                        if (res.state === 'COMPLETED') {
                            if (res.file_id) {
                                resolve(`/files/${res.file_id}/download`);
                            } else {
                                reject(new Error('分片上传会话已完成但文件 ID 缺失'));
                            }
                        } else if (res.state === 'FAILED') {
                            reject(new Error(res.failure_reason || '服务器端合并与哈希校验失败'));
                        } else {
                            attempts++;
                            if (attempts >= maxAttempts) {
                                reject(new Error('上传校验响应超时'));
                            } else {
                                setTimeout(check, 200);
                            }
                        }
                    } catch (e) {
                        reject(new Error('解析状态回执失败: ' + e.message));
                    }
                } else {
                    reject(new Error(`查询失败, 状态码 ${xhr.status}`));
                }
            };
            xhr.onerror = () => reject(new Error('网络请求异常'));
            xhr.send();
        };
        
        setTimeout(check, 100);
    });
}

function credentials() {
    const username = loginUser.value.trim();
    const password = loginPassword.value;
    if (!username || !password) {
        showAuthError('请输入用户名和密码');
        return null;
    }
    if (authMode === 'register') {
        if (!confirmPassword.value) {
            showAuthError('请再次输入密码进行确认');
            return null;
        }
        if (password !== confirmPassword.value) {
            showAuthError('两次输入的密码不一致');
            return null;
        }
    }
    return {username, password};
}

async function submitAuth() {
    const body = credentials();
    if (!body) return;

    const isRegistration = authMode === 'register';
    const path = isRegistration ? '/auth/register' : '/auth/login';
    const actionName = isRegistration ? '创建账户' : '登录';

    try {
        loginButton.disabled = true;
        const response = await fetch(path, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(body)
        });
        const result = await response.json().catch(() => ({}));
        if (!response.ok || !result.username) {
            throw new Error(result.message || `HTTP ${response.status}`);
        }
        currentUser = {username: result.username};
        updateAuthUI();
        clearAuthError();
        hideAuthModal();
        navigateToDashboard();
        showToast(isRegistration ? '账户注册成功' : '您已成功登录', 'success');
    } catch (error) {
        showAuthError(`${actionName}失败: ${error.message || String(error)}`);
        showToast(`${actionName}失败: ${error.message || String(error)}`, 'error');
    } finally {
        updateAuthUI();
    }
}

loginModeButton.addEventListener('click', () => setAuthMode('login'));
registerModeButton.addEventListener('click', () => setAuthMode('register'));
loginButton.addEventListener('click', submitAuth);

async function refreshCurrentUser() {
    try {
        const response = await fetch('/auth/me');
        if (!response.ok) return;
        const result = await response.json();
        if (result.username) currentUser = {username: result.username};
    } catch (_) {
        currentUser = null;
    } finally {
        updateAuthUI();
        refreshFiles();
    }
}

async function executeLogout() {
    try {
        const response = await fetch('/auth/logout', {method: 'POST'});
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        currentUser = null;
        hideShareModal();
        resetUploadDownloadPageState();
        hideAuthModal();
        navigateToLanding();
        refreshFiles();
        showToast('您已成功退出登录', 'info');
    } catch (_) {
        showAuthModal();
        showAuthError('退出登录失败，请重试');
        showToast('退出登录失败，请重试', 'error');
    }
}

logoutButton.addEventListener('click', hideAuthModal);
sidebarLogoutBtn.addEventListener('click', executeLogout);

copyButton.addEventListener('click', () => {
    const link = downloadLink.href;
    copyToClipboard(link).then((ok) => {
        const originalText = copyButton.textContent;
        copyButton.textContent = ok ? '已复制' : '复制失败';
        setTimeout(() => copyButton.textContent = originalText, 1500);
        if (ok) showToast('私有下载链接已复制到剪贴板', 'success');
    });
});

function showSuccess(url) {
    downloadLink.href = url;
    downloadLink.textContent = url;
    resultArea.style.display = 'block';
    uploadButton.style.display = 'none';
    cancelButton.style.display = 'none';
}

function showError(msg) {
    const errorIcon = `<span class="error-icon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg></span>`;
    errorMessage.innerHTML = msg ? (errorIcon + `<span>${msg}</span>`) : '';
    errorMessage.style.display = msg ? 'flex' : 'none';
    if (msg) {
        errorMessage.classList.add('show');
    } else {
        errorMessage.classList.remove('show');
    }
    progressContainer.style.display = 'none';
    progressText.style.display = 'none';
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

updateAuthUI();
refreshCurrentUser();

// Auth Mode Slider and Expiry Segment UI Helpers
function updateAuthModeSlider() {
    const authModeSlider = document.getElementById('authModeSlider');
    if (!authModeSlider) return;
    const isRegistration = authMode === 'register';
    const activeBtn = isRegistration ? registerModeButton : loginModeButton;
    if (activeBtn) {
        authModeSlider.style.width = `${activeBtn.offsetWidth}px`;
        authModeSlider.style.transform = `translateX(${activeBtn.offsetLeft - 4}px)`;
    }
}

function selectExpirySegment(btn) {
    const segments = document.querySelectorAll('#shareExpirySegmented .segment-btn');
    segments.forEach(b => b.classList.toggle('active', b === btn));
    
    // Sync to hidden select
    const val = btn.getAttribute('data-value');
    if (shareExpiry) {
        shareExpiry.value = val;
    }
    
    // Update slider position
    const slider = document.querySelector('#shareExpirySegmented .segment-slider');
    if (slider) {
        slider.style.width = `${btn.offsetWidth}px`;
        slider.style.transform = `translateX(${btn.offsetLeft - 4}px)`;
    }
}

function initExpirySegments() {
    const segments = document.querySelectorAll('#shareExpirySegmented .segment-btn');
    segments.forEach(btn => {
        btn.addEventListener('click', () => {
            selectExpirySegment(btn);
        });
    });
}

// Hero terminal typewriter animation
function initTerminalAnimation() {
    const terminalBody = document.querySelector('.hero-terminal .terminal-body');
    if (!terminalBody) return;
    
    const lines = [
        { type: 'cmd', text: 'curl -T my_large_dataset.zip http://filelink.dev/uploads' },
        { type: 'comment', text: '# 计算 Blake3 哈希进行秒传探测 (Checking hash)...' },
        { type: 'success', text: 'Instant Upload Success! File deduped on server.' },
        { type: 'cmd', text: 'echo "Private download:"' },
        { type: 'link', text: 'http://filelink.dev/files/2f7c9e/download' }
    ];
    
    let currentLineIndex = 0;
    let currentCharIndex = 0;
    
    function render() {
        terminalBody.innerHTML = '';
        for (let i = 0; i < currentLineIndex; i++) {
            appendLine(lines[i], lines[i].text);
        }
        if (currentLineIndex < lines.length) {
            const curLine = lines[currentLineIndex];
            if (curLine.type === 'cmd') {
                const textToShow = curLine.text.substring(0, currentCharIndex);
                appendLine(curLine, textToShow, true);
            } else {
                appendLine(curLine, curLine.text);
            }
        }
    }
    
    function appendLine(lineObj, text, showCursor = false) {
        const div = document.createElement('div');
        div.className = 'line';
        if (lineObj.type === 'cmd') {
            div.innerHTML = `<span class="cmd-prompt">$</span> <span class="typed-text"></span>${showCursor ? '<span class="terminal-cursor"></span>' : ''}`;
            div.querySelector('.typed-text').textContent = text;
        } else if (lineObj.type === 'comment') {
            div.className = 'line-comment';
            div.textContent = text;
        } else if (lineObj.type === 'success') {
            div.className = 'line-success';
            div.textContent = text;
        } else if (lineObj.type === 'link') {
            div.className = 'line-link';
            div.textContent = text;
        }
        terminalBody.appendChild(div);
    }
    
    function step() {
        if (currentLineIndex >= lines.length) {
            setTimeout(() => {
                currentLineIndex = 0;
                currentCharIndex = 0;
                step();
            }, 6000);
            return;
        }
        
        const curLine = lines[currentLineIndex];
        if (curLine.type === 'cmd') {
            if (currentCharIndex < curLine.text.length) {
                currentCharIndex++;
                render();
                const delay = Math.random() * 50 + 30; // 30-80ms
                setTimeout(step, delay);
            } else {
                render();
                currentCharIndex = 0;
                currentLineIndex++;
                setTimeout(step, 600);
            }
        } else {
            setTimeout(() => {
                currentLineIndex++;
                render();
                step();
            }, curLine.type === 'comment' ? 1200 : 400);
        }
    }
    
    render();
    setTimeout(step, 1000);
}

// Window resize listener to sync active sliders
window.addEventListener('resize', () => {
    updateAuthModeSlider();
    const activeExpiry = document.querySelector('#shareExpirySegmented .segment-btn.active');
    if (activeExpiry) {
        selectExpirySegment(activeExpiry);
    }
});

// Initialize on execution
initTerminalAnimation();
initExpirySegments();
