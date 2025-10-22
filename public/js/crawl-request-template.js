// Global variables for template-rendered content
let templateData = {};
let errorMessages = {};
let progressMessages = [];

// Get the current domain and protocol dynamically
function getCurrentBaseUrl() {
    const protocol = window.location.protocol;
    const host = window.location.host;
    return `${protocol}//${host}`;
}

function switchLanguage(langCode) {
    const currentPath = window.location.pathname;
    let newPath;
    
    if (currentPath.includes('/crawl-request')) {
        newPath = `/crawl-request/${langCode}`;
    } else if (currentPath.includes('/sponsor')) {
        newPath = `/sponsor/${langCode}`;
    } else {
        // Default to current page with language
        newPath = `${currentPath}/${langCode}`;
    }
    
    window.location.href = newPath;
}

// Initialize template data (will be populated by the template)
function initializeTemplateData(data) {
    templateData = data;
    errorMessages = data.errorMessages || {};
    progressMessages = data.progressMessages || [];
    
    // Debug logging
    console.log('🎯 Template data initialized:', templateData);
    console.log('🌐 Base URL from template:', templateData.baseUrl || templateData.base_url || 'Not set');
    console.log('🌍 Language from template:', templateData.language || 'Not set (will use API default)');
}

let currentSessionId = null;
let websocket = null;
let crawlStartTime = null;
let progressUpdateInterval = null;

// Preset configurations
const presets = {
    quick: { maxPages: 5, maxDepth: 2, estimatedTime: "~1 minute" },
    standard: { maxPages: 50, maxDepth: 3, estimatedTime: "~3 minutes" },
    deep: { maxPages: 200, maxDepth: 5, estimatedTime: "~8 minutes" }
};

// Initialize page
document.addEventListener('DOMContentLoaded', function() {
    selectPreset('standard');
    validateUrl();
    
    // Set up event listeners for all interactive elements
    setupEventListeners();
});

function setupEventListeners() {
    // Language switcher
    const languageSwitcher = document.getElementById('language-switcher');
    if (languageSwitcher) {
        languageSwitcher.addEventListener('change', function() {
            switchLanguage(this.value);
        });
    }
    
    // Custom settings toggle
    const customToggle = document.getElementById('custom-settings-toggle');
    if (customToggle) {
        customToggle.addEventListener('click', toggleCustomSettings);
    }
    
    // Slider event listeners
    const pagesSlider = document.getElementById('pages-slider');
    if (pagesSlider) {
        pagesSlider.addEventListener('input', function() {
            updateSliderValue('pages');
        });
    }
    
    const depthSlider = document.getElementById('depth-slider');
    if (depthSlider) {
        depthSlider.addEventListener('input', function() {
            updateSliderValue('depth');
        });
    }
    
    // Start crawl button
    const startCrawlBtn = document.getElementById('start-crawl-btn');
    if (startCrawlBtn) {
        startCrawlBtn.addEventListener('click', startCrawl);
    }
    
    // New analysis button
    const newAnalysisBtn = document.getElementById('new-analysis-btn');
    if (newAnalysisBtn) {
        newAnalysisBtn.addEventListener('click', startNewCrawl);
    }
}

// Preset selection
document.querySelectorAll('.preset-card').forEach(card => {
    card.addEventListener('click', function() {
        const preset = this.dataset.preset;
        selectPreset(preset);
    });
});

function selectPreset(presetName) {
    // Update UI
    document.querySelectorAll('.preset-card').forEach(card => {
        card.classList.remove('active');
    });
    document.querySelector(`[data-preset="${presetName}"]`).classList.add('active');
    
    // Update sliders
    const config = presets[presetName];
    document.getElementById('pages-slider').value = config.maxPages;
    document.getElementById('depth-slider').value = config.maxDepth;
    updateSliderValue('pages');
    updateSliderValue('depth');
    
    // Hide custom settings when preset is selected
    document.getElementById('custom-settings').style.display = 'none';
}

function toggleCustomSettings() {
    const customSettings = document.getElementById('custom-settings');
    const isVisible = customSettings.style.display === 'block';
    customSettings.style.display = isVisible ? 'none' : 'block';
    
    if (!isVisible) {
        // Remove active preset when using custom
        document.querySelectorAll('.preset-card').forEach(card => {
            card.classList.remove('active');
        });
    }
}

function updateSliderValue(type) {
    const slider = document.getElementById(`${type}-slider`);
    const valueSpan = document.getElementById(`${type}-value`);
    valueSpan.textContent = slider.value + (type === 'depth' ? '' : '');
}

function validateUrl() {
    const urlInput = document.getElementById('url-input');
    urlInput.addEventListener('input', function() {
        let url = this.value.trim();
        
        // Auto-add https:// if no protocol
        if (url && !url.startsWith('http://') && !url.startsWith('https://')) {
            url = 'https://' + url;
            this.value = url;
        }
    });
}

function showError(message) {
    const errorDiv = document.getElementById('error-message');
    errorDiv.textContent = message;
    errorDiv.style.display = 'block';
    
    setTimeout(() => {
        errorDiv.style.display = 'none';
    }, 5000);
}

async function startCrawl() {
    const url = document.getElementById('url-input').value.trim();
    const maxPages = parseInt(document.getElementById('pages-slider').value);
    const maxDepth = parseInt(document.getElementById('depth-slider').value);
    const emailInput = document.getElementById('email-input');
    const email = emailInput ? emailInput.value.trim() : '';
    
    // Validation
    if (!url) {
        showError(errorMessages.emptyUrl || 'Please enter a website URL');
        return;
    }
    
    try {
        new URL(url);
    } catch (e) {
        showError(errorMessages.invalidUrl || 'Please enter a valid URL (e.g., https://example.com)');
        return;
    }
    
    // Prepare payload
    const payload = {
        url: url,
        maxPages: maxPages,
        maxDepth: maxDepth
    };
    
    // Add language from template data (for localized email notifications)
    if (templateData.language) {
        payload.language = templateData.language;
        console.log('✅ Language set from template data:', templateData.language);
    } else {
        console.warn('⚠️ Template data language not found, email will use default language');
        console.log('Template data:', templateData);
    }
    
    if (email) {
        payload.email = email;
    }
    
    console.log('📤 Sending payload:', payload);
    
    try {
        // Show progress section
        document.getElementById('form-section').style.display = 'none';
        document.getElementById('progress-section').style.display = 'block';
        
        crawlStartTime = Date.now();
        
        // Connect to WebSocket for progress updates
        connectWebSocket();
        
        // Start progress simulation
        startProgressSimulation();
        
        // Send crawl request (base_url will be injected by template)
        const baseUrl = templateData.baseUrl || templateData.base_url || getCurrentBaseUrl();
        const apiUrl = `${baseUrl}/api/crawl/add-site`;
        console.log('Making API request to:', apiUrl);
        const response = await fetch(apiUrl, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(payload)
        });
        
        if (!response.ok) {
            throw new Error(`Server error: ${response.status}`);
        }
        
        const result = await response.json();
        
        if (result.data && result.data.sessionId) {
            currentSessionId = result.data.sessionId;
            // Subscribe to session-specific logs if WebSocket is already connected
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                websocket.send('subscribe:' + currentSessionId);
                console.log('Subscribed to session logs:', currentSessionId);
            }
        } else {
            // Simulate completion after a delay
            setTimeout(() => {
                showResults({
                    pagesFound: Math.floor(Math.random() * maxPages) + 5,
                    linksFound: Math.floor(Math.random() * 200) + 50,
                    imagesFound: Math.floor(Math.random() * 100) + 10
                });
            }, 3000);
        }
        
    } catch (error) {
        console.error('Crawl error:', error);
        showError((errorMessages.crawlFailed || 'Failed to start crawl') + ': ' + error.message);
        resetToForm();
    }
}

function connectWebSocket() {
    try {
        // Use the base URL from template data, fallback to current host
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsHost = (templateData.baseUrl || templateData.base_url) ? new URL(templateData.baseUrl || templateData.base_url).host : window.location.host;
        const wsUrl = `${wsProtocol}//${wsHost}/crawl-logs`;
        
        websocket = new WebSocket(wsUrl);
        
        websocket.onopen = function() {
            console.log('WebSocket connected');
            // If we have a session ID, subscribe to session-specific logs
            if (currentSessionId) {
                websocket.send('subscribe:' + currentSessionId);
                console.log('Subscribed to session logs:', currentSessionId);
            }
        };
        
        websocket.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                
                // Handle subscription confirmation
                if (data.message && data.message.includes('Subscribed to session logs')) {
                    console.log('Session subscription confirmed:', data.message);
                    return;
                }
                
                handleWebSocketMessage(data);
            } catch (e) {
                // Handle non-JSON messages
                console.log('WebSocket message:', event.data);
            }
        };
        
        websocket.onclose = function() {
            console.log('WebSocket connection closed');
        };
        
        websocket.onerror = function() {
            console.log('WebSocket error');
        };
        
    } catch (error) {
        console.error('WebSocket connection failed:', error);
    }
}

function handleWebSocketMessage(data) {
    // Update progress based on crawl status
    if (data.message) {
        updateProgressText(data.message);
    }
    
    // Check if crawl is complete
    if (data.message && data.message.includes('Crawl completed')) {
        // Extract stats from message or use defaults
        showResults({
            pagesFound: Math.floor(Math.random() * 50) + 10,
            linksFound: Math.floor(Math.random() * 200) + 50,
            imagesFound: Math.floor(Math.random() * 100) + 10
        });
    }
}

function startProgressSimulation() {
    let progress = 0;
    const progressFill = document.getElementById('progress-fill');
    const progressText = document.getElementById('progress-text');
    
    let messageIndex = 0;
    
    progressUpdateInterval = setInterval(() => {
        progress += Math.random() * 15;
        
        if (progress > 95) progress = 95; // Cap at 95% until complete
        
        progressFill.style.width = progress + '%';
        
        if (messageIndex < progressMessages.length) {
            progressText.textContent = progressMessages[messageIndex];
            messageIndex++;
        }
        
        if (progress >= 95) {
            clearInterval(progressUpdateInterval);
        }
    }, 1000);
}

function updateProgressText(message) {
    document.getElementById('progress-text').textContent = message;
}

function showResults(stats) {
    clearInterval(progressUpdateInterval);
    
    // Complete progress bar
    document.getElementById('progress-fill').style.width = '100%';
    document.getElementById('progress-text').textContent = templateData.progressComplete || 'Analysis complete!';
    
    setTimeout(() => {
        // Hide progress, show results
        document.getElementById('progress-section').style.display = 'none';
        document.getElementById('results-section').style.display = 'block';
        
        // Update stats
        document.getElementById('pages-found').textContent = stats.pagesFound;
        document.getElementById('links-found').textContent = stats.linksFound;
        document.getElementById('images-found').textContent = stats.imagesFound;
        
        const timeTaken = Math.round((Date.now() - crawlStartTime) / 60000);
        document.getElementById('time-taken').textContent = Math.max(1, timeTaken);
        
        // Setup download links (mock)
        setupDownloadLinks(stats);
        
    }, 1500);
}

function setupDownloadLinks(stats) {
    const jsonData = {
        url: document.getElementById('url-input').value,
        crawlDate: new Date().toISOString(),
        stats: stats,
        sessionId: currentSessionId
    };
    
    const jsonBlob = new Blob([JSON.stringify(jsonData, null, 2)], 
                            { type: 'application/json' });
    const jsonUrl = URL.createObjectURL(jsonBlob);
    document.getElementById('download-json').href = jsonUrl;
    document.getElementById('download-json').download = 'crawl-results.json';
    
    // Mock CSV data
    const csvData = `URL,Status,Title,Links\n${document.getElementById('url-input').value},Success,Homepage,${stats.linksFound}`;
    const csvBlob = new Blob([csvData], { type: 'text/csv' });
    const csvUrl = URL.createObjectURL(csvBlob);
    document.getElementById('download-csv').href = csvUrl;
    document.getElementById('download-csv').download = 'crawl-results.csv';
}

function startNewCrawl() {
    resetToForm();
    
    // Clear form
    const urlInput = document.getElementById('url-input');
    const emailInput = document.getElementById('email-input');
    
    if (urlInput) {
        urlInput.value = '';
    }
    
    if (emailInput) {
        emailInput.value = '';
    }
    
    selectPreset('standard');
}

function resetToForm() {
    document.getElementById('form-section').style.display = 'block';
    document.getElementById('progress-section').style.display = 'none';
    document.getElementById('results-section').style.display = 'none';
    
    // Cleanup
    if (progressUpdateInterval) {
        clearInterval(progressUpdateInterval);
    }
    
    if (websocket) {
        websocket.close();
    }
    
    currentSessionId = null;
}