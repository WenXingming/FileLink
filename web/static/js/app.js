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
const shareLink = document.getElementById('shareLink');
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
const loginButton = document.getElementById('loginButton');
const registerButton = document.getElementById('registerButton');
const logoutButton = document.getElementById('logoutButton');
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

const THEME_STORAGE_KEY = 'fileshare.theme';
let currentUser = null;

let selectedFile = null;
let currentXhr = null;
let isCancelled = false;
let isPaused = false;

const MAX_UPLOAD_BYTES = 5 * 1024 * 1024 * 1024;

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

// SPA Routing & Navigation
function navigateToDashboard() {
    landingView.style.display = 'none';
    dashboardView.style.display = 'flex';
    
    // Default active panel
    switchPanel('upload');
    refreshFiles();
}

function navigateToLanding() {
    dashboardView.style.display = 'none';
    landingView.style.display = 'flex';
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

    shareLink.href = '#';
    shareLink.textContent = '';

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
            <td><div class="table-actions"><a href="/files/${fileId}/download" class="table-action-btn">下载</a>
            <button class="table-action-btn delete-file-btn" data-file-id="${fileId}" style="color: var(--error-color);">删除</button>
            </div></td></tr>`;
    }).join('');

    filesTableBody.querySelectorAll('.delete-file-btn').forEach(button => {
        button.addEventListener('click', () => deleteFile(button.dataset.fileId));
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
    } catch (_) {
        showError('删除文件失败，请重试');
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
    registerButton.disabled = isAuthenticated;
    uploadArea.classList.toggle('disabled', !isAuthenticated);
    uploadButton.disabled = !isAuthenticated;
    if (landingLoginBtn) landingLoginBtn.textContent = isAuthenticated ? `你好, ${displayUsername}` : '登录 / 注册';
}

function showAuthModal() {
    clearAuthError();
    authOverlay.classList.add('show');
    authOverlay.setAttribute('aria-hidden', 'false');
}

function hideAuthModal() {
    authOverlay.classList.remove('show');
    authOverlay.setAttribute('aria-hidden', 'true');
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

document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') hideAuthModal();
});

// Dropzone interactions
uploadArea.addEventListener('click', () => {
    if (uploadArea.classList.contains('disabled')) {
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
        showError('文件大小不能超过 5GB');
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
    const password = loginPassword.value.trim();
    if (!username || !password) {
        showAuthError('请输入用户名和密码');
        return null;
    }
    return {username, password};
}

async function submitAuth(path, actionName) {
    const body = credentials();
    if (!body) return;

    try {
        loginButton.disabled = true;
        registerButton.disabled = true;
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
    } catch (error) {
        showAuthError(`${actionName}失败: ${error.message || String(error)}`);
    } finally {
        updateAuthUI();
    }
}

loginButton.addEventListener('click', () => submitAuth('/auth/login', '登录'));
registerButton.addEventListener('click', () => submitAuth('/auth/register', '注册'));

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
        resetUploadDownloadPageState();
        hideAuthModal();
        navigateToLanding();
        refreshFiles();
    } catch (_) {
        showAuthModal();
        showAuthError('退出登录失败，请重试');
    }
}

logoutButton.addEventListener('click', hideAuthModal);
sidebarLogoutBtn.addEventListener('click', executeLogout);

// Copy file share link
copyButton.addEventListener('click', () => {
    const link = shareLink.href;
    copyToClipboard(link).then((ok) => {
        const originalText = copyButton.textContent;
        copyButton.textContent = ok ? '已复制' : '复制失败';
        setTimeout(() => copyButton.textContent = originalText, 1500);
    });
});

function showSuccess(url) {
    shareLink.href = url;
    shareLink.textContent = url;
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
