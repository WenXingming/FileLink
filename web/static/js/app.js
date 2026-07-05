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
        const themeToggle = document.getElementById('themeToggle');

        const loginUser = document.getElementById('loginUser');
        const loginPassword = document.getElementById('loginPassword');
        const loginButton = document.getElementById('loginButton');
        const logoutButton = document.getElementById('logoutButton');
        const loginStatus = document.getElementById('loginStatus');
        const authErrorMessage = document.getElementById('authErrorMessage');

        const authToggle = document.getElementById('authToggle');
        const authOverlay = document.getElementById('authOverlay');

        const THEME_STORAGE_KEY = 'fileshare.theme';
        const TOKEN_STORAGE_KEY = 'fileshare.token';

        let authEnabled = false;
        let loginMode = 'password'; // password | unavailable

        let selectedFile = null;
        let currentXhr = null;
        let isCancelled = false;
        let isPaused = false;

        const MAX_UPLOAD_BYTES = 5 * 1024 * 1024 * 1024;

        // SVG 图标定义，避免字符编码问题
        const sunIcon = `<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="5"></circle><line x1="12" y1="1" x2="12" y2="3"></line><line x1="12" y1="21" x2="12" y2="23"></line><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line><line x1="1" y1="12" x2="3" y2="12"></line><line x1="21" y1="12" x2="23" y2="12"></line><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line></svg>`;
        const moonIcon = `<svg viewBox="0 0 24 24"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path></svg>`;

        let isDarkMode = false;
        let isThemeTransitioning = false;

        function loadThemePreference() {
            // 返回 true/false 或 null（没有保存过）
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
            // 仅用于遮罩层背景：用固定值保证切换过程中不受 CSS 变量变化影响
            if (theme === 'dark') {
                return 'linear-gradient(135deg, #667eea, #764ba2)';
            }
            return 'linear-gradient(135deg, #e0e7ff, #f3e8ff)';
        }

        function setTheme(nextIsDark, persist = true) {
            isDarkMode = nextIsDark;
            if (isDarkMode) {
                document.documentElement.setAttribute('data-theme', 'dark');
                themeToggle.innerHTML = moonIcon;
            } else {
                document.documentElement.removeAttribute('data-theme');
                themeToggle.innerHTML = sunIcon;
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

            // 白天 -> 黑夜：从右下到左上（起点在右下）
            // 黑夜 -> 白天：从左上到右下（起点在左上）
            const revealOrigin = (!isDarkMode && nextIsDark) ? '0% 0%' : '100% 100%';

            // 先切到新主题，再用旧主题遮罩从指定角落收缩，露出新主题
            setTheme(nextIsDark);

            const layer = document.createElement('div');
            layer.className = 'theme-transition-layer';
            layer.style.background = getThemeBackgroundGradient(prevTheme);
            layer.style.clipPath = `circle(150vmax at ${revealOrigin})`;
            document.body.appendChild(layer);

            // 下一帧触发 transition
            requestAnimationFrame(() => {
                layer.style.clipPath = `circle(0vmax at ${revealOrigin})`;
            });

            const cleanup = () => {
                layer.removeEventListener('transitionend', cleanup);
                if (layer.parentNode) layer.parentNode.removeChild(layer);
                isThemeTransitioning = false;
            };
            layer.addEventListener('transitionend', cleanup);

            // 兜底：极端情况下 transitionend 不触发
            setTimeout(cleanup, 1100);
        }

        themeToggle.addEventListener('click', toggleThemeWithDiagonalReveal);

        // 初始化主题：优先本地保存，否则跟随系统主题
        const saved = loadThemePreference();
        if (saved === null) {
            setTheme(getSystemPrefersDark(), false);

            // 仅当“未手动选择过主题”时，跟随系统主题变化
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

        function resetUI() {
            resultArea.style.display = 'none';
            errorMessage.style.display = 'none';
            progressContainer.style.display = 'none';
            progressText.style.display = 'none';
            progressBar.style.width = '0%';
            progressText.textContent = '0%';

            uploadButton.style.display = 'block';
            uploadButton.textContent = 'Start Upload';

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

            resetUI();
        }

        function showAuthError(msg) {
            authErrorMessage.textContent = msg;
            authErrorMessage.style.display = msg ? 'block' : 'none';
        }

        function clearAuthError() {
            showAuthError('');
        }

        function loadToken() {
            try {
                return localStorage.getItem(TOKEN_STORAGE_KEY) || '';
            } catch (_) {
                return '';
            }
        }

        function saveToken(token) {
            try {
                localStorage.setItem(TOKEN_STORAGE_KEY, token);
            } catch (_) { }
        }

        function clearToken() {
            try {
                localStorage.removeItem(TOKEN_STORAGE_KEY);
            } catch (_) { }
        }

        function updateAuthUI() {
            const token = loadToken();
            const hasToken = !!token;
            const canUpload = authEnabled ? hasToken : true;

            const dotClass = !authEnabled ? 'status-dot' : hasToken ? 'status-dot ok' : (loginMode === 'unavailable') ? 'status-dot warn' : 'status-dot';
            let text = '';
            if (!authEnabled) {
                text = '未启用登录';
            } else if (hasToken) {
                text = '已登录';
            } else {
                if (loginMode === 'unavailable') {
                    text = '登录不可用：无法检查服务端状态';
                } else {
                    text = '未登录';
                }
            }
            loginStatus.innerHTML = `<span class="${dotClass}"></span><span>${text}</span>`;

            // UI 始终保留：即使未启用登录，也允许用户尝试点击登录（服务端会返回 404 并提示）。
            loginButton.disabled = loginMode === 'unavailable' || hasToken;
            // “退出”按钮同时作为“关闭弹窗”入口：无论是否已登录都应该可点
            logoutButton.disabled = false;
            logoutButton.textContent = hasToken ? '退出' : '关闭';
            uploadArea.classList.toggle('disabled', !canUpload);
            uploadButton.disabled = !canUpload;
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

        authToggle.addEventListener('click', () => {
            showAuthModal();
        });

        authOverlay.addEventListener('click', (e) => {
            if (e.target === authOverlay) hideAuthModal();
        });

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') hideAuthModal();
        });

        // 不再通过 /auth/status 主动探测服务端是否启用登录。
        // 现在改为：默认认为未启用登录（上传可用）；当上传返回 401 时再提示用户登录。

        uploadArea.addEventListener('click', () => {
            if (uploadArea.classList.contains('disabled')) {
                showError('Please Log in First');
                return;
            }
            fileInput.click();
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
        }

        uploadButton.addEventListener('click', () => {
            if (!selectedFile) return;
            const token = loadToken();
            if (authEnabled && !token) {
                showError('Please Log in First');
                return;
            }

            resetUI();
            fileInfo.style.display = 'block';
            progressContainer.style.display = 'block';
            progressText.style.display = 'block';

            uploadButton.disabled = true;
            uploadButton.textContent = 'Uploading...';
            uploadButton.style.display = 'none';
            pauseButton.style.display = 'block';
            cancelButton.style.display = 'block';

            isCancelled = false;
            isPaused = false;
            startTusUpload(selectedFile, token);
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
                uploadButton.textContent = 'Resume Upload';
                cancelButton.style.display = 'block';
            } else {
                uploadButton.textContent = 'Start Upload';
                cancelButton.style.display = 'none';
            }
            uploadButton.disabled = false;
            uploadButton.style.display = 'block';
            pauseButton.style.display = 'none';
        }

        async function startTusUpload(file, token) {
            const fingerprint = `tus:${file.name}-${file.size}-${file.lastModified}`;
            let sessionUrl = null;
            
            try {
                sessionUrl = localStorage.getItem(fingerprint);
            } catch (_) {}

            let offset = 0;

            if (sessionUrl) {
                try {
                    offset = await getTusSessionOffset(sessionUrl, token);
                } catch (e) {
                    sessionUrl = null;
                    try {
                        localStorage.removeItem(fingerprint);
                    } catch (_) {}
                }
            }

            if (!sessionUrl) {
                try {
                    sessionUrl = await createTusSession(file, token);
                    try {
                        localStorage.setItem(fingerprint, sessionUrl);
                    } catch (_) {}
                    offset = 0;
                } catch (e) {
                    showError('Failed to create upload session: ' + e.message);
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
                    offset = await uploadTusChunk(sessionUrl, chunk, offset, totalSize, token);
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
                    showError('Upload failed: ' + e.message);
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
            progressText.textContent = '99% (Server finalization...)';
            
            try {
                const finalUrl = await pollTusStatus(sessionUrl, token);
                try {
                    localStorage.removeItem(fingerprint);
                } catch (_) {}
                showSuccess(finalUrl);
            } catch (e) {
                showError('Finalization failed: ' + e.message);
            } finally {
                finishUpload();
            }
        }

        function createTusSession(file, token) {
            return new Promise((resolve, reject) => {
                const xhr = new XMLHttpRequest();
                xhr.open('POST', '/uploads', true);
                xhr.setRequestHeader('Tus-Resumable', '1.0.0');
                xhr.setRequestHeader('Upload-Length', file.size.toString());
                xhr.setRequestHeader('Upload-Metadata', `filename ${btoa(unescape(encodeURIComponent(file.name)))}`);
                if (authEnabled && token) {
                    xhr.setRequestHeader('X-Auth-Token', token);
                }
                
                xhr.onload = () => {
                    if (xhr.status === 201) {
                        const location = xhr.getResponseHeader('Location');
                        if (location) {
                            resolve(location);
                        } else {
                            reject(new Error('Missing Location header in response'));
                        }
                    } else if (xhr.status === 401) {
                        authEnabled = true;
                        loginMode = 'password';
                        updateAuthUI();
                        reject(new Error('Please log in first'));
                    } else {
                        reject(new Error(`Server returned ${xhr.status}: ${xhr.responseText}`));
                    }
                };
                xhr.onerror = () => reject(new Error('Network error'));
                xhr.send();
            });
        }

        function getTusSessionOffset(sessionUrl, token) {
            return new Promise((resolve, reject) => {
                const xhr = new XMLHttpRequest();
                xhr.open('HEAD', sessionUrl, true);
                xhr.setRequestHeader('Tus-Resumable', '1.0.0');
                if (authEnabled && token) {
                    xhr.setRequestHeader('X-Auth-Token', token);
                }
                
                xhr.onload = () => {
                    if (xhr.status === 200) {
                        const offsetStr = xhr.getResponseHeader('Upload-Offset');
                        if (offsetStr !== null) {
                            resolve(parseInt(offsetStr, 10));
                        } else {
                            reject(new Error('Missing Upload-Offset header'));
                        }
                    } else {
                        reject(new Error(`Session not found or server returned ${xhr.status}`));
                    }
                };
                xhr.onerror = () => reject(new Error('Network error'));
                xhr.send();
            });
        }

        function uploadTusChunk(sessionUrl, chunk, offset, totalSize, token) {
            return new Promise((resolve, reject) => {
                const xhr = new XMLHttpRequest();
                currentXhr = xhr;
                xhr.open('PATCH', sessionUrl, true);
                xhr.setRequestHeader('Tus-Resumable', '1.0.0');
                xhr.setRequestHeader('Upload-Offset', offset.toString());
                xhr.setRequestHeader('Content-Type', 'application/offset+octet-stream');
                if (authEnabled && token) {
                    xhr.setRequestHeader('X-Auth-Token', token);
                }
                
                // Track progress inside the chunk
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
                        reject(new Error(`Server returned ${xhr.status}: ${xhr.responseText}`));
                    }
                };
                xhr.onerror = () => reject(new Error('Network error'));
                xhr.send(chunk);
            });
        }

        function pollTusStatus(sessionUrl, token) {
            return new Promise((resolve, reject) => {
                let attempts = 0;
                const maxAttempts = 100;
                
                const check = () => {
                    if (isCancelled || isPaused) {
                        reject(new Error(isCancelled ? 'Upload cancelled' : 'Upload paused'));
                        return;
                    }
                    
                    const xhr = new XMLHttpRequest();
                    xhr.open('GET', sessionUrl, true);
                    if (authEnabled && token) {
                        xhr.setRequestHeader('X-Auth-Token', token);
                    }
                    
                    xhr.onload = () => {
                        if (xhr.status === 200) {
                            try {
                                const res = JSON.parse(xhr.responseText);
                                if (res.state === 'COMPLETED') {
                                    if (res.content_hash) {
                                        const dotIdx = res.file_name.lastIndexOf('.');
                                        const ext = dotIdx !== -1 ? res.file_name.substring(dotIdx) : '';
                                        const host = window.location.host;
                                        const shareUrl = `${window.location.protocol}//${host}/objects/${res.content_hash}${ext}`;
                                        resolve(shareUrl);
                                    } else {
                                        reject(new Error('Completed session is missing content_hash'));
                                    }
                                } else if (res.state === 'FAILED') {
                                    reject(new Error(res.failure_reason || 'Verification failed on server'));
                                } else {
                                    attempts++;
                                    if (attempts >= maxAttempts) {
                                        reject(new Error('Server finalization timeout'));
                                    } else {
                                        setTimeout(check, 200);
                                    }
                                }
                            } catch (e) {
                                reject(new Error('Failed to parse server status: ' + e.message));
                            }
                        } else {
                            reject(new Error(`Server status returned ${xhr.status}`));
                        }
                    };
                    xhr.onerror = () => reject(new Error('Network error polling status'));
                    xhr.send();
                };
                
                setTimeout(check, 100);
            });
        }

        loginButton.addEventListener('click', () => {
            const user = loginUser.value.trim();
            const pwd = loginPassword.value.trim();
            if (!user || !pwd) {
                showAuthError('Please enter username and password');
                return;
            }

            if (loginMode === 'unavailable') {
                showAuthError('Login unavailable: server status check failed');
                return;
            }

            (async () => {
                try {
                    loginButton.disabled = true;

                    const resp = await fetch('/login', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ user, password: pwd })
                    });

                    if (resp.status === 404) {
                        authEnabled = false;
                        updateAuthUI();
                        throw new Error('Login not enabled on server');
                    }

                    if (!resp.ok) {
                        const t = await resp.text().catch(() => '');
                        throw new Error(t || ('HTTP ' + resp.status));
                    }

                    const result = await resp.json();
                    if (result && result.token) {
                        authEnabled = true;
                        saveToken(result.token);
                        updateAuthUI();
                        clearAuthError();
                        hideAuthModal();
                        return;
                    }
                    throw new Error('Login response format error');
                } catch (e) {
                    showAuthError('Login failed: ' + (e && e.message ? e.message : String(e)));
                } finally {
                    loginButton.disabled = false;
                }
            })();
        });

        logoutButton.addEventListener('click', () => {
            // 始终关闭弹窗；如果已登录则顺便退出登录（清除本地 token）
            const token = loadToken();
            const wasLoggedIn = !!token;

            if (wasLoggedIn) {
                clearToken();
            }

            resetUploadDownloadPageState();
            updateAuthUI();
            hideAuthModal();

            // 已登录用户点击“退出”后刷新页面，确保上传/下载区域状态完全重置。
            if (wasLoggedIn) {
                window.location.reload();
            }
        });

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
            errorMessage.textContent = msg;
            errorMessage.style.display = msg ? 'block' : 'none';
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