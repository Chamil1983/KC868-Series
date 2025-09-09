// KC868-A16 Controller JavaScript

// Global variables
let systemData = {};
let analogHistory = {
    labels: [],
    datasets: [{}, {}, {}, {}]
};
let analogChart = null;
let updateTimer = null;
let debugConsole = null;
let webSocketConnected = false;
let ws = null;
let lastDataTimestamp = 0;
let reconnectAttempts = 0;
let maxReconnectAttempts = 10;
let connectionStatusElement = null;

// DOM elements
let sections;
let navLinks;
let toast;

// Wait for DOM to be fully loaded
document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM content loaded');
    
    // Initialize critical UI elements
    sections = document.querySelectorAll('main section');
    navLinks = document.querySelectorAll('nav ul li a');
    toast = document.getElementById('toast');
    
    // Add custom CSS to make input states more visible
    const style = document.createElement('style');
    style.textContent = `
        .input-state {
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background-color: #F44336;
            transition: background-color 0.3s ease;
        }
        
        .input-state.active {
            background-color: #4CAF50;
            box-shadow: 0 0 5px #4CAF50;
        }
        
        .visual-box {
            transition: all 0.3s ease;
        }
        
        .visual-box.on {
            background-color: #4CAF50;
            box-shadow: 0 0 8px #4CAF50;
        }
        
        .visual-box.off {
            background-color: #F44336;
            box-shadow: none;
        }
        
        .connection-status {
            display: inline-block;
            padding: 2px 8px;
            border-radius: 12px;
            font-size: 0.8rem;
            font-weight: bold;
            margin-left: 10px;
        }
        
        .connection-status.connected {
            background-color: #4CAF50;
            color: white;
        }
        
        .connection-status.connecting {
            background-color: #FF9800;
            color: white;
            animation: pulse 1.5s infinite;
        }
        
        .connection-status.disconnected {
            background-color: #F44336;
            color: white;
        }
        
        @keyframes pulse {
            0% { opacity: 0.6; }
            50% { opacity: 1; }
            100% { opacity: 0.6; }
        }
        
        .data-refresh-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background-color: #4CAF50;
            margin-left: 5px;
            animation: fade-out 2s forwards;
        }
        
        @keyframes fade-out {
            from { opacity: 1; }
            to { opacity: 0; }
        }
        
        /* HT Sensors Styles */
        .sensors-container {
            margin-top: 20px;
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
            padding: 15px;
        }

        .sensors-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 15px;
        }

        .sensor-card {
            background-color: #f5f5f5;
            border-radius: 5px;
            padding: 15px;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
        }

        .sensor-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }

        .sensor-header h4 {
            margin: 0;
            color: #1a2b42;
        }

        .sensor-type {
            font-size: 0.8em;
            padding: 2px 5px;
            background-color: #e0e0e0;
            border-radius: 3px;
            color: #333;
        }

        .sensor-value {
            font-size: 1.5em;
            text-align: center;
            padding: 10px;
            font-weight: bold;
            background-color: #e0e0e0;
            border-radius: 5px;
        }

        .sensor-value.active {
            background-color: #4CAF50;
            color: white;
        }

        .sensor-values {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }

        .sensor-temp, .sensor-humidity {
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 10px 5px;
            background-color: #e0e0e0;
            border-radius: 5px;
        }

        .sensor-temp {
            background-color: #ffebee;
            color: #d32f2f;
        }

        .sensor-humidity {
            background-color: #e3f2fd;
            color: #1976d2;
        }

        .sensor-temp i, .sensor-humidity i {
            font-size: 1.5em;
            margin-bottom: 5px;
        }

        .sensor-temp span, .sensor-humidity span {
            font-weight: bold;
        }

        .sensor-config-info {
            background-color: #f9f9f9;
            padding: 10px;
            border-radius: 5px;
            margin: 15px 0;
            border: 1px solid #ddd;
            font-size: 0.9em;
        }
    `;
    document.head.appendChild(style);
    
    // Add connection status indicator to header
    addConnectionStatusIndicator();
    
    // Setup navigation
    setupNavigation();
    
    // Initialize the rest of the app
    initApp();
});

// Add connection status indicator to the header
function addConnectionStatusIndicator() {
    const statusTime = document.querySelector('.status-time');
    if (statusTime) {
        connectionStatusElement = document.createElement('span');
        connectionStatusElement.className = 'connection-status disconnected';
        connectionStatusElement.textContent = 'Disconnected';
        statusTime.appendChild(connectionStatusElement);
    }
}

// Update connection status indicator
function updateConnectionStatus(status) {
    if (!connectionStatusElement) return;
    
    connectionStatusElement.className = 'connection-status ' + status;
    
    switch(status) {
        case 'connected':
            connectionStatusElement.textContent = 'Connected';
            break;
        case 'connecting':
            connectionStatusElement.textContent = 'Connecting...';
            break;
        case 'disconnected':
            connectionStatusElement.textContent = 'Disconnected';
            break;
    }
}

// Add data refresh indicator
function showDataRefreshIndicator() {
    const indicator = document.createElement('span');
    indicator.className = 'data-refresh-indicator';
    
    if (connectionStatusElement && connectionStatusElement.parentNode) {
        connectionStatusElement.parentNode.appendChild(indicator);
        
        // Remove indicator after animation completes
        setTimeout(() => {
            if (indicator && indicator.parentNode) {
                indicator.parentNode.removeChild(indicator);
            }
        }, 2000);
    }
}

// Modified initApp function to ensure frequent status updates
function initApp() {
    console.log('Initializing application');
    
    // Initialize UI components
    initRelayControls();
    initInputControls();
    initScheduleUI();
    initTriggerUI();
    initInputInterruptsUI(); // Add this line to initialize Input Interrupts UI
    initHTSensorsUI();  // Add this line to initialize HT sensors UI
    initSettingsUI();
    initDiagnosticsUI();
    initCommunicationUI();
    initNetworkUI(); 
    
    // Setup event listeners for quick actions
    const allRelaysOnBtn = document.getElementById('all-relays-on');
    if (allRelaysOnBtn) {
        allRelaysOnBtn.addEventListener('click', () => controlAllRelays(true));
    }
    
    const allRelaysOffBtn = document.getElementById('all-relays-off');
    if (allRelaysOffBtn) {
        allRelaysOffBtn.addEventListener('click', () => controlAllRelays(false));
    }
    
    const refreshStatusBtn = document.getElementById('refresh-status');
    if (refreshStatusBtn) {
        refreshStatusBtn.addEventListener('click', refreshSystemStatus);
    }
    
    const rebootDeviceBtn = document.getElementById('reboot-device');
    if (rebootDeviceBtn) {
        rebootDeviceBtn.addEventListener('click', rebootDevice);
    }
    
    // Setup clock
    updateClock();
    setInterval(updateClock, 1000);
    
    // Initialize analog chart
    initAnalogChart();
    
    // Initialize WebSocket connection
    initWebSocket();
    
    // Load initial data
    refreshSystemStatus();
    
    // Setup data freshness monitor
    setupDataFreshnessMonitor();
    
    // Initialize debug console
    initDebugConsole();
    
    // Update current year in footer
    const currentYearEl = document.getElementById('current-year');
    if (currentYearEl) {
        currentYearEl.textContent = new Date().getFullYear();
    }
    
    // Show a welcome toast
    showToast('Welcome to KC868-A16 Smart Home Controller');
}


// Enhance the updateDashboard function to show network info consistently
function updateDashboard(data) {
    // Update device name
    document.getElementById('device-name').textContent = data.device || 'KC868-A16';
    
    // Update system uptime
    document.getElementById('system-uptime').textContent = data.uptime || '0';
    
    // Update connection info
    document.getElementById('wifi-status').className = data.wifi_connected ? 'indicator connected' : 'indicator disconnected';
    document.getElementById('eth-status').className = data.eth_connected ? 'indicator connected' : 'indicator disconnected';
    document.getElementById('protocol-status').textContent = data.active_protocol || 'Unknown';
    
    // Update IP addresses - ensure proper display even when data is missing
    document.getElementById('wifi-ip').textContent = (data.wifi_connected && data.wifi_ip && data.wifi_ip !== '0.0.0.0') ? 
        data.wifi_ip : 'Not connected';
    
    document.getElementById('eth-ip').textContent = (data.eth_connected && data.eth_ip && data.eth_ip !== '0.0.0.0') ? 
        data.eth_ip : 'Not connected';
    
    // Ensure MAC address displays properly
    const macAddress = data.mac || '';
    document.getElementById('mac-address').textContent = 
        (macAddress && macAddress !== '00:00:00:00:00:00') ? macAddress : '--';
    
    document.getElementById('active-protocol').textContent = data.active_protocol || '--';
    
    // Update network mode based on actual connections
    let networkMode = 'Disconnected';
    if (data.eth_connected) networkMode = 'Ethernet';
    else if (data.wifi_client_mode) networkMode = 'WiFi Client';
    else if (data.wifi_ap_mode) networkMode = 'Access Point';
    
    document.getElementById('network-mode').textContent = networkMode;
    
    // Generate and display device serial number
    generateAndDisplaySerialNumber(data);
}




// Generate and display device serial number for dashboard
function generateAndDisplaySerialNumber(data) {
    const deviceIdElement = document.getElementById('device-id');
    const serialNumberElement = document.getElementById('serial-number');
    
    if (!deviceIdElement || !serialNumberElement) return;
    
    // Generate device ID from MAC address (if available)
    let deviceId = "unknown";
    const macAddress = data.mac || '';
    
    if (macAddress && macAddress !== '--' && macAddress !== '00:00:00:00:00:00') {
        // Create a unique ID by removing colons from MAC and taking last 6 chars
        deviceId = macAddress.replace(/:/g, '').slice(-6).toUpperCase();
    }
    
    // Generate serial number format: KC868-A16-YYMMDD-XXXX where XXXX is the device ID
    const now = new Date();
    const year = now.getFullYear().toString().slice(-2);
    const month = (now.getMonth() + 1).toString().padStart(2, '0');
    const day = now.getDate().toString().padStart(2, '0');
    
    const serialNumber = `KC868-A16-${year}${month}${day}-${deviceId}`;
    
    // Update elements
    deviceIdElement.textContent = deviceId;
    serialNumberElement.textContent = serialNumber;
}

// New function to monitor data freshness and ensure real-time updates
function setupDataFreshnessMonitor() {
    // Clear any existing timer
    if (updateTimer) {
        clearInterval(updateTimer);
    }
    
    // Set up a monitor to check if data is fresh
    updateTimer = setInterval(() => {
        const currentTime = Date.now();
        
        // If no data received in the last 5 seconds and WebSocket is supposedly connected
        // or if data is older than 10 seconds regardless of connection status
        if ((webSocketConnected && currentTime - lastDataTimestamp > 5000) || 
            (currentTime - lastDataTimestamp > 10000)) {
            
            console.log('Data refresh required - last update was ' + 
                      ((currentTime - lastDataTimestamp) / 1000) + ' seconds ago');
            
            // Try WebSocket reconnect if it's supposedly connected but no data is coming through
            if (webSocketConnected && currentTime - lastDataTimestamp > 8000) {
                console.log('WebSocket seems stale despite being connected - reconnecting...');
                reconnectWebSocket();
            } else {
                // Fallback to HTTP refresh
                refreshSystemStatus();
            }
        }
    }, 3000); // Check every 3 seconds
}

// Navigation setup with improved analog inputs support
function setupNavigation() {
    console.log(`Found ${navLinks.length} navigation links`);
    console.log(`Found ${sections.length} sections`);
    
    // Fix HTML structure issues - if there are nested sections
    sections.forEach(section => {
        if (section.querySelector('section')) {
            console.warn('Found nested section elements. This can cause navigation issues.');
        }
    });
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('data-section');
            console.log(`Navigation clicked: ${targetId}`);
            
            // Remove active class from all links and sections
            navLinks.forEach(navLink => navLink.classList.remove('active'));
            sections.forEach(section => section.classList.remove('active'));
            
            // Add active class to clicked link
            this.classList.add('active');
            
            // Show the selected section
            const targetSection = document.getElementById(targetId);
            if (targetSection) {
                targetSection.classList.add('active');
                console.log(`Activated section: ${targetId}`);
                
                // If analog inputs section, ensure displays are initialized and updated
                if (targetId === 'analog-inputs') {
                    // Make sure analog visual grid exists
                    ensureAnalogVisualGrid();
                    
                    // Refresh analog display
                    refreshAnalogDisplay();
                    
                    // Update chart if available
                    if (analogChart) {
                        analogChart.update();
                    }
                }
                
                // Load protocol-specific settings if communication section
                if (targetId === 'communication') {
                    loadProtocolSettings();
                }
            } else {
                console.error(`Target section not found: ${targetId}`);
            }
        });
    });
    
    // Fix any section structure issues if needed
    fixSectionStructure();
}

// Fix section structure issues if found
function fixSectionStructure() {
    // Check for content outside of sections
    const main = document.querySelector('main');
    if (main) {
        const directTextNodes = Array.from(main.childNodes)
            .filter(node => node.nodeType === Node.TEXT_NODE && node.textContent.trim().length > 0);
        
        if (directTextNodes.length > 0) {
            console.warn('Found content outside of section elements. This can cause display issues.');
        }
    }
    
    // Check each section for duplicate section tags
    sections.forEach(section => {
        // Remove any "section content" text that might be in the section
        const directTextNodes = Array.from(section.childNodes)
            .filter(node => node.nodeType === Node.TEXT_NODE && 
                   node.textContent.trim().includes('content'));
        
        directTextNodes.forEach(node => {
            section.removeChild(node);
        });
    });
}

// Load protocol-specific settings for the Communication tab
function loadProtocolSettings() {
    // Get the active protocol
    const protocol = document.querySelector('input[name="protocol"]:checked');
    if (!protocol) return;
    
    const protocolValue = protocol.value;
    console.log(`Loading settings for protocol: ${protocolValue}`);
    
    // Request protocol settings via WebSocket
    if (webSocketConnected) {
        const message = {
            command: "get_protocol_config",
            protocol: protocolValue
        };
        ws.send(JSON.stringify(message));
    } else {
        // Fallback to regular HTTP request if WebSocket is not connected
        fetch(`/api/communication/config?protocol=${protocolValue}`)
            .then(response => response.json())
            .then(data => {
                displayProtocolConfig(data);
            })
            .catch(error => {
                console.error('Error fetching protocol configuration:', error);
                showToast('Failed to load protocol configuration', 'error');
            });
    }
}

// Initialize WebSocket connection
// Improved WebSocket initialization with better connection management
function initWebSocket() {
    updateConnectionStatus('connecting');
    
    // Determine if we're using SSL
    const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
    
    // Create WebSocket connection
    const wsUrl = protocol + window.location.hostname + ':81';
    
    // Close existing connection if any
    if (ws) {
        try {
            ws.close();
        } catch(e) {
            console.error('Error closing existing WebSocket:', e);
        }
    }
    
    try {
        ws = new WebSocket(wsUrl);
        
        ws.onopen = function() {
            console.log('WebSocket connected');
            webSocketConnected = true;
            reconnectAttempts = 0;
            updateConnectionStatus('connected');
            
            // Subscribe to real-time updates
            ws.send(JSON.stringify({command: 'subscribe'}));
            
            // Show connection toast
            showToast('Real-time connection established', 'success');
            
            // Request an immediate status update
            refreshSystemStatus();
        };
        
        ws.onclose = function(event) {
            console.log(`WebSocket disconnected, code: ${event.code}, reason: ${event.reason}`);
            webSocketConnected = false;
            updateConnectionStatus('disconnected');
            
            // Try to reconnect with exponential backoff
            const delay = Math.min(1000 * (Math.pow(1.5, reconnectAttempts)), 30000);
            reconnectAttempts++;
            
            if (reconnectAttempts <= maxReconnectAttempts) {
                console.log(`Attempting to reconnect in ${delay/1000} seconds (attempt ${reconnectAttempts}/${maxReconnectAttempts})...`);
                setTimeout(initWebSocket, delay);
                updateConnectionStatus('connecting');
            } else {
                console.log('Maximum reconnection attempts reached. Using HTTP fallback.');
                showToast('Real-time connection failed, using polling instead', 'warning');
                // Ensure we still get updates via HTTP polling
                setupDataFreshnessMonitor();
            }
        };
        
        ws.onerror = function(error) {
            console.error('WebSocket error:', error);
            webSocketConnected = false;
            updateConnectionStatus('disconnected');
            
            // WebSocket error handling is managed by onclose handler
        };
        
        ws.onmessage = function(event) {
            try {
                lastDataTimestamp = Date.now();
                showDataRefreshIndicator();
                
                const data = JSON.parse(event.data);
                
                if (data.type === 'status_update') {
                    // Handle real-time status updates
                    handleStatusUpdate(data);
                }
                else if (data.type === 'relay_update') {
                    // Handle relay status change
                    updateRelayState(data.relay, data.state);
                }
                else if (data.type === 'protocol_config') {
                    // Handle protocol configuration data
                    displayProtocolConfig(data);
                }
            } catch (e) {
                console.error('Error processing WebSocket message:', e);
            }
        };
    } catch (error) {
        console.error('Error creating WebSocket:', error);
        webSocketConnected = false;
        updateConnectionStatus('disconnected');
        
        // Fallback to HTTP polling
        setupDataFreshnessMonitor();
    }
}

// Function to reconnect WebSocket
function reconnectWebSocket() {
    if (ws) {
        try {
            ws.close();
        } catch(e) {
            console.error('Error closing WebSocket during reconnect:', e);
        }
    }
    
    webSocketConnected = false;
    updateConnectionStatus('connecting');
    
    // Small delay before reconnecting
    setTimeout(() => {
        initWebSocket();
    }, 500);
}

// Display protocol-specific configuration in the UI
function displayProtocolConfig(data) {
    const protocol = data.protocol;
    console.log(`Received configuration for protocol: ${protocol}`);
    
    // Clear previous config
    const configContainer = document.getElementById('protocol-config-container');
    if (!configContainer) {
        console.error('Protocol config container not found');
        return;
    }
    
    // Create HTML for protocol-specific settings
    let html = `<h3>${protocol.toUpperCase()} Configuration</h3><div class="protocol-form">`;
    
    if (protocol === 'wifi') {
        html += `
            <div class="form-group">
                <label for="wifi-ssid-config">Network Name (SSID)</label>
                <input type="text" id="wifi-ssid-config" value="${data.ssid || ''}">
            </div>
            <div class="form-group">
                <label for="wifi-security">Security Type</label>
                <select id="wifi-security">
                    <option value="WPA2" ${data.security === 'WPA2' ? 'selected' : ''}>WPA2</option>
                    <option value="WPA" ${data.security === 'WPA' ? 'selected' : ''}>WPA</option>
                    <option value="WEP" ${data.security === 'WEP' ? 'selected' : ''}>WEP</option>
                    <option value="OPEN" ${data.security === 'OPEN' ? 'selected' : ''}>Open</option>
                </select>
            </div>
            <div class="form-group">
                <label for="wifi-hidden">Hidden Network</label>
                <input type="checkbox" id="wifi-hidden" ${data.hidden ? 'checked' : ''}>
            </div>
            <div class="form-group">
                <label for="wifi-channel">Channel</label>
                <select id="wifi-channel">
                    ${Array(13).fill(0).map((_, i) => 
                        `<option value="${i+1}" ${data.channel === i+1 ? 'selected' : ''}>${i+1}</option>`
                    ).join('')}
                </select>
            </div>
            <div class="form-group">
                <label for="wifi-channel-width">Channel Width</label>
                <select id="wifi-channel-width">
                    <option value="20" ${data.channel_width === 20 ? 'selected' : ''}>20 MHz</option>
                    <option value="40" ${data.channel_width === 40 ? 'selected' : ''}>40 MHz</option>
                </select>
            </div>
            <div class="form-group">
                <label for="wifi-radio-mode">Radio Mode</label>
                <select id="wifi-radio-mode">
                    <option value="802.11b" ${data.radio_mode === '802.11b' ? 'selected' : ''}>802.11b</option>
                    <option value="802.11g" ${data.radio_mode === '802.11g' ? 'selected' : ''}>802.11g</option>
                    <option value="802.11n" ${data.radio_mode === '802.11n' ? 'selected' : ''}>802.11n</option>
                </select>
            </div>
            <div class="form-group">
                <label for="wifi-wmm-enabled">Enable WMM (WiFi Multimedia)</label>
                <input type="checkbox" id="wifi-wmm-enabled" ${data.wmm_enabled ? 'checked' : ''}>
            </div>`;
            
        // Add current status if connected
        if (data.ip) {
            html += `
                <div class="status-card">
                    <h4>Current Status</h4>
                    <p><strong>IP Address:</strong> ${data.ip}</p>
                    <p><strong>MAC Address:</strong> ${data.mac}</p>
                    <p><strong>Signal Strength:</strong> ${data.rssi} dBm</p>
                </div>
            `;
        }
    }
    else if (protocol === 'ethernet') {
        html += `
            <div class="form-group">
                <label for="eth-dhcp-mode">Use DHCP</label>
                <input type="checkbox" id="eth-dhcp-mode" ${data.dhcp_mode ? 'checked' : ''}>
            </div>`;
        
        if (!data.dhcp_mode) {
            html += `
                <div id="eth-static-settings">
                    <div class="form-group">
                        <label for="eth-ip">IP Address</label>
                        <input type="text" id="eth-ip" value="${data.ip || ''}">
                    </div>
                    <div class="form-group">
                        <label for="eth-gateway">Gateway</label>
                        <input type="text" id="eth-gateway" value="${data.gateway || ''}">
                    </div>
                    <div class="form-group">
                        <label for="eth-subnet">Subnet Mask</label>
                        <input type="text" id="eth-subnet" value="${data.subnet || '255.255.255.0'}">
                    </div>
                    <div class="form-group">
                        <label for="eth-dns1">Primary DNS</label>
                        <input type="text" id="eth-dns1" value="${data.dns1 || '8.8.8.8'}">
                    </div>
                    <div class="form-group">
                        <label for="eth-dns2">Secondary DNS</label>
                        <input type="text" id="eth-dns2" value="${data.dns2 || '8.8.4.4'}">
                    </div>
                </div>`;
        }
        
        // Add current status if connected
        if (data.eth_ip) {
            html += `
                <div class="status-card">
                    <h4>Current Status</h4>
                    <p><strong>IP Address:</strong> ${data.eth_ip}</p>
                    <p><strong>MAC Address:</strong> ${data.eth_mac}</p>
                    <p><strong>Link Speed:</strong> ${data.eth_speed}</p>
                    <p><strong>Duplex Mode:</strong> ${data.eth_duplex}</p>
                </div>
            `;
        }
    }
    else if (protocol === 'usb') {
        html += `
            <div class="form-group">
                <label for="usb-baud-rate">Baud Rate</label>
                <select id="usb-baud-rate">
                    ${data.available_baud_rates ? 
                        data.available_baud_rates.map(rate => 
                            `<option value="${rate}" ${data.baud_rate === rate ? 'selected' : ''}>${rate}</option>`
                        ).join('') :
                        `<option value="115200" ${data.baud_rate === 115200 ? 'selected' : ''}>115200</option>
                         <option value="57600" ${data.baud_rate === 57600 ? 'selected' : ''}>57600</option>
                         <option value="38400" ${data.baud_rate === 38400 ? 'selected' : ''}>38400</option>
                         <option value="19200" ${data.baud_rate === 19200 ? 'selected' : ''}>19200</option>
                         <option value="9600" ${data.baud_rate === 9600 ? 'selected' : ''}>9600</option>`
                    }
                </select>
            </div>
            <div class="form-group">
                <label for="usb-data-bits">Data Bits</label>
                <select id="usb-data-bits">
                    <option value="8" ${data.data_bits === 8 ? 'selected' : ''}>8</option>
                    <option value="7" ${data.data_bits === 7 ? 'selected' : ''}>7</option>
                </select>
            </div>
            <div class="form-group">
                <label for="usb-parity">Parity</label>
                <select id="usb-parity">
                    <option value="0" ${data.parity === 0 ? 'selected' : ''}>None</option>
                    <option value="1" ${data.parity === 1 ? 'selected' : ''}>Odd</option>
                    <option value="2" ${data.parity === 2 ? 'selected' : ''}>Even</option>
                </select>
            </div>
            <div class="form-group">
                <label for="usb-stop-bits">Stop Bits</label>
                <select id="usb-stop-bits">
                    <option value="1" ${data.stop_bits === 1 ? 'selected' : ''}>1</option>
                    <option value="2" ${data.stop_bits === 2 ? 'selected' : ''}>2</option>
                </select>
            </div>
            <div class="form-group">
                <label for="usb-com-port">COM Port Number (virtual)</label>
                <input type="number" id="usb-com-port" min="0" max="255" value="${data.com_port || 0}">
            </div>`;
    }
    else if (protocol === 'rs485') {
        html += `
            <div class="form-group">
                <label for="rs485-protocol-type">Protocol Type</label>
                <select id="rs485-protocol-type">
                    ${data.available_protocols ? 
                        data.available_protocols.map(p => 
                            `<option value="${p}" ${data.protocol_type === p ? 'selected' : ''}>${p}</option>`
                        ).join('') :
                        `<option value="Modbus RTU" ${data.protocol_type === 'Modbus RTU' ? 'selected' : ''}>Modbus RTU</option>
                         <option value="BACnet" ${data.protocol_type === 'BACnet' ? 'selected' : ''}>BACnet</option>
                         <option value="Custom ASCII" ${data.protocol_type === 'Custom ASCII' ? 'selected' : ''}>Custom ASCII</option>
                         <option value="Custom Binary" ${data.protocol_type === 'Custom Binary' ? 'selected' : ''}>Custom Binary</option>`
                    }
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-comm-mode">Communication Mode</label>
                <select id="rs485-comm-mode">
                    ${data.available_modes ? 
                        data.available_modes.map(m => 
                            `<option value="${m}" ${data.comm_mode === m ? 'selected' : ''}>${m}</option>`
                        ).join('') :
                        `<option value="Half-duplex" ${data.comm_mode === 'Half-duplex' ? 'selected' : ''}>Half-duplex</option>
                         <option value="Full-duplex" ${data.comm_mode === 'Full-duplex' ? 'selected' : ''}>Full-duplex</option>
                         <option value="Log Mode" ${data.comm_mode === 'Log Mode' ? 'selected' : ''}>Log Mode</option>
                         <option value="NMEA Mode" ${data.comm_mode === 'NMEA Mode' ? 'selected' : ''}>NMEA Mode</option>
                         <option value="TCP ASCII" ${data.comm_mode === 'TCP ASCII' ? 'selected' : ''}>TCP ASCII</option>
                         <option value="TCP Binary" ${data.comm_mode === 'TCP Binary' ? 'selected' : ''}>TCP Binary</option>`
                    }
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-baud-rate">Baud Rate</label>
                <select id="rs485-baud-rate">
                    ${data.available_baud_rates ? 
                        data.available_baud_rates.map(rate => 
                            `<option value="${rate}" ${data.baud_rate === rate ? 'selected' : ''}>${rate}</option>`
                        ).join('') :
                        `<option value="1200" ${data.baud_rate === 1200 ? 'selected' : ''}>1200</option>
                         <option value="2400" ${data.baud_rate === 2400 ? 'selected' : ''}>2400</option>
                         <option value="4800" ${data.baud_rate === 4800 ? 'selected' : ''}>4800</option>
                         <option value="9600" ${data.baud_rate === 9600 ? 'selected' : ''}>9600</option>
                         <option value="19200" ${data.baud_rate === 19200 ? 'selected' : ''}>19200</option>
                         <option value="38400" ${data.baud_rate === 38400 ? 'selected' : ''}>38400</option>
                         <option value="57600" ${data.baud_rate === 57600 ? 'selected' : ''}>57600</option>
                         <option value="115200" ${data.baud_rate === 115200 ? 'selected' : ''}>115200</option>`
                    }
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-data-bits">Data Bits</label>
                <select id="rs485-data-bits">
                    <option value="8" ${data.data_bits === 8 ? 'selected' : ''}>8</option>
                    <option value="7" ${data.data_bits === 7 ? 'selected' : ''}>7</option>
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-parity">Parity</label>
                <select id="rs485-parity">
                    <option value="0" ${data.parity === 0 ? 'selected' : ''}>None</option>
                    <option value="1" ${data.parity === 1 ? 'selected' : ''}>Odd</option>
                    <option value="2" ${data.parity === 2 ? 'selected' : ''}>Even</option>
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-stop-bits">Stop Bits</label>
                <select id="rs485-stop-bits">
                    <option value="1" ${data.stop_bits === 1 ? 'selected' : ''}>1</option>
                    <option value="2" ${data.stop_bits === 2 ? 'selected' : ''}>2</option>
                </select>
            </div>
            <div class="form-group">
                <label for="rs485-device-address">Device Address</label>
                <input type="number" id="rs485-device-address" min="1" max="255" value="${data.device_address || 1}">
            </div>
            <div class="form-group">
                <label for="rs485-flow-control">Flow Control</label>
                <input type="checkbox" id="rs485-flow-control" ${data.flow_control ? 'checked' : ''}>
            </div>
            <div class="form-group">
                <label for="rs485-night-mode">Night Mode</label>
                <input type="checkbox" id="rs485-night-mode" ${data.night_mode ? 'checked' : ''}>
            </div>`;
    }
    
    // Add save button
    html += `</div>
        <div class="form-actions">
            <button type="button" id="save-protocol-config" class="btn btn-success" data-protocol="${protocol}">
                Save ${protocol.toUpperCase()} Configuration
            </button>
        </div>`;
    
    // Update the container
    configContainer.innerHTML = html;
    
    // Setup event handlers for dynamic elements
    setupProtocolConfigEventHandlers(protocol);
}

// Setup event handlers for protocol-specific configuration elements
function setupProtocolConfigEventHandlers(protocol) {
    // Common save button handler
    const saveBtn = document.getElementById('save-protocol-config');
    if (saveBtn) {
        saveBtn.addEventListener('click', function() {
            saveProtocolConfiguration(protocol);
        });
    }
    
    // Protocol-specific handlers
    if (protocol === 'ethernet') {
        const dhcpToggle = document.getElementById('eth-dhcp-mode');
        const staticSettings = document.getElementById('eth-static-settings');
        
        if (dhcpToggle && staticSettings) {
            dhcpToggle.addEventListener('change', function() {
                staticSettings.style.display = this.checked ? 'none' : 'block';
            });
        }
    }
}

// Save protocol-specific configuration
function saveProtocolConfiguration(protocol) {
    const configData = {
        protocol: protocol
    };
    
    // Collect protocol-specific settings
    if (protocol === 'wifi') {
        configData.security = document.getElementById('wifi-security').value;
        configData.hidden = document.getElementById('wifi-hidden').checked;
        configData.radio_mode = document.getElementById('wifi-radio-mode').value;
        configData.channel = parseInt(document.getElementById('wifi-channel').value);
        configData.channel_width = parseInt(document.getElementById('wifi-channel-width').value);
        configData.wmm_enabled = document.getElementById('wifi-wmm-enabled').checked;
        
        // If SSID is provided in the config screen, also update it in settings
        const ssidConfig = document.getElementById('wifi-ssid-config');
        if (ssidConfig && ssidConfig.value) {
            configData.ssid = ssidConfig.value;
        }
    }
    else if (protocol === 'ethernet') {
        configData.dhcp_mode = document.getElementById('eth-dhcp-mode').checked;
        
        if (!configData.dhcp_mode) {
            configData.ip = document.getElementById('eth-ip').value;
            configData.gateway = document.getElementById('eth-gateway').value;
            configData.subnet = document.getElementById('eth-subnet').value;
            configData.dns1 = document.getElementById('eth-dns1').value;
            configData.dns2 = document.getElementById('eth-dns2').value;
        }
    }
    else if (protocol === 'usb') {
        configData.baud_rate = parseInt(document.getElementById('usb-baud-rate').value);
        configData.data_bits = parseInt(document.getElementById('usb-data-bits').value);
        configData.parity = parseInt(document.getElementById('usb-parity').value);
        configData.stop_bits = parseInt(document.getElementById('usb-stop-bits').value);
        configData.com_port = parseInt(document.getElementById('usb-com-port').value);
    }
    else if (protocol === 'rs485') {
        configData.protocol_type = document.getElementById('rs485-protocol-type').value;
        configData.comm_mode = document.getElementById('rs485-comm-mode').value;
        configData.baud_rate = parseInt(document.getElementById('rs485-baud-rate').value);
        configData.data_bits = parseInt(document.getElementById('rs485-data-bits').value);
        configData.parity = parseInt(document.getElementById('rs485-parity').value);
        configData.stop_bits = parseInt(document.getElementById('rs485-stop-bits').value);
        configData.device_address = parseInt(document.getElementById('rs485-device-address').value);
        configData.flow_control = document.getElementById('rs485-flow-control').checked;
        configData.night_mode = document.getElementById('rs485-night-mode').checked;
    }
    
    // Send configuration to server
    fetch('/api/communication/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(configData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`${protocol.toUpperCase()} configuration saved successfully`, 'success');
        } else {
            showToast(`Failed to save configuration: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving protocol configuration:', error);
        showToast('Network error. Could not save configuration', 'error');
    });
}

// New function to ensure analog visualizations are created properly
function ensureAnalogVisualGrid() {
    // Check if visual grid exists and create if needed
    const visualGrid = document.querySelector('.analog-visual-grid');
    if (visualGrid && visualGrid.children.length === 0) {
        createVisualAnalogGrid();
    }
}

// New function to completely refresh the analog display
function refreshAnalogDisplay() {
    // Check if we are on the analog inputs page
    if (document.getElementById('analog-inputs').classList.contains('active')) {
        console.log("Refreshing analog display with data:", systemData.analog);
        
        // Ensure the analog grid exists and is populated
        const analogInputsGrid = document.getElementById('analog-inputs-grid');
        if (analogInputsGrid) {
            analogInputsGrid.innerHTML = '';
            
            // Create analog cards if we have analog data
            if (systemData.analog && systemData.analog.length > 0) {
                systemData.analog.forEach(input => {
                    const analogCard = document.createElement('div');
                    analogCard.className = 'analog-card';
                    analogCard.setAttribute('data-input', input.id);
                    
                    // Format voltage to 2 decimal places for display
                    const voltage = input.voltage.toFixed(2);
                    
                    analogCard.innerHTML = `
                        <div class="analog-header">
                            <h4>A${input.id + 1}</h4>
                            <span class="analog-value">${input.value} (${voltage}V)</span>
                        </div>
                        <div class="analog-percentage">${input.percentage}%</div>
                        <div class="analog-progress">
                            <div class="analog-fill" style="width: ${input.percentage}%"></div>
                        </div>
                    `;
                    analogInputsGrid.appendChild(analogCard);
                });
                
                // Make sure the analog bar visualizations are created and updated
                updateAnalogVisualizations(systemData.analog);
                
                // Update the chart
                updateAnalogChart();
            } else {
                // If no analog data is available, display a message
                analogInputsGrid.innerHTML = '<div class="no-data">No analog input data available</div>';
            }
        }
    }
}


// Improved handleStatusUpdate function with better network data handling
function handleStatusUpdate(data) {
    // Update outputs
    if (data.outputs) {
        data.outputs.forEach(output => {
            updateRelayState(output.id, output.state);
        });
        
        // Update active outputs list on dashboard
        const activeOutputs = data.outputs.filter(output => output.state);
        const activeOutputsElement = document.getElementById('active-outputs');
        if (activeOutputsElement) {
            if (activeOutputs.length > 0) {
                activeOutputsElement.innerHTML = '';
                activeOutputs.forEach(output => {
                    const div = document.createElement('div');
                    div.className = 'input-item';
                    div.textContent = `Relay ${output.id + 1}`;
                    activeOutputsElement.appendChild(div);
                });
            } else {
                activeOutputsElement.innerHTML = '<p>No active outputs</p>';
            }
        }
    }
    
    // Update inputs
    if (data.inputs) {
        data.inputs.forEach(input => {
            updateInputState(input.id, input.state);
        });
        
        // Update active inputs list on dashboard
        const activeInputs = data.inputs.filter(input => input.state);
        const activeInputsElement = document.getElementById('active-inputs');
        if (activeInputsElement) {
            if (activeInputs.length > 0 || (data.direct_inputs && data.direct_inputs.some(i => i.state))) {
                activeInputsElement.innerHTML = '';
                
                activeInputs.forEach(input => {
                    const div = document.createElement('div');
                    div.className = 'input-item';
                    div.textContent = `Input ${input.id + 1}`;
                    activeInputsElement.appendChild(div);
                });
                
                if (data.direct_inputs) {
                    data.direct_inputs
                        .filter(input => input.state)
                        .forEach(input => {
                            const div = document.createElement('div');
                            div.className = 'input-item';
                            div.textContent = `HT${input.id + 1}`;
                            activeInputsElement.appendChild(div);
                        });
                }
            } else {
                activeInputsElement.innerHTML = '<p>No active inputs</p>';
            }
        }
    }
    
    // Update direct inputs
    if (data.direct_inputs) {
        data.direct_inputs.forEach(input => {
            updateDirectInputState(input.id, input.state);
        });
    }
    
    // Update HT sensors
    if (data.htSensors) {
        // Store HT sensor data in systemData
        if (!systemData.htSensors) {
            systemData.htSensors = [];
        }
        systemData.htSensors = data.htSensors;
        
        // Render the sensors in the UI
        renderHTSensors(data.htSensors);
    }
    
    // Update analog inputs with improved handling
    if (data.analog) {
        // Store data in systemData before processing it
        if (!systemData.analog) {
            systemData.analog = [];
        }
        
        // Update analog data in systemData
        data.analog.forEach((input, index) => {
            if (!systemData.analog[index]) {
                systemData.analog[index] = {};
            }
            systemData.analog[index] = input;
            
            // Update UI elements
            updateAnalogValue(index, input.value);
        });
        
        // If we're on the analog inputs page, make sure to fully refresh the display
        if (document.getElementById('analog-inputs').classList.contains('active')) {
            refreshAnalogDisplay();
        }
    }
    
    // Update dashboard with the improved network information
    updateDashboard(data);
    
    // Update time if provided
    if (data.time) {
        document.getElementById('current-time').textContent = data.time;
    }
    
    // Update systemData with the latest data
    systemData = data;
}


// Initialize relay controls
function initRelayControls() {
    const relaysGrid = document.getElementById('relays-grid');
    if (!relaysGrid) return;
    
    // Create relay cards
    for (let i = 0; i < 16; i++) {
        const relayCard = document.createElement('div');
        relayCard.className = 'relay-card';
        relayCard.innerHTML = `
            <div class="relay-header">
                <span class="relay-name">Relay ${i+1}</span>
                <input type="checkbox" class="relay-checkbox" data-relay="${i}">
            </div>
            <div class="relay-controls">
                <label class="switch">
                    <input type="checkbox" class="relay-toggle" data-relay="${i}">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="relay-status">
                <span class="status-label">Status:</span>
                <span class="status-indicator" id="relay-status-${i}"></span>
            </div>
        `;
        relaysGrid.appendChild(relayCard);
        
        // Add event listener to toggle switch
        const toggleSwitch = relayCard.querySelector('.relay-toggle');
        toggleSwitch.addEventListener('change', function() {
            const relayId = this.getAttribute('data-relay');
            const state = this.checked;
            controlRelay(relayId, state);
        });
    }
    
    // Setup select all checkbox
    const selectAllCheckbox = document.getElementById('select-all-relays');
    if (selectAllCheckbox) {
        selectAllCheckbox.addEventListener('change', function() {
            const isChecked = this.checked;
            document.querySelectorAll('.relay-checkbox').forEach(checkbox => {
                checkbox.checked = isChecked;
            });
        });
    }
    
    // Setup selected relays on/off buttons
    const selectedRelaysOnBtn = document.getElementById('selected-relays-on');
    if (selectedRelaysOnBtn) {
        selectedRelaysOnBtn.addEventListener('click', function() {
            controlSelectedRelays(true);
        });
    }
    
    const selectedRelaysOffBtn = document.getElementById('selected-relays-off');
    if (selectedRelaysOffBtn) {
        selectedRelaysOffBtn.addEventListener('click', function() {
            controlSelectedRelays(false);
        });
    }
    
    // Create visual output grid
    createVisualOutputGrid();
}

// Create visual representation of outputs
function createVisualOutputGrid() {
    const visualGrid = document.querySelector('.output-visual-grid');
    if (!visualGrid) return;
    
    visualGrid.innerHTML = '';
    
    for (let i = 0; i < 16; i++) {
        const visualItem = document.createElement('div');
        visualItem.className = 'visual-item';
        visualItem.innerHTML = `
            <div class="visual-box off" id="output-visual-${i}">${i+1}</div>
            <span class="visual-label">Relay ${i+1}</span>
        `;
        visualGrid.appendChild(visualItem);
    }
}

// Create visual representation of inputs
function createVisualInputGrid() {
    const visualGrid = document.querySelector('.input-visual-grid');
    if (!visualGrid) return;
    
    visualGrid.innerHTML = '';
    
    // Digital inputs X1-X16
    for (let i = 0; i < 16; i++) {
        const visualItem = document.createElement('div');
        visualItem.className = 'visual-item';
        visualItem.innerHTML = `
            <div class="visual-box off" id="input-visual-${i}">X${i+1}</div>
            <span class="visual-label">Input ${i+1}</span>
        `;
        visualGrid.appendChild(visualItem);
    }
    
    // Direct inputs HT1-HT3
    for (let i = 0; i < 3; i++) {
        const visualItem = document.createElement('div');
        visualItem.className = 'visual-item';
        visualItem.innerHTML = `
            <div class="visual-box off" id="direct-visual-${i}">HT${i+1}</div>
            <span class="visual-label">HT${i+1}</span>
        `;
        visualGrid.appendChild(visualItem);
    }
}

// Create visual representation of analog inputs
function createVisualAnalogGrid() {
    const visualGrid = document.querySelector('.analog-visual-grid');
    if (!visualGrid) return;
    
    visualGrid.innerHTML = '';
    
    for (let i = 0; i < 4; i++) {
        const visualItem = document.createElement('div');
        visualItem.className = 'analog-visual-item';
        visualItem.innerHTML = `
            <div class="analog-bar-container">
                <div class="analog-bar" id="analog-bar-${i}" style="height: 0%"></div>
                <span class="analog-bar-value" id="analog-bar-value-${i}">0</span>
                <span class="analog-bar-label" id="analog-bar-label-${i}">0%</span>
            </div>
            <span class="visual-label">Analog ${i+1}</span>
        `;
        visualGrid.appendChild(visualItem);
    }
}

// Initialize Schedule UI - Modified to support both HIGH and LOW relay conditions
function initScheduleUI() {
    console.log("Initializing Schedule UI");
    
    // Load schedules from server
    fetchSchedules();
    
    // Setup add schedule button
    const addScheduleBtn = document.getElementById('add-schedule');
    if (addScheduleBtn) {
        console.log("Add schedule button found, adding event listener");
        addScheduleBtn.addEventListener('click', function() {
            console.log("Add schedule button clicked");
            openScheduleModal();
        });
    } else {
        console.error("Add schedule button not found!");
    }
    
    // Setup modal close buttons - More robust approach
    const closeButtons = document.querySelectorAll('#schedule-modal .close-modal, #schedule-modal .close-btn');
    console.log(`Found ${closeButtons.length} close buttons`);
    
    closeButtons.forEach(btn => {
        btn.addEventListener('click', function() {
            console.log("Close button clicked");
            const modal = document.getElementById('schedule-modal');
            if (modal) {
                modal.style.display = 'none';
            }
        });
    });
    
    // Setup form submission
    const scheduleForm = document.getElementById('schedule-form');
    if (scheduleForm) {
        scheduleForm.addEventListener('submit', function(e) {
            e.preventDefault();
            saveSchedule();
        });
    } else {
        console.error("Schedule form not found!");
    }
    
    // Setup trigger type change
    const triggerTypeSelect = document.getElementById('schedule-trigger-type');
    if (triggerTypeSelect) {
        triggerTypeSelect.addEventListener('change', function() {
            updateVisibleTriggerSections(this.value);
        });
    }
    
    // Setup target type change
    const targetTypeSelect = document.getElementById('schedule-target-type');
    if (targetTypeSelect) {
        targetTypeSelect.addEventListener('change', function() {
            const singleTarget = document.getElementById('single-target');
            const multipleTargets = document.getElementById('multiple-targets');
            const inputConditions = document.getElementById('input-condition-sections');
            const triggerType = parseInt(document.getElementById('schedule-trigger-type').value);
            
            if (this.value === '0') {
                // Single target mode
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                // Multiple targets mode
                if (singleTarget) singleTarget.style.display = 'none';
                
                // For input-based or combined triggers, show input condition sections
                if (triggerType === 1 || triggerType === 2 || triggerType === 3 || triggerType === 4) {
                    if (multipleTargets) multipleTargets.style.display = 'none';
                    if (inputConditions) inputConditions.style.display = 'block';
                } else {
                    // For time-based, show regular multiple targets
                    if (multipleTargets) multipleTargets.style.display = 'block';
                    if (inputConditions) inputConditions.style.display = 'none';
                }
            }
        });
    }
    
    // Fill relay options for single target
    const targetSelect = document.getElementById('schedule-target-id');
    if (targetSelect) {
        targetSelect.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const option = document.createElement('option');
            option.value = i;
            option.textContent = `Relay ${i+1}`;
            targetSelect.appendChild(option);
        }
    }
    
    // Fill relay checkboxes for multiple targets (time-based)
    const checkboxGrid = document.getElementById('relay-checkboxes');
    if (checkboxGrid) {
        checkboxGrid.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-relay', i);
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Relay ${i+1}`));
            checkboxGrid.appendChild(label);
        }
    }
    
    // Fill relay checkboxes for HIGH condition (input-based)
    const highCheckboxGrid = document.getElementById('relay-checkboxes-high');
    if (highCheckboxGrid) {
        highCheckboxGrid.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-relay', i);
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Relay ${i+1}`));
            highCheckboxGrid.appendChild(label);
        }
    }
    
    // Fill relay checkboxes for LOW condition (input-based)
    const lowCheckboxGrid = document.getElementById('relay-checkboxes-low');
    if (lowCheckboxGrid) {
        lowCheckboxGrid.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-relay', i);
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Relay ${i+1}`));
            lowCheckboxGrid.appendChild(label);
        }
    }
    
    // Fill input checkboxes with state selects - FIXED VERSION
    const inputCheckboxes = document.getElementById('input-checkboxes');
    if (inputCheckboxes) {
        inputCheckboxes.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const container = document.createElement('div');
            container.className = 'input-container';
            
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-input', i);
            
            // Create state select that appears when checked
            const stateSelect = document.createElement('select');
            stateSelect.className = 'input-state-select';
            stateSelect.style.display = 'none'; // Initially hidden
            stateSelect.innerHTML = `
                <option value="0">LOW</option>
                <option value="1">HIGH</option>
            `;
            
            // Show/hide state select when checkbox changes
            checkbox.addEventListener('change', function() {
                stateSelect.style.display = this.checked ? 'inline-block' : 'none';
            });
            
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Input ${i+1} `));
            container.appendChild(label);
            container.appendChild(stateSelect);
            
            inputCheckboxes.appendChild(container);
        }
    }
    
    // Fill HT input checkboxes with state selects - FIXED VERSION
    const htInputCheckboxes = document.getElementById('ht-input-checkboxes');
    if (htInputCheckboxes) {
        htInputCheckboxes.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            const container = document.createElement('div');
            container.className = 'input-container';
            
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-input', i + 16); // HT inputs are at bits 16-18
            
            // Create state select that appears when checked
            const stateSelect = document.createElement('select');
            stateSelect.className = 'input-state-select';
            stateSelect.style.display = 'none'; // Initially hidden
            stateSelect.innerHTML = `
                <option value="0">LOW</option>
                <option value="1">HIGH</option>
            `;
            
            // Show/hide state select when checkbox changes
            checkbox.addEventListener('change', function() {
                stateSelect.style.display = this.checked ? 'inline-block' : 'none';
            });
            
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` HT${i+1} `));
            container.appendChild(label);
            container.appendChild(stateSelect);
            
            htInputCheckboxes.appendChild(container);
        }
    }
    
    // Create input condition sections if they don't exist
    if (!document.getElementById('input-condition-sections')) {
        const inputTriggerSection = document.getElementById('input-trigger-section');
        if (inputTriggerSection) {
            const conditionsSection = document.createElement('div');
            conditionsSection.id = 'input-condition-sections';
            conditionsSection.style.display = 'none';
            
            // Create HIGH condition section
            const highSection = document.createElement('div');
            highSection.className = 'form-group input-condition-section';
            highSection.innerHTML = `
                <label>Relays to control when inputs are HIGH</label>
                <div id="relay-checkboxes-high" class="checkbox-grid"></div>
            `;
            
            // Create LOW condition section
            const lowSection = document.createElement('div');
            lowSection.className = 'form-group input-condition-section';
            lowSection.innerHTML = `
                <label>Relays to control when inputs are LOW</label>
                <div id="relay-checkboxes-low" class="checkbox-grid"></div>
            `;
            
            conditionsSection.appendChild(highSection);
            conditionsSection.appendChild(lowSection);
            inputTriggerSection.appendChild(conditionsSection);
            
            // Populate the relay checkboxes
            const highGrid = document.getElementById('relay-checkboxes-high');
            const lowGrid = document.getElementById('relay-checkboxes-low');
            
            if (highGrid && lowGrid) {
                for (let i = 0; i < 16; i++) {
                    // HIGH condition checkboxes
                    const highLabel = document.createElement('label');
                    const highCheckbox = document.createElement('input');
                    highCheckbox.type = 'checkbox';
                    highCheckbox.setAttribute('data-relay', i);
                    highLabel.appendChild(highCheckbox);
                    highLabel.appendChild(document.createTextNode(` Relay ${i+1}`));
                    highGrid.appendChild(highLabel);
                    
                    // LOW condition checkboxes
                    const lowLabel = document.createElement('label');
                    const lowCheckbox = document.createElement('input');
                    lowCheckbox.type = 'checkbox';
                    lowCheckbox.setAttribute('data-relay', i);
                    lowLabel.appendChild(lowCheckbox);
                    lowLabel.appendChild(document.createTextNode(` Relay ${i+1}`));
                    lowGrid.appendChild(lowLabel);
                }
            }
        }
    }
    
    // Create sensor trigger section if it doesn't exist
    if (!document.getElementById('sensor-trigger-section')) {
        createSensorTriggerSection();
    }
    
    // Create Digital+HT Sensor section if it doesn't exist
    if (!document.getElementById('digital-ht-sensor-section')) {
        createDigitalHTSensorSection();
    }
    
    // Initialize modal scroll fix
    setupModalScrollFix();
}


// Fetch schedules from the server
function fetchSchedules() {
    fetch('/api/schedules')
        .then(response => response.json())
        .then(data => {
            renderSchedulesTable(data.schedules);
        })
        .catch(error => {
            console.error('Error fetching schedules:', error);
            showToast('Failed to load schedules', 'error');
        });
}


// Modify renderSchedulesTable function to include Digital+HT Sensor trigger type
function renderSchedulesTable(schedules) {
    const tableBody = document.querySelector('#schedules-table tbody');
    if (!tableBody) return;
    
    tableBody.innerHTML = '';
    
    if (!schedules || schedules.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = '<td colspan="7" class="text-center">No schedules configured</td>';
        tableBody.appendChild(row);
        return;
    }
    
    schedules.forEach(schedule => {
        const row = document.createElement('tr');
        
        // Format trigger based on type
        let triggerText = '';
        let triggerTypeText = '';
        
        // Determine trigger type
        switch(schedule.triggerType) {
            case 0:
                triggerTypeText = 'Time';
                
                // Format days for time-based trigger
                const days = [];
                if (schedule.days & 1) days.push('Sun');
                if (schedule.days & 2) days.push('Mon');
                if (schedule.days & 4) days.push('Tue');
                if (schedule.days & 8) days.push('Wed');
                if (schedule.days & 16) days.push('Thu');
                if (schedule.days & 32) days.push('Fri');
                if (schedule.days & 64) days.push('Sat');
                
                // Format time
                const hour = schedule.hour.toString().padStart(2, '0');
                const minute = schedule.minute.toString().padStart(2, '0');
                
                triggerText = `${days.join(', ')} at ${hour}:${minute}`;
                break;
                
            case 1:
                triggerTypeText = 'Input';
                triggerText = formatInputTriggerText(schedule.inputMask, schedule.inputStates, schedule.logic);
                break;
                
            case 2:
                triggerTypeText = 'Combined';
                // Format time part
                const combinedDays = [];
                if (schedule.days & 1) combinedDays.push('Sun');
                if (schedule.days & 2) combinedDays.push('Mon');
                if (schedule.days & 4) combinedDays.push('Tue');
                if (schedule.days & 8) combinedDays.push('Wed');
                if (schedule.days & 16) combinedDays.push('Thu');
                if (schedule.days & 32) combinedDays.push('Fri');
                if (schedule.days & 64) combinedDays.push('Sat');
                
                const hourStr = schedule.hour.toString().padStart(2, '0');
                const minuteStr = schedule.minute.toString().padStart(2, '0');
                
                const timePart = `${combinedDays.join(', ')} at ${hourStr}:${minuteStr}`;
                
                // Format input part
                const inputPart = formatInputTriggerText(schedule.inputMask, schedule.inputStates, schedule.logic);
                
                triggerText = `${timePart} AND ${inputPart}`;
                break;
                
            case 3: // Sensor-based
                triggerTypeText = 'Sensor';
                
                // Format sensor trigger
                const sensorNames = ['HT1', 'HT2', 'HT3'];
                const measurementTypes = ['Temperature', 'Humidity'];
                const conditions = ['>', '<', '='];
                
                let sensorName = sensorNames[schedule.sensorIndex] || 'Unknown';
                let measurementType = measurementTypes[schedule.sensorTriggerType] || 'Value';
                let condition = conditions[schedule.sensorCondition] || '?';
                let threshold = schedule.sensorThreshold;
                let unit = schedule.sensorTriggerType === 0 ? '°C' : '%';
                
                triggerText = `${sensorName} ${measurementType} ${condition} ${threshold}${unit}`;
                break;
                
            case 4: // Digital+HT Sensor Combined
                triggerTypeText = 'Digital+HT';
                
                // Format digital input part
                const digitalPart = formatInputTriggerText(schedule.inputMask, schedule.inputStates, schedule.logic);
                
                // Format HT sensor part
                const htSensorNames = ['HT1', 'HT2', 'HT3'];
                const htMeasurementTypes = ['Temperature', 'Humidity'];
                const htConditions = ['>', '<', '='];
                
                let htSensorName = htSensorNames[schedule.sensorIndex] || 'Unknown';
                let htMeasurementType = htMeasurementTypes[schedule.sensorTriggerType] || 'Value';
                let htCondition = htConditions[schedule.sensorCondition] || '?';
                let htThreshold = schedule.sensorThreshold;
                let htUnit = schedule.sensorTriggerType === 0 ? '°C' : '%';
                
                const sensorPart = `${htSensorName} ${htMeasurementType} ${htCondition} ${htThreshold}${htUnit}`;
                
                triggerText = `${digitalPart} AND ${sensorPart}`;
                break;
        }
        
        // Format action
        const actions = ['Turn OFF', 'Turn ON', 'Toggle'];
        
        // Format target based on trigger type
        let targetText = '';
        
        // For input-based or combined triggers, show both HIGH and LOW targets
        if ((schedule.triggerType === 1 || schedule.triggerType === 2 || 
             schedule.triggerType === 3 || schedule.triggerType === 4) && schedule.targetType === 1) {
            // Format HIGH condition targets
            let highTargetText = '';
            if (schedule.targetId > 0) {
                let highRelayCount = 0;
                let highRelayList = [];
                for (let i = 0; i < 16; i++) {
                    if (schedule.targetId & (1 << i)) {
                        highRelayCount++;
                        highRelayList.push(i + 1);
                    }
                }
                highTargetText = `<span class="target-high">HIGH: ${highRelayCount} Relays (${highRelayList.join(', ')})</span>`;
            } else {
                highTargetText = '<span class="target-high">HIGH: None</span>';
            }
            
            // Format LOW condition targets
            let lowTargetText = '';
            if (schedule.targetIdLow > 0) {
                let lowRelayCount = 0;
                let lowRelayList = [];
                for (let i = 0; i < 16; i++) {
                    if (schedule.targetIdLow & (1 << i)) {
                        lowRelayCount++;
                        lowRelayList.push(i + 1);
                    }
                }
                lowTargetText = `<span class="target-low">LOW: ${lowRelayCount} Relays (${lowRelayList.join(', ')})</span>`;
            } else {
                lowTargetText = '<span class="target-low">LOW: None</span>';
            }
            
            targetText = `${highTargetText}<br>${lowTargetText}`;
        } else {
            // For time-based or single target, use the original format
            if (schedule.targetType === 0) {
                targetText = `Relay ${schedule.targetId + 1}`;
            } else {
                // Count the number of relays
                let relayCount = 0;
                let relayList = [];
                for (let i = 0; i < 16; i++) {
                    if (schedule.targetId & (1 << i)) {
                        relayCount++;
                        relayList.push(i + 1);
                    }
                }
                targetText = `${relayCount} Relays (${relayList.join(', ')})`;
            }
        }
        
        row.innerHTML = `
            <td><input type="checkbox" ${schedule.enabled ? 'checked' : ''} onchange="toggleSchedule(${schedule.id}, this.checked)"></td>
            <td>${schedule.name}</td>
            <td><span class="trigger-type trigger-type-${schedule.triggerType}">${triggerTypeText}</span></td>
            <td>${triggerText}</td>
            <td>${actions[schedule.action]}</td>
            <td>${targetText}</td>
            <td>
                <div class="action-btns">
                    <button class="btn btn-secondary btn-sm" onclick="editSchedule(${schedule.id})"><i class="fa fa-edit"></i></button>
                    <button class="btn btn-danger btn-sm" onclick="deleteSchedule(${schedule.id})"><i class="fa fa-trash"></i></button>
                </div>
            </td>
        `;
        
        tableBody.appendChild(row);
    });
    
    // Add styles for HIGH and LOW condition targets
    if (!document.getElementById('condition-target-styles')) {
        const style = document.createElement('style');
        style.id = 'condition-target-styles';
        style.textContent = `
            .target-high {
                display: block;
                padding: 3px;
                margin: 2px 0;
                background-color: #e8f5e9;
                border-left: 3px solid #4CAF50;
            }
            .target-low {
                display: block;
                padding: 3px;
                margin: 2px 0;
                background-color: #ffebee;
                border-left: 3px solid #F44336;
            }
            .trigger-type-4 {
                background-color: #673AB7; /* Purple for Digital+HT */
            }
        `;
        document.head.appendChild(style);
    }
}

// Add a function to immediately evaluate input-based schedules after creation
function evaluateInputBasedSchedules() {
    // Call the server to immediately evaluate input-based schedules
    fetch('/api/evaluate-input-schedules')
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                console.log('Input-based schedules evaluated');
            }
        })
        .catch(error => {
            console.error('Error evaluating input-based schedules:', error);
        });
}

// Helper function to format input trigger text with improved display
function formatInputTriggerText(inputMask, inputStates, logic) {
    if (!inputMask) return 'No inputs selected';
    
    const selectedInputs = [];
    
    // Check digital inputs (bits 0-15)
    for (let i = 0; i < 16; i++) {
        if (inputMask & (1 << i)) {
            const state = (inputStates & (1 << i)) ? 'HIGH' : 'LOW';
            selectedInputs.push(`Input ${i+1} = ${state}`);
        }
    }
    
    // Check HT inputs (bits 16-18)
    for (let i = 0; i < 3; i++) {
        const bitPos = i + 16;
        if (inputMask & (1 << bitPos)) {
            const state = (inputStates & (1 << bitPos)) ? 'HIGH' : 'LOW';
            selectedInputs.push(`HT${i+1} = ${state}`);
        }
    }
    
    if (selectedInputs.length === 0) return 'No inputs selected';
    
    // If more than one input, include the logic operator
    if (selectedInputs.length > 1) {
        const operator = logic === 0 ? 'AND' : 'OR';
        return selectedInputs.join(` ${operator} `);
    } else {
        return selectedInputs[0];
    }
}


// Update the updateVisibleTriggerSections function to include Digital+HT Sensor trigger type
function updateVisibleTriggerSections(triggerType) {
    const timeSection = document.getElementById('time-trigger-section');
    const inputSection = document.getElementById('input-trigger-section');
    const sensorSection = document.getElementById('sensor-trigger-section');
    const digitalHTSensorSection = document.getElementById('digital-ht-sensor-section');
    const inputConditions = document.getElementById('input-condition-sections');
    const singleTarget = document.getElementById('single-target');
    const multipleTargets = document.getElementById('multiple-targets');
    const targetType = document.getElementById('schedule-target-type').value;
    
    if (!timeSection || !inputSection) {
        console.error('Time or input trigger sections not found!');
        return;
    }
    
    triggerType = parseInt(triggerType);
    console.log(`Updating visible sections for trigger type: ${triggerType}`);
    
    // Hide all sections first
    timeSection.style.display = 'none';
    inputSection.style.display = 'none';
    if (sensorSection) sensorSection.style.display = 'none';
    if (digitalHTSensorSection) digitalHTSensorSection.style.display = 'none';
    if (inputConditions) inputConditions.style.display = 'none';
    
    switch (triggerType) {
        case 0: // Time-based
            timeSection.style.display = 'block';
            
            // Show appropriate target section based on target type
            if (targetType === '0') {
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                if (singleTarget) singleTarget.style.display = 'none';
                if (multipleTargets) multipleTargets.style.display = 'block';
                if (inputConditions) inputConditions.style.display = 'none';
            }
            break;
            
        case 1: // Input-based
            inputSection.style.display = 'block';
            
            // Show appropriate target section based on target type
            if (targetType === '0') {
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                if (singleTarget) singleTarget.style.display = 'none';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'block';
            }
            break;
            
        case 2: // Combined (Time + Input)
            timeSection.style.display = 'block';
            inputSection.style.display = 'block';
            
            // Show appropriate target section based on target type
            if (targetType === '0') {
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                if (singleTarget) singleTarget.style.display = 'none';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'block';
            }
            break;
            
        case 3: // Sensor-based
            if (sensorSection) sensorSection.style.display = 'block';
            
            // Show appropriate target section based on target type
            if (targetType === '0') {
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                if (singleTarget) singleTarget.style.display = 'none';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'block';
            }
            break;
            
        case 4: // Digital+HT Sensor
            if (digitalHTSensorSection) {
                digitalHTSensorSection.style.display = 'block';
                
                // Make sure we have checkboxes populated
                createDigitalHTSensorInputCheckboxes();
            }
            
            // Show appropriate target section based on target type
            if (targetType === '0') {
                if (singleTarget) singleTarget.style.display = 'block';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'none';
            } else {
                if (singleTarget) singleTarget.style.display = 'none';
                if (multipleTargets) multipleTargets.style.display = 'none';
                if (inputConditions) inputConditions.style.display = 'block';
            }
            break;
            
        default:
            console.error(`Unknown trigger type: ${triggerType}`);
            timeSection.style.display = 'block';
    }
}


// Function to create the sensor trigger section
function createSensorTriggerSection() {
    const form = document.getElementById('schedule-form');
    if (!form) return;
    
    // Find the input trigger section to insert after
    const inputSection = document.getElementById('input-trigger-section');
    if (!inputSection) return;
    
    // Create the sensor trigger section
    const sensorSection = document.createElement('div');
    sensorSection.id = 'sensor-trigger-section';
    sensorSection.style.display = 'none';
    sensorSection.innerHTML = `
        <div class="form-group">
            <label for="schedule-sensor">Select Sensor</label>
            <select id="schedule-sensor" required>
                <option value="0">HT1</option>
                <option value="1">HT2</option>
                <option value="2">HT3</option>
            </select>
        </div>
        <div class="form-group">
            <label for="schedule-sensor-type">Measurement Type</label>
            <select id="schedule-sensor-type" required>
                <option value="0">Temperature (°C)</option>
                <option value="1">Humidity (%)</option>
            </select>
        </div>
        <div class="form-group">
            <label for="schedule-sensor-condition">Condition</label>
            <select id="schedule-sensor-condition" required>
                <option value="0">Above</option>
                <option value="1">Below</option>
                <option value="2">Equal to (±0.5)</option>
            </select>
        </div>
        <div class="form-group">
            <label for="schedule-sensor-threshold" id="sensor-threshold-label">Threshold (°C)</label>
            <input type="number" id="schedule-sensor-threshold" step="0.1" min="-40" max="125" value="25.0" required>
        </div>
        <div class="sensor-triggers-info">
            <p><strong>Sensor Trigger Settings:</strong></p>
            <ul>
                <li>HT pins must be configured as sensors (DHT11, DHT22, or DS18B20)</li>
                <li>For humidity, only DHT11 and DHT22 sensors provide readings</li>
                <li>Temperature range: -40°C to 125°C</li>
                <li>Humidity range: 0% to 100%</li>
            </ul>
        </div>
    `;
    
    // Add the section after the input trigger section
    inputSection.parentNode.insertBefore(sensorSection, inputSection.nextSibling);
    
    // Add event handler for sensor type change to update the label and range
    const sensorTypeSelect = document.getElementById('schedule-sensor-type');
    const thresholdLabel = document.getElementById('sensor-threshold-label');
    const thresholdInput = document.getElementById('schedule-sensor-threshold');
    
    if (sensorTypeSelect && thresholdLabel && thresholdInput) {
        sensorTypeSelect.addEventListener('change', function() {
            if (this.value === "0") {
                thresholdLabel.textContent = "Threshold (°C)";
                thresholdInput.min = "-40";
                thresholdInput.max = "125";
                thresholdInput.value = "25.0";
            } else {
                thresholdLabel.textContent = "Threshold (%)";
                thresholdInput.min = "0";
                thresholdInput.max = "100";
                thresholdInput.value = "50.0";
            }
        });
    }
}

// Enhanced openScheduleModal function to handle the Digital+HT Sensor section
function openScheduleModal(scheduleId = null) {
    console.log("Opening schedule modal, id:", scheduleId);
    const modal = document.getElementById('schedule-modal');
    if (!modal) {
        console.error("Schedule modal not found!");
        return;
    }
    
    // For mobile: scroll to top of modal when opening
    modal.style.display = 'block';
    
    // Reset scroll position when opening modal
    if (modal.querySelector('.modal-content')) {
        modal.querySelector('.modal-content').scrollTop = 0;
    }
    
    // Reset form
    const scheduleForm = document.getElementById('schedule-form');
    if (scheduleForm) {
        scheduleForm.reset();
    } else {
        console.error("Schedule form not found!");
    }
    
    const scheduleIdField = document.getElementById('schedule-id');
    if (scheduleIdField) {
        scheduleIdField.value = '';
    } else {
        console.error("Schedule ID field not found!");
    }
    
    // Reset day checkboxes
    document.querySelectorAll('.days-selector input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Reset input checkboxes
    document.querySelectorAll('#input-checkboxes input[type="checkbox"], #digital-ht-input-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
        // Reset the state select
        const container = checkbox.closest('.input-container');
        if (container) {
            const stateSelect = container.querySelector('.input-state-select');
            if (stateSelect) {
                stateSelect.value = '0';
                stateSelect.style.display = 'none';
            }
        }
    });
    
    // Reset HT input checkboxes
    document.querySelectorAll('#ht-input-checkboxes input[type="checkbox"], #digital-ht-direct-input-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
        // Reset the state select
        const container = checkbox.closest('.input-container');
        if (container) {
            const stateSelect = container.querySelector('.input-state-select');
            if (stateSelect) {
                stateSelect.value = '0';
                stateSelect.style.display = 'none';
            }
        }
    });
    
    // Reset relay checkboxes for time-based
    document.querySelectorAll('#relay-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Reset relay checkboxes for input-based conditions
    document.querySelectorAll('#relay-checkboxes-high input[type="checkbox"], #relay-checkboxes-low input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Make sure we have all the required sections
    if (!document.getElementById('sensor-trigger-section')) {
        createSensorTriggerSection();
    }
    
    if (!document.getElementById('digital-ht-sensor-section')) {
        createDigitalHTSensorSection();
    }
    
    // Show time-based trigger by default and hide others
    const timeSection = document.getElementById('time-trigger-section');
    const inputSection = document.getElementById('input-trigger-section');
    const sensorSection = document.getElementById('sensor-trigger-section');
    const digitalHTSensorSection = document.getElementById('digital-ht-sensor-section');
    const inputConditions = document.getElementById('input-condition-sections');
    
    if (timeSection) timeSection.style.display = 'block';
    if (inputSection) inputSection.style.display = 'none';
    if (sensorSection) sensorSection.style.display = 'none';
    if (digitalHTSensorSection) digitalHTSensorSection.style.display = 'none';
    if (inputConditions) inputConditions.style.display = 'none';
    
    // Show single target by default
    const singleTarget = document.getElementById('single-target');
    const multipleTargets = document.getElementById('multiple-targets');
    
    if (singleTarget) singleTarget.style.display = 'block';
    if (multipleTargets) multipleTargets.style.display = 'none';
    
    // If editing an existing schedule
    if (scheduleId !== null) {
        if (scheduleIdField) scheduleIdField.value = scheduleId;
        console.log("Loading schedule:", scheduleId);
        
        // Fetch the schedule data
        fetch(`/api/schedules?id=${scheduleId}`)
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok');
                return response.json();
            })
            .then(data => {
                const schedules = data.schedules;
                if (schedules && schedules.length > 0) {
                    // Find the schedule with matching ID
                    const schedule = schedules.find(s => s.id === parseInt(scheduleId));
                    if (schedule) {
                        console.log("Schedule data loaded:", schedule);
                        
                        // Set form values from schedule data
                        document.getElementById('schedule-id').value = schedule.id;
                        document.getElementById('schedule-enabled').checked = schedule.enabled;
                        document.getElementById('schedule-name').value = schedule.name;
                        document.getElementById('schedule-trigger-type').value = schedule.triggerType;
                        document.getElementById('schedule-action').value = schedule.action;
                        document.getElementById('schedule-target-type').value = schedule.targetType;
                        
                        // Update visible sections based on trigger type first
                        updateVisibleTriggerSections(schedule.triggerType);
                        
                        // Set time fields if applicable
                        if (schedule.triggerType === 0 || schedule.triggerType === 2) {
                            // Set day checkboxes
                            for (let i = 0; i < 7; i++) {
                                const dayBit = (1 << i);
                                const checkbox = document.querySelector(`.days-selector input[data-day="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.days & dayBit) !== 0;
                                    console.log(`Day ${i}: bit ${dayBit}, checked: ${(schedule.days & dayBit) !== 0}`);
                                }
                            }
                            
                            // Set time with proper padding
                            const hour = schedule.hour.toString().padStart(2, '0');
                            const minute = schedule.minute.toString().padStart(2, '0');
                            const timeField = document.getElementById('schedule-time');
                            if (timeField) timeField.value = `${hour}:${minute}`;
                        }
                        
                        // Set input fields if applicable
                        if (schedule.triggerType === 1 || schedule.triggerType === 2) {
                            // Set input logic
                            const logicField = document.getElementById('schedule-input-logic');
                            if (logicField) logicField.value = schedule.logic;
                            
                            console.log("Setting input checkboxes, inputMask:", schedule.inputMask);
                            console.log("Input states:", schedule.inputStates);
                            
                            // Set digital input checkboxes
                            for (let i = 0; i < 16; i++) {
                                const bitMask = (1 << i);
                                if (schedule.inputMask & bitMask) {
                                    const checkbox = document.querySelector(`#input-checkboxes input[data-input="${i}"]`);
                                    if (checkbox) {
                                        checkbox.checked = true;
                                        
                                        // Set the state select to visible and set its value
                                        const container = checkbox.closest('.input-container');
                                        if (container) {
                                            const stateSelect = container.querySelector('.input-state-select');
                                            if (stateSelect) {
                                                stateSelect.style.display = 'inline-block';
                                                stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                                console.log(`Input ${i+1} state set to ${stateSelect.value}`);
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Set HT input checkboxes
                            for (let i = 0; i < 3; i++) {
                                const bitPos = i + 16;
                                const bitMask = (1 << bitPos);
                                if (schedule.inputMask & bitMask) {
                                    const checkbox = document.querySelector(`#ht-input-checkboxes input[data-input="${bitPos}"]`);
                                    if (checkbox) {
                                        checkbox.checked = true;
                                        
                                        // Set the state select to visible and set its value
                                        const container = checkbox.closest('.input-container');
                                        if (container) {
                                            const stateSelect = container.querySelector('.input-state-select');
                                            if (stateSelect) {
                                                stateSelect.style.display = 'inline-block';
                                                stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                                console.log(`HT${i+1} state set to ${stateSelect.value}`);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Set sensor fields if applicable
                        if (schedule.triggerType === 3) {
                            document.getElementById('schedule-sensor').value = schedule.sensorIndex;
                            document.getElementById('schedule-sensor-type').value = schedule.sensorTriggerType;
                            document.getElementById('schedule-sensor-condition').value = schedule.sensorCondition;
                            document.getElementById('schedule-sensor-threshold').value = schedule.sensorThreshold;
                            
                            // Update label based on sensor type
                            const thresholdLabel = document.getElementById('sensor-threshold-label');
                            if (thresholdLabel) {
                                thresholdLabel.textContent = schedule.sensorTriggerType === 0 ? 
                                    "Threshold (°C)" : "Threshold (%)";
                            }
                        }
                        
                        // Set Digital+HT Sensor fields if applicable
                        if (schedule.triggerType === 4) {
                            // Set input logic
                            const htLogicField = document.getElementById('digital-ht-input-logic');
                            if (htLogicField) htLogicField.value = schedule.logic;
                            
                            // Set digital input checkboxes
                            for (let i = 0; i < 16; i++) {
                                const bitMask = (1 << i);
                                if (schedule.inputMask & bitMask) {
                                    const checkbox = document.querySelector(`#digital-ht-input-checkboxes input[data-input="${i}"]`);
                                    if (checkbox) {
                                        checkbox.checked = true;
                                        
                                        // Set the state select to visible and set its value
                                        const container = checkbox.closest('.input-container');
                                        if (container) {
                                            const stateSelect = container.querySelector('.input-state-select');
                                            if (stateSelect) {
                                                stateSelect.style.display = 'inline-block';
                                                stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Set HT direct input checkboxes
                            for (let i = 0; i < 3; i++) {
                                const bitPos = i + 16;
                                const bitMask = (1 << bitPos);
                                if (schedule.inputMask & bitMask) {
                                    const checkbox = document.querySelector(`#digital-ht-direct-input-checkboxes input[data-input="${bitPos}"]`);
                                    if (checkbox) {
                                        checkbox.checked = true;
                                        
                                        // Set the state select to visible and set its value
                                        const container = checkbox.closest('.input-container');
                                        if (container) {
                                            const stateSelect = container.querySelector('.input-state-select');
                                            if (stateSelect) {
                                                stateSelect.style.display = 'inline-block';
                                                stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Set HT sensor fields
                            document.getElementById('digital-ht-sensor').value = schedule.sensorIndex;
                            document.getElementById('digital-ht-sensor-type').value = schedule.sensorTriggerType;
                            document.getElementById('digital-ht-sensor-condition').value = schedule.sensorCondition;
                            document.getElementById('digital-ht-sensor-threshold').value = schedule.sensorThreshold;
                            
                            // Update threshold label based on sensor type
                            const thresholdLabel = document.getElementById('digital-ht-threshold-label');
                            if (thresholdLabel) {
                                thresholdLabel.textContent = schedule.sensorTriggerType === 0 ? 
                                    "Threshold (°C)" : "Threshold (%)";
                            }
                        }
                        
                        // Set target based on trigger type and target type
                        if (schedule.triggerType === 0 || schedule.targetType === 0) {
                            // Time-based or single target
                            if (schedule.targetType === 0) {
                                if (singleTarget) singleTarget.style.display = 'block';
                                if (multipleTargets) multipleTargets.style.display = 'none';
                                if (inputConditions) inputConditions.style.display = 'none';
                                
                                const targetIdField = document.getElementById('schedule-target-id');
                                if (targetIdField) targetIdField.value = schedule.targetId;
                            } else {
                                if (singleTarget) singleTarget.style.display = 'none';
                                if (multipleTargets) multipleTargets.style.display = 'block';
                                if (inputConditions) inputConditions.style.display = 'none';
                                
                                // Set relay checkboxes
                                for (let i = 0; i < 16; i++) {
                                    const checkbox = document.querySelector(`#relay-checkboxes input[data-relay="${i}"]`);
                                    if (checkbox) {
                                        checkbox.checked = (schedule.targetId & (1 << i)) !== 0;
                                    }
                                }
                            }
                        } else if ((schedule.triggerType === 1 || schedule.triggerType === 2 || 
                                   schedule.triggerType === 3 || schedule.triggerType === 4) && 
                                   schedule.targetType === 1) {
                            // Input-based, combined, sensor-based, or Digital+HT sensor with multiple targets
                            if (singleTarget) singleTarget.style.display = 'none';
                            if (multipleTargets) multipleTargets.style.display = 'none';
                            if (inputConditions) inputConditions.style.display = 'block';
                            
                            // Set HIGH condition relay checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#relay-checkboxes-high input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.targetId & (1 << i)) !== 0;
                                }
                            }
                            
                            // Set LOW condition relay checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#relay-checkboxes-low input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.targetIdLow & (1 << i)) !== 0;
                                }
                            }
                        }
                    } else {
                        console.error("Schedule with ID", scheduleId, "not found");
                        showToast('Schedule not found', 'error');
                    }
                } else {
                    console.error("No schedules returned from API");
                    showToast('No schedule data found', 'error');
                }
            })
            .catch(error => {
                console.error('Error loading schedule:', error);
                showToast('Failed to load schedule data', 'error');
            });
    }
}


// Improved setupModalScrollFix function to handle all modals properly
function setupModalScrollFix() {
    // For iOS devices - fix modal scrolling issues
    const modalContents = document.querySelectorAll('.modal-content');
    modalContents.forEach(content => {
        content.addEventListener('touchstart', function(e) {
            if (e.target === this) {
                e.preventDefault();
            }
        }, { passive: false });
    });
    
    // Close modal if clicked outside content area
    const modals = document.querySelectorAll('.modal');
    modals.forEach(modal => {
        modal.addEventListener('click', function(e) {
            if (e.target === this) {
                this.style.display = 'none';
            }
        });
    });
}

// Call this function after the DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    // Your existing code...
    
    // Add this line to initialize modal scroll fixes
    setupModalScrollFix();
});

// Modified saveSchedule function to handle Digital+HT Sensor trigger type
function saveSchedule() {
    const scheduleId = document.getElementById('schedule-id').value;
    const isNew = scheduleId === '';
    
    // Get trigger type
    const triggerType = parseInt(document.getElementById('schedule-trigger-type').value);
    
    // Initialize time, input and sensor fields
    let days = 0;
    let hour = 0;
    let minute = 0;
    let inputMask = 0;
    let inputStates = 0;
    let logic = 0;
    let sensorIndex = 0;
    let sensorTriggerType = 0;
    let sensorCondition = 0;
    let sensorThreshold = 25.0;
    
    // Process time-based fields if applicable
    if (triggerType === 0 || triggerType === 2) {
        // Calculate days bitmask - ensure all checkboxes are found
        const dayCheckboxes = document.querySelectorAll('.days-selector input[type="checkbox"]:checked');
        console.log(`Found ${dayCheckboxes.length} checked day checkboxes`);
        
        dayCheckboxes.forEach(checkbox => {
            const day = parseInt(checkbox.getAttribute('data-day'));
            days |= (1 << day);
            console.log(`Adding day ${day}, bit value ${(1 << day)}, new days value: ${days}`);
        });
        
        // Get time
        const timeValue = document.getElementById('schedule-time').value;
        if (timeValue) {
            const timeParts = timeValue.split(':').map(Number);
            if (timeParts.length === 2) {
                hour = timeParts[0];
                minute = timeParts[1];
                console.log(`Set time to ${hour}:${minute}`);
            }
        }
    }
    
    // Process input-based fields if applicable
    if (triggerType === 1 || triggerType === 2) {
        // Get logic (AND/OR)
        logic = parseInt(document.getElementById('schedule-input-logic').value);
        
        // Process digital inputs
        document.querySelectorAll('#input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get desired state - ensure we find the state select properly
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId); // Set to HIGH
                }
            }
        });
        
        // Process HT inputs
        document.querySelectorAll('#ht-input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get desired state - ensure we find the state select properly
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId); // Set to HIGH
                }
            }
        });
    }
    
    // Process sensor-based fields if applicable
    if (triggerType === 3) {
        sensorIndex = parseInt(document.getElementById('schedule-sensor').value);
        sensorTriggerType = parseInt(document.getElementById('schedule-sensor-type').value);
        sensorCondition = parseInt(document.getElementById('schedule-sensor-condition').value);
        sensorThreshold = parseFloat(document.getElementById('schedule-sensor-threshold').value);
    }
    
    // Process Digital+HT Sensor fields if applicable
    if (triggerType === 4) {
        // Get logic (AND/OR)
        logic = parseInt(document.getElementById('digital-ht-input-logic').value);
        
        // Process digital inputs
        document.querySelectorAll('#digital-ht-input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get desired state
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId); // Set to HIGH
                }
            }
        });
        
        // Process HT direct inputs
        document.querySelectorAll('#digital-ht-direct-input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get desired state
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId); // Set to HIGH
                }
            }
        });
        
        // Get HT sensor settings
        sensorIndex = parseInt(document.getElementById('digital-ht-sensor').value);
        sensorTriggerType = parseInt(document.getElementById('digital-ht-sensor-type').value);
        sensorCondition = parseInt(document.getElementById('digital-ht-sensor-condition').value);
        sensorThreshold = parseFloat(document.getElementById('digital-ht-sensor-threshold').value);
    }
    
    // Get target for HIGH state
    const targetType = parseInt(document.getElementById('schedule-target-type').value);
    let targetId = 0;
    let targetIdLow = 0;
    
    // Handle the case for input-based or combined trigger types
    if (triggerType === 1 || triggerType === 2 || triggerType === 3 || triggerType === 4) {
        if (targetType === 0) {
            // Single relay - not used for dual condition mode
            targetId = parseInt(document.getElementById('schedule-target-id').value);
        } else {
            // Get relays for HIGH state
            document.querySelectorAll('#relay-checkboxes-high input[type="checkbox"]:checked').forEach(checkbox => {
                const relay = parseInt(checkbox.getAttribute('data-relay'));
                targetId |= (1 << relay);
            });
            
            // Get relays for LOW state
            document.querySelectorAll('#relay-checkboxes-low input[type="checkbox"]:checked').forEach(checkbox => {
                const relay = parseInt(checkbox.getAttribute('data-relay'));
                targetIdLow |= (1 << relay);
            });
        }
    } else {
        // For time-based triggers, use the original approach
        if (targetType === 0) {
            targetId = parseInt(document.getElementById('schedule-target-id').value);
        } else {
            document.querySelectorAll('#relay-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
                const relay = parseInt(checkbox.getAttribute('data-relay'));
                targetId |= (1 << relay);
            });
        }
    }
    
    // Create schedule object
    const schedule = {
        id: scheduleId ? parseInt(scheduleId) : null,
        enabled: document.getElementById('schedule-enabled').checked,
        name: document.getElementById('schedule-name').value || `Schedule ${new Date().getTime()}`,
        triggerType: triggerType,
        days: days,
        hour: hour,
        minute: minute,
        inputMask: inputMask,
        inputStates: inputStates,
        logic: logic,
        action: parseInt(document.getElementById('schedule-action').value),
        targetType: targetType,
        targetId: targetId,
        targetIdLow: targetIdLow,
        
        // Add sensor-specific fields
        sensorIndex: sensorIndex,
        sensorTriggerType: sensorTriggerType,
        sensorCondition: sensorCondition,
        sensorThreshold: sensorThreshold
    };
    
    console.log("Saving schedule:", schedule);
    
    // Validate time-based schedule has at least one day selected
    if ((triggerType === 0 || triggerType === 2) && days === 0) {
        showToast('Please select at least one day for time-based schedule', 'warning');
        return;
    }
    
    // Validate input-based schedule has at least one input selected
    if ((triggerType === 1 || triggerType === 2) && inputMask === 0) {
        showToast('Please select at least one input for input-based schedule', 'warning');
        return;
    }
    
    // Validate Digital+HT Sensor schedule has at least one digital input selected
    if (triggerType === 4 && inputMask === 0) {
        showToast('Please select at least one digital input for Digital+HT Sensor schedule', 'warning');
        return;
    }
    
    // For input-based or combined, ensure at least one relay is selected for HIGH or LOW
    if ((triggerType === 1 || triggerType === 2 || triggerType === 3 || triggerType === 4) && 
        targetType === 1 && (targetId === 0 && targetIdLow === 0)) {
        showToast('Please select at least one relay for either HIGH or LOW condition', 'warning');
        return;
    }
    
    // Show saving message
    showToast('Saving schedule...', 'info');
    
    // Save to server
    fetch('/api/schedules', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ schedule: schedule })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.status === 'success') {
            // Hide the modal - explicitly get the modal element and hide it
            const modal = document.getElementById('schedule-modal');
            if (modal) {
                modal.style.display = 'none';
            } else {
                console.error("Modal element not found!");
            }
            
            showToast(`Schedule ${isNew ? 'created' : 'updated'} successfully`, 'success');
            
            // Refresh the schedules table
            fetchSchedules();
            
            // For input-based schedules, immediately evaluate them
            if (triggerType === 1 || triggerType === 2 || triggerType === 4) {
                evaluateInputBasedSchedules();
            }
        } else {
            showToast(`Failed to ${isNew ? 'create' : 'update'} schedule: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving schedule:', error);
        showToast(`Network error. Could not ${isNew ? 'create' : 'update'} schedule`, 'error');
    });
}


// Add this function to automatically sync time from browser to device
function autoSyncTimeFromBrowser() {
    const now = new Date();
    
    const timeData = {
        year: now.getFullYear(),
        month: now.getMonth() + 1,
        day: now.getDate(),
        hour: now.getHours(),
        minute: now.getMinutes(),
        second: now.getSeconds()
    };
    
    // Send time to the device
    fetch('/api/time', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(timeData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            console.log('Device time automatically synced with browser time');
        } else {
            console.error('Failed to auto-sync time:', data.message);
        }
    })
    .catch(error => {
        console.error('Error auto-syncing time:', error);
    });
}

// Call this function when the app initializes
document.addEventListener('DOMContentLoaded', function() {
    // Existing init code...
    
    // Auto sync time when the app loads
    setTimeout(autoSyncTimeFromBrowser, 3000); // Small delay to ensure connectivity
    
    // Set up periodic time sync (every 30 minutes)
    setInterval(autoSyncTimeFromBrowser, 30 * 60 * 1000);
});

// Global functions for schedule management
// Enhanced editSchedule function - FIXED VERSION with proper input-based handling
window.editSchedule = function(scheduleId) {
    console.log("Editing schedule:", scheduleId);
    const modal = document.getElementById('schedule-modal');
    if (!modal) {
        console.error("Schedule modal not found!");
        return;
    }
    
    // Open the modal
    modal.style.display = 'block';
    
    // Reset form
    document.getElementById('schedule-form').reset();
    document.getElementById('schedule-id').value = scheduleId;
    
    // Reset day checkboxes
    document.querySelectorAll('.days-selector input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Reset input checkboxes and hide all state selects
    document.querySelectorAll('#input-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
        const container = checkbox.closest('.input-container');
        if (container) {
            const stateSelect = container.querySelector('.input-state-select');
            if (stateSelect) {
                stateSelect.style.display = 'none';
                stateSelect.value = '0';
            }
        }
    });
    
    // Reset HT input checkboxes and hide all state selects
    document.querySelectorAll('#ht-input-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
        const container = checkbox.closest('.input-container');
        if (container) {
            const stateSelect = container.querySelector('.input-state-select');
            if (stateSelect) {
                stateSelect.style.display = 'none';
                stateSelect.value = '0';
            }
        }
    });
    
    // Reset relay checkboxes
    document.querySelectorAll('#relay-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Fetch schedule data
    fetch(`/api/schedules?id=${scheduleId}`)
        .then(response => response.json())
        .then(data => {
            const schedules = data.schedules;
            if (schedules && schedules.length > 0) {
                // Find the schedule with matching ID
                const schedule = schedules.find(s => s.id === parseInt(scheduleId));
                if (schedule) {
                    console.log("Retrieved schedule data:", schedule);
                    
                    // Set form values from schedule data
                    document.getElementById('schedule-name').value = schedule.name;
                    document.getElementById('schedule-enabled').checked = schedule.enabled;
                    document.getElementById('schedule-trigger-type').value = schedule.triggerType;
                    document.getElementById('schedule-action').value = schedule.action;
                    document.getElementById('schedule-target-type').value = schedule.targetType;
                    
                    // First update visible sections based on trigger type
                    updateVisibleTriggerSections(schedule.triggerType);
                    
                    // Set time fields if applicable
                    if (schedule.triggerType === 0 || schedule.triggerType === 2) {
                        // Set day checkboxes
                        for (let i = 0; i < 7; i++) {
                            const checkbox = document.querySelector(`.days-selector input[data-day="${i}"]`);
                            if (checkbox) {
                                checkbox.checked = (schedule.days & (1 << i)) !== 0;
                            }
                        }
                        
                        // Set time
                        const hour = schedule.hour.toString().padStart(2, '0');
                        const minute = schedule.minute.toString().padStart(2, '0');
                        document.getElementById('schedule-time').value = `${hour}:${minute}`;
                    }
                    
                    // Set input fields if applicable
                    if (schedule.triggerType === 1 || schedule.triggerType === 2) {
                        // Set input logic
                        document.getElementById('schedule-input-logic').value = schedule.logic;
                        
                        console.log("Setting input checkboxes, inputMask:", schedule.inputMask);
                        console.log("Input states:", schedule.inputStates);
                        
                        // Set digital input checkboxes (bits 0-15)
                        for (let i = 0; i < 16; i++) {
                            const bitMask = (1 << i);
                            if (schedule.inputMask & bitMask) {
                                const checkbox = document.querySelector(`#input-checkboxes input[data-input="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = true;
                                    
                                    // Get the container and find the state select
                                    const container = checkbox.closest('.input-container');
                                    if (container) {
                                        const stateSelect = container.querySelector('.input-state-select');
                                        if (stateSelect) {
                                            // Show the state select
                                            stateSelect.style.display = 'inline-block';
                                            
                                            // Set its value based on the bit in inputStates
                                            stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                            
                                            console.log(`Input ${i+1} state set to ${stateSelect.value}`);
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Set HT input checkboxes (bits 16-18)
                        for (let i = 0; i < 3; i++) {
                            const bitPos = i + 16;
                            const bitMask = (1 << bitPos);
                            if (schedule.inputMask & bitMask) {
                                const checkbox = document.querySelector(`#ht-input-checkboxes input[data-input="${bitPos}"]`);
                                if (checkbox) {
                                    checkbox.checked = true;
                                    
                                    // Get the container and find the state select
                                    const container = checkbox.closest('.input-container');
                                    if (container) {
                                        const stateSelect = container.querySelector('.input-state-select');
                                        if (stateSelect) {
                                            // Show the state select
                                            stateSelect.style.display = 'inline-block';
                                            
                                            // Set its value based on the bit in inputStates
                                            stateSelect.value = (schedule.inputStates & bitMask) ? '1' : '0';
                                            
                                            console.log(`HT${i+1} state set to ${stateSelect.value}`);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // Set sensor fields if applicable
                    if (schedule.triggerType === 3) {
                        document.getElementById('schedule-sensor').value = schedule.sensorIndex;
                        document.getElementById('schedule-sensor-type').value = schedule.sensorTriggerType;
                        document.getElementById('schedule-sensor-condition').value = schedule.sensorCondition;
                        document.getElementById('schedule-sensor-threshold').value = schedule.sensorThreshold;
                        
                        // Update label based on sensor type
                        const thresholdLabel = document.getElementById('sensor-threshold-label');
                        if (thresholdLabel) {
                            thresholdLabel.textContent = schedule.sensorTriggerType === 0 ? 
                                "Threshold (°C)" : "Threshold (%)";
                        }
                    }
                    
                    // Set target based on trigger type
                    if (schedule.targetType === 0) {
                        // Single target
                        document.getElementById('single-target').style.display = 'block';
                        document.getElementById('multiple-targets').style.display = 'none';
                        document.getElementById('schedule-target-id').value = schedule.targetId;
                    } else {
                        // Multiple targets
                        if (schedule.triggerType === 0) {
                            // Time-based
                            document.getElementById('single-target').style.display = 'none';
                            document.getElementById('multiple-targets').style.display = 'block';
                            
                            // Set relay checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#relay-checkboxes input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.targetId & (1 << i)) !== 0;
                                }
                            }
                        } else {
                            // Input-based, Combined, or Sensor-based
                            document.getElementById('single-target').style.display = 'none';
                            document.getElementById('multiple-targets').style.display = 'none';
                            document.getElementById('input-condition-sections').style.display = 'block';
                            
                            // Set HIGH condition checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#relay-checkboxes-high input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.targetId & (1 << i)) !== 0;
                                }
                            }
                            
                            // Set LOW condition checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#relay-checkboxes-low input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = (schedule.targetIdLow & (1 << i)) !== 0;
                                }
                            }
                        }
                    }
                }
            }
        })
        .catch(error => {
            console.error('Error loading schedule:', error);
            showToast('Failed to load schedule data', 'error');
        });
};

window.deleteSchedule = function(scheduleId) {
    if (confirm('Are you sure you want to delete this schedule?')) {
        fetch(`/api/schedules`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ 
                id: scheduleId,
                delete: true
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showToast('Schedule deleted successfully');
                fetchSchedules();
            } else {
                showToast(`Failed to delete schedule: ${data.message}`, 'error');
            }
        })
        .catch(error => {
            console.error('Error deleting schedule:', error);
            showToast('Network error. Could not delete schedule', 'error');
        });
    }
};

window.toggleSchedule = function(scheduleId, enabled) {
    fetch('/api/schedules', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            id: scheduleId,
            enabled: enabled
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`Schedule ${enabled ? 'enabled' : 'disabled'}`);
        } else {
            showToast(`Failed to update schedule: ${data.message}`, 'error');
            fetchSchedules(); // Refresh to get actual state
        }
    })
    .catch(error => {
        console.error('Error toggling schedule:', error);
        showToast('Network error. Could not update schedule', 'error');
        fetchSchedules(); // Refresh to get actual state
    });
};

// Update to initTriggerUI function to add combined trigger support
function initTriggerUI() {
    // Load triggers
    fetchAnalogTriggers();
    
    // Setup add button
    const addTriggerBtn = document.getElementById('add-trigger');
    if (addTriggerBtn) {
        addTriggerBtn.addEventListener('click', function() {
            openTriggerModal();
        });
    }
    
    // Setup modal close buttons
    document.querySelectorAll('#trigger-modal .close-modal, #trigger-modal .close-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.getElementById('trigger-modal').style.display = 'none';
        });
    });
    
    // Setup form submission
    const triggerForm = document.getElementById('trigger-form');
    if (triggerForm) {
        triggerForm.addEventListener('submit', function(e) {
            e.preventDefault();
            saveTrigger();
        });
    }
    
    // Setup trigger type change
    const triggerTypeSelect = document.getElementById('trigger-type');
    if (triggerTypeSelect) {
        triggerTypeSelect.addEventListener('change', function() {
            updateTriggerTypeVisibility(this.value);
        });
    }
    
    // Setup target type change
    const targetTypeSelect = document.getElementById('trigger-target-type');
    if (targetTypeSelect) {
        targetTypeSelect.addEventListener('change', function() {
            const singleTarget = document.getElementById('trigger-single-target');
            const multipleTargets = document.getElementById('trigger-multiple-targets');
            
            if (this.value === '0') {
                singleTarget.style.display = 'block';
                multipleTargets.style.display = 'none';
            } else {
                singleTarget.style.display = 'none';
                multipleTargets.style.display = 'block';
            }
        });
    }
    
    // Fill relay options
    const targetSelect = document.getElementById('trigger-target-id');
    if (targetSelect) {
        targetSelect.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const option = document.createElement('option');
            option.value = i;
            option.textContent = `Relay ${i+1}`;
            targetSelect.appendChild(option);
        }
    }
    
    // Fill relay checkboxes
    const checkboxGrid = document.getElementById('trigger-relay-checkboxes');
    if (checkboxGrid) {
        checkboxGrid.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-relay', i);
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Relay ${i+1}`));
            checkboxGrid.appendChild(label);
        }
    }
    
    // Setup threshold slider
    const thresholdSlider = document.getElementById('trigger-threshold');
    const thresholdValue = document.getElementById('threshold-value');
    if (thresholdSlider && thresholdValue) {
        thresholdSlider.addEventListener('input', function() {
            thresholdValue.textContent = this.value;
        });
    }
    
    // Create digital input checkboxes if they don't exist
    createDigitalInputCheckboxes();
    
    // Create HT sensor selectors
    createHTSensorTriggerUI();
    
    // Add analog input options to the trigger input select
    const triggerInputSelect = document.getElementById('trigger-input');
    if (triggerInputSelect) {
        // Clear existing options
        triggerInputSelect.innerHTML = '';
        
        // Add standard analog input options
        const analogOptgroup = document.createElement('optgroup');
        analogOptgroup.label = "Analog Inputs";
        
        analogOptgroup.innerHTML = `
            <option value="0">A1</option>
            <option value="1">A2</option>
            <option value="2">A3</option>
            <option value="3">A4</option>
        `;
        
        triggerInputSelect.appendChild(analogOptgroup);
        
        // Add HT sensor options
        const htOptgroup = document.createElement('optgroup');
        htOptgroup.label = "Temperature & Humidity Sensors";
        
        htOptgroup.innerHTML = `
            <option value="100">HT1 Temperature</option>
            <option value="101">HT1 Humidity</option>
            <option value="102">HT2 Temperature</option>
            <option value="103">HT2 Humidity</option>
            <option value="104">HT3 Temperature</option>
            <option value="105">HT3 Humidity</option>
        `;
        
        triggerInputSelect.appendChild(htOptgroup);
        
        // Listen for changes to show appropriate sections
        triggerInputSelect.addEventListener('change', function() {
            const value = parseInt(this.value);
            
            // Show/hide HT sensor specific settings
            const htSensorSettings = document.getElementById('ht-sensor-settings');
            const analogSettings = document.getElementById('analog-threshold-settings');
            
            if (value >= 100) { // HT sensor selected
                if (htSensorSettings) htSensorSettings.style.display = 'block';
                if (analogSettings) analogSettings.style.display = 'none';
                
                // Set appropriate HT sensor index and trigger type
                const htIndex = Math.floor((value - 100) / 2);
                const triggerType = (value - 100) % 2; // 0 = temperature, 1 = humidity
                
                document.getElementById('ht-sensor-index').value = htIndex;
                document.getElementById('ht-sensor-trigger-type').value = triggerType;
                
                // Update threshold label
                updateHTSensorThresholdLabel(triggerType);
            } else {
                if (htSensorSettings) htSensorSettings.style.display = 'none';
                if (analogSettings) analogSettings.style.display = 'block';
            }
        });
    }
    
    // Initialize combined mode checkbox
    const combinedModeCheckbox = document.getElementById('trigger-combined-mode');
    if (combinedModeCheckbox) {
        combinedModeCheckbox.addEventListener('change', function() {
            const combinedSettings = document.getElementById('trigger-combined-settings');
            if (combinedSettings) {
                combinedSettings.style.display = this.checked ? 'block' : 'none';
            }
        });
    }
}

// Create digital input checkboxes for combined triggers
function createDigitalInputCheckboxes() {
    const inputSection = document.getElementById('trigger-input-checkboxes');
    if (!inputSection) return;
    
    inputSection.innerHTML = '';
    
    // Create digital input checkboxes
    for (let i = 0; i < 16; i++) {
        const container = document.createElement('div');
        container.className = 'input-container';
        
        const label = document.createElement('label');
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.setAttribute('data-input', i);
        
        // Create state select that appears when checked
        const stateSelect = document.createElement('select');
        stateSelect.className = 'input-state-select';
        stateSelect.style.display = 'none'; // Initially hidden
        stateSelect.innerHTML = `
            <option value="0">LOW</option>
            <option value="1">HIGH</option>
        `;
        
        // Show/hide state select when checkbox changes
        checkbox.addEventListener('change', function() {
            stateSelect.style.display = this.checked ? 'inline-block' : 'none';
        });
        
        label.appendChild(checkbox);
        label.appendChild(document.createTextNode(` Input ${i+1} `));
        container.appendChild(label);
        container.appendChild(stateSelect);
        
        inputSection.appendChild(container);
    }
    
    // Create HT input checkboxes
    const htInputSection = document.getElementById('trigger-ht-input-checkboxes');
    if (!htInputSection) return;
    
    htInputSection.innerHTML = '';
    
    // Add HT1-HT3 digital inputs
    for (let i = 0; i < 3; i++) {
        const container = document.createElement('div');
        container.className = 'input-container';
        
        const label = document.createElement('label');
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.setAttribute('data-input', i + 16); // HT inputs are at bits 16-18
        
        // Create state select that appears when checked
        const stateSelect = document.createElement('select');
        stateSelect.className = 'input-state-select';
        stateSelect.style.display = 'none'; // Initially hidden
        stateSelect.innerHTML = `
            <option value="0">LOW</option>
            <option value="1">HIGH</option>
        `;
        
        // Show/hide state select when checkbox changes
        checkbox.addEventListener('change', function() {
            stateSelect.style.display = this.checked ? 'inline-block' : 'none';
        });
        
        label.appendChild(checkbox);
        label.appendChild(document.createTextNode(` HT${i+1} `));
        container.appendChild(label);
        container.appendChild(stateSelect);
        
        htInputSection.appendChild(container);
    }
}

// Create HT sensor trigger UI
function createHTSensorTriggerUI() {
    const htSensorSettings = document.getElementById('ht-sensor-settings');
    if (htSensorSettings) return; // Already created
    
    // Find insert point - after analog threshold settings
    const analogSettings = document.getElementById('analog-threshold-settings');
    if (!analogSettings) return;
    
    const htSettings = document.createElement('div');
    htSettings.id = 'ht-sensor-settings';
    htSettings.style.display = 'none'; // Hidden by default
    htSettings.innerHTML = `
        <div class="form-group">
            <label for="ht-sensor-condition">Condition</label>
            <select id="ht-sensor-condition" required>
                <option value="0">Above</option>
                <option value="1">Below</option>
                <option value="2">Equal to (±0.5)</option>
            </select>
        </div>
        <div class="form-group">
            <label for="ht-sensor-threshold" id="ht-threshold-label">Threshold (°C)</label>
            <input type="number" id="ht-sensor-threshold" step="0.1" min="-40" max="125" value="25.0" required>
        </div>
        
        <!-- Hidden fields to store HT sensor data -->
        <input type="hidden" id="ht-sensor-index" value="0">
        <input type="hidden" id="ht-sensor-trigger-type" value="0">
    `;
    
    analogSettings.parentNode.insertBefore(htSettings, analogSettings.nextSibling);
    
    // Add event listener for trigger type changes
    const triggerTypeSelect = document.getElementById('ht-sensor-trigger-type');
    if (triggerTypeSelect) {
        triggerTypeSelect.addEventListener('change', function() {
            updateHTSensorThresholdLabel(parseInt(this.value));
        });
    }
}

// Update threshold label based on sensor measurement type
function updateHTSensorThresholdLabel(triggerType) {
    const thresholdLabel = document.getElementById('ht-threshold-label');
    const thresholdInput = document.getElementById('ht-sensor-threshold');
    
    if (thresholdLabel && thresholdInput) {
        if (triggerType === 0) { // Temperature
            thresholdLabel.textContent = 'Threshold (°C)';
            thresholdInput.min = '-40';
            thresholdInput.max = '125';
            thresholdInput.value = '25.0';
        } else { // Humidity
            thresholdLabel.textContent = 'Threshold (%)';
            thresholdInput.min = '0';
            thresholdInput.max = '100';
            thresholdInput.value = '50.0';
        }
    }
}

// Update trigger type visibility
function updateTriggerTypeVisibility(type) {
    const analogSettings = document.getElementById('analog-threshold-settings');
    const htSettings = document.getElementById('ht-sensor-settings');
    const combinedSettings = document.getElementById('trigger-combined-settings');
    
    // Default all sections to hidden
    if (analogSettings) analogSettings.style.display = 'none';
    if (htSettings) htSettings.style.display = 'none';
    if (combinedSettings) combinedSettings.style.display = 'none';
    
    // Show appropriate sections based on type
    if (type === '0') { // Standard Analog
        if (analogSettings) analogSettings.style.display = 'block';
    }
    else if (type === '1') { // HT Sensor
        if (htSettings) htSettings.style.display = 'block';
    }
    else if (type === '2') { // Combined
        if (analogSettings) analogSettings.style.display = 'block';
        if (combinedSettings) combinedSettings.style.display = 'block';
    }
}

// Initialize input controls
function initInputControls() {
    // Create digital input displays
    const digitalInputsGrid = document.getElementById('digital-inputs-grid');
    if (digitalInputsGrid) {
        digitalInputsGrid.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const inputItem = document.createElement('div');
            inputItem.className = 'input-item';
            inputItem.setAttribute('data-input', i);
            inputItem.innerHTML = `
                Input ${i+1}
                <div class="input-state"></div>
            `;
            digitalInputsGrid.appendChild(inputItem);
        }
    }
    
    // Create direct input displays (HT1-HT3)
    const directInputsGrid = document.getElementById('direct-inputs-grid');
    if (directInputsGrid) {
        directInputsGrid.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            const inputItem = document.createElement('div');
            inputItem.className = 'input-item';
            inputItem.setAttribute('data-input', 'ht' + i);
            inputItem.innerHTML = `
                HT${i+1}
                <div class="input-state"></div>
            `;
            directInputsGrid.appendChild(inputItem);
        }
    }
    
    // Create visual input grid for graphical representation
    createVisualInputGrid();
    
    // Create visual analog grid
    createVisualAnalogGrid();
}

// Fetch analog triggers from the server - FIXED
function fetchAnalogTriggers() {
    fetch('/api/analog-triggers')
        .then(response => response.json())
        .then(data => {
            renderTriggersTable(data.triggers);
        })
        .catch(error => {
            console.error('Error fetching analog triggers:', error);
            showToast('Failed to load analog triggers', 'error');
        });
}

// Modified renderTriggersTable function to show combined trigger info
function renderTriggersTable(triggers) {
    const tableBody = document.querySelector('#triggers-table tbody');
    if (!tableBody) return;
    
    tableBody.innerHTML = '';
    
    if (!triggers || triggers.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = '<td colspan="8" class="text-center">No triggers configured</td>';
        tableBody.appendChild(row);
        return;
    }
    
    triggers.forEach(trigger => {
        const row = document.createElement('tr');
        
        // Format input
        let inputName;
        let conditionText;
        
        if (trigger.analogInput >= 100) { // HT sensor
            // Parse the HT sensor trigger
            const htIndex = Math.floor((trigger.analogInput - 100) / 2);
            const type = (trigger.analogInput - 100) % 2 === 0 ? 'temp' : 'hum';
            inputName = `HT${htIndex + 1} ${type === 'temp' ? 'Temperature' : 'Humidity'}`;
            
            // Format condition based on sensor type
            const conditions = ['Above', 'Below', 'Equal to'];
            const value = trigger.sensorThreshold;
            const unit = type === 'temp' ? '°C' : '%';
            conditionText = `${conditions[trigger.sensorCondition]} ${value}${unit}`;
        } else {
            // Standard analog input
            const inputs = ['A1', 'A2', 'A3', 'A4'];
            inputName = inputs[trigger.analogInput] || 'Unknown';
            
            // Format condition
            const conditions = ['Above', 'Below', 'Equal to'];
            conditionText = `${conditions[trigger.condition]} ${trigger.threshold}`;
            
            // Add combined mode info if enabled
            if (trigger.combinedMode) {
                inputName += ' + Inputs';
                
                // Add info about digital inputs
                const inputCount = countBits(trigger.inputMask);
                if (inputCount > 0) {
                    const logic = trigger.logic === 0 ? 'AND' : 'OR';
                    inputName += ` (${inputCount} inputs, ${logic})`;
                }
            }
        }
        
        // Format action
        const actions = ['Turn OFF', 'Turn ON', 'Toggle'];
        
        // Format target
        let targetText = '';
        if (trigger.targetType === 0) {
            targetText = `Relay ${trigger.targetId + 1}`;
        } else {
            // Count the number of relays
            let relayCount = 0;
            let relayList = [];
            for (let i = 0; i < 16; i++) {
                if (trigger.targetId & (1 << i)) {
                    relayCount++;
                    relayList.push(i + 1);
                }
            }
            targetText = `${relayCount} Relays (${relayList.join(', ')})`;
        }
        
        row.innerHTML = `
            <td><input type="checkbox" ${trigger.enabled ? 'checked' : ''} onchange="toggleTrigger(${trigger.id}, this.checked)"></td>
            <td>${trigger.name}</td>
            <td>${inputName}</td>
            <td>${conditionText}</td>
            <td>${trigger.threshold}</td>
            <td>${actions[trigger.action]}</td>
            <td>${targetText}</td>
            <td>
                <div class="action-btns">
                    <button class="btn btn-secondary btn-sm" onclick="editTrigger(${trigger.id})"><i class="fa fa-edit"></i></button>
                    <button class="btn btn-danger btn-sm" onclick="deleteTrigger(${trigger.id})"><i class="fa fa-trash"></i></button>
                </div>
            </td>
        `;
        
        tableBody.appendChild(row);
    });
}

// Helper function to count set bits
function countBits(n) {
    let count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

// Modified openTriggerModal function to support combined triggers
function openTriggerModal(triggerId = null) {
    const modal = document.getElementById('trigger-modal');
    if (!modal) return;
    
    modal.style.display = 'block';
    
    // Reset form
    document.getElementById('trigger-form').reset();
    document.getElementById('trigger-id').value = '';
    document.getElementById('threshold-value').textContent = '2048';
    
    // Reset multiple targets
    document.querySelectorAll('#trigger-relay-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
    });
    
    // Reset combined mode checkboxes
    document.querySelectorAll('#trigger-input-checkboxes input[type="checkbox"], #trigger-ht-input-checkboxes input[type="checkbox"]').forEach(checkbox => {
        checkbox.checked = false;
        // Reset state selects
        const container = checkbox.closest('.input-container');
        if (container) {
            const stateSelect = container.querySelector('.input-state-select');
            if (stateSelect) {
                stateSelect.style.display = 'none';
                stateSelect.value = '0';
            }
        }
    });
    
    // Show single target by default
    document.getElementById('trigger-single-target').style.display = 'block';
    document.getElementById('trigger-multiple-targets').style.display = 'none';
    
    // Reset trigger type visibility
    updateTriggerTypeVisibility('0'); // Default to standard analog
    
    if (triggerId !== null) {
        // Load trigger data
        fetch(`/api/analog-triggers?id=${triggerId}`)
            .then(response => response.json())
            .then(data => {
                if (data.triggers && data.triggers.length > 0) {
                    const trigger = data.triggers.find(t => t.id === parseInt(triggerId));
                    if (trigger) {
                        document.getElementById('trigger-id').value = trigger.id;
                        document.getElementById('trigger-enabled').checked = trigger.enabled;
                        document.getElementById('trigger-name').value = trigger.name;
                        
                        // Determine trigger type
                        let triggerType = '0'; // Default to standard analog
                        
                        if (trigger.analogInput >= 100) {
                            triggerType = '1'; // HT Sensor
                            
                            // Set HT sensor details
                            document.getElementById('ht-sensor-index').value = trigger.htSensorIndex || 0;
                            document.getElementById('ht-sensor-trigger-type').value = trigger.sensorTriggerType || 0;
                            document.getElementById('ht-sensor-condition').value = trigger.sensorCondition || 0;
                            document.getElementById('ht-sensor-threshold').value = trigger.sensorThreshold || 25.0;
                            
                            // Update label
                            updateHTSensorThresholdLabel(trigger.sensorTriggerType || 0);
                        } else {
                            // Standard analog or combined
                            document.getElementById('trigger-input').value = trigger.analogInput;
                            document.getElementById('trigger-condition').value = trigger.condition;
                            document.getElementById('trigger-threshold').value = trigger.threshold;
                            document.getElementById('threshold-value').textContent = trigger.threshold;
                            
                            if (trigger.combinedMode) {
                                triggerType = '2'; // Combined
                                document.getElementById('trigger-combined-mode').checked = true;
                                document.getElementById('trigger-input-logic').value = trigger.logic || 0;
                                
                                // Set digital input checkboxes
                                for (let i = 0; i < 16; i++) {
                                    const bitMask = (1 << i);
                                    if (trigger.inputMask & bitMask) {
                                        const checkbox = document.querySelector(`#trigger-input-checkboxes input[data-input="${i}"]`);
                                        if (checkbox) {
                                            checkbox.checked = true;
                                            
                                            // Set state select
                                            const container = checkbox.closest('.input-container');
                                            if (container) {
                                                const stateSelect = container.querySelector('.input-state-select');
                                                if (stateSelect) {
                                                    stateSelect.style.display = 'inline-block';
                                                    stateSelect.value = (trigger.inputStates & bitMask) ? '1' : '0';
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                // Set HT input checkboxes
                                for (let i = 0; i < 3; i++) {
                                    const bitPos = i + 16;
                                    const bitMask = (1 << bitPos);
                                    if (trigger.inputMask & bitMask) {
                                        const checkbox = document.querySelector(`#trigger-ht-input-checkboxes input[data-input="${bitPos}"]`);
                                        if (checkbox) {
                                            checkbox.checked = true;
                                            
                                            // Set state select
                                            const container = checkbox.closest('.input-container');
                                            if (container) {
                                                const stateSelect = container.querySelector('.input-state-select');
                                                if (stateSelect) {
                                                    stateSelect.style.display = 'inline-block';
                                                    stateSelect.value = (trigger.inputStates & bitMask) ? '1' : '0';
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Set trigger type and update visibility
                        document.getElementById('trigger-type').value = triggerType;
                        updateTriggerTypeVisibility(triggerType);
                        
                        // Set action and target type
                        document.getElementById('trigger-action').value = trigger.action;
                        document.getElementById('trigger-target-type').value = trigger.targetType;
                        
                        if (trigger.targetType === 0) {
                            // Single target
                            document.getElementById('trigger-single-target').style.display = 'block';
                            document.getElementById('trigger-multiple-targets').style.display = 'none';
                            document.getElementById('trigger-target-id').value = trigger.targetId;
                        } else {
                            // Multiple targets
                            document.getElementById('trigger-single-target').style.display = 'none';
                            document.getElementById('trigger-multiple-targets').style.display = 'block';
                            
                            // Set checkboxes
                            for (let i = 0; i < 16; i++) {
                                const checkbox = document.querySelector(`#trigger-relay-checkboxes input[data-relay="${i}"]`);
                                if (checkbox) {
                                    checkbox.checked = !!(trigger.targetId & (1 << i));
                                }
                            }
                        }
                    }
                }
            })
            .catch(error => {
                console.error('Error loading trigger:', error);
                showToast('Failed to load trigger details', 'error');
            });
    }
}



// Modified saveTrigger function to handle combined triggers
function saveTrigger() {
    const triggerId = document.getElementById('trigger-id').value;
    const isNew = !triggerId;
    
    // Get trigger type
    const triggerType = document.getElementById('trigger-type').value;
    
    // Get target
    const targetType = parseInt(document.getElementById('trigger-target-type').value);
    let targetId = 0;
    
    if (targetType === 0) {
        // Single relay
        targetId = parseInt(document.getElementById('trigger-target-id').value);
    } else {
        // Multiple relays - calculate bitmask
        document.querySelectorAll('#trigger-relay-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const relay = parseInt(checkbox.getAttribute('data-relay'));
            targetId |= (1 << relay);
        });
    }
    
    // Create base trigger object
    const trigger = {
        id: triggerId ? parseInt(triggerId) : null,
        enabled: document.getElementById('trigger-enabled').checked,
        name: document.getElementById('trigger-name').value || `Trigger ${Date.now()}`,
        action: parseInt(document.getElementById('trigger-action').value),
        targetType: targetType,
        targetId: targetId
    };
    
    // Add specific fields based on trigger type
    if (triggerType === '0') { // Standard analog
        trigger.analogInput = parseInt(document.getElementById('trigger-input').value);
        trigger.threshold = parseInt(document.getElementById('trigger-threshold').value);
        trigger.condition = parseInt(document.getElementById('trigger-condition').value);
        trigger.combinedMode = false;
    }
    else if (triggerType === '1') { // HT Sensor based
        // Store sensor type in analogInput field (100 = HT1 temp, 101 = HT1 humidity, etc.)
        const htIndex = parseInt(document.getElementById('ht-sensor-index').value);
        const triggerMeasurement = parseInt(document.getElementById('ht-sensor-trigger-type').value);
        trigger.analogInput = 100 + (htIndex * 2) + triggerMeasurement;
        
        // Add sensor specific fields
        trigger.htSensorIndex = htIndex;
        trigger.sensorTriggerType = triggerMeasurement;
        trigger.sensorCondition = parseInt(document.getElementById('ht-sensor-condition').value);
        trigger.sensorThreshold = parseFloat(document.getElementById('ht-sensor-threshold').value);
        trigger.combinedMode = false;
    }
    else if (triggerType === '2') { // Combined
        trigger.analogInput = parseInt(document.getElementById('trigger-input').value);
        trigger.threshold = parseInt(document.getElementById('trigger-threshold').value);
        trigger.condition = parseInt(document.getElementById('trigger-condition').value);
        trigger.combinedMode = true;
        trigger.logic = parseInt(document.getElementById('trigger-input-logic').value);
        
        // Calculate input masks
        let inputMask = 0;
        let inputStates = 0;
        
        // Process digital inputs
        document.querySelectorAll('#trigger-input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get state from select
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId);
                }
            }
        });
        
        // Process HT inputs
        document.querySelectorAll('#trigger-ht-input-checkboxes input[type="checkbox"]:checked').forEach(checkbox => {
            const inputId = parseInt(checkbox.getAttribute('data-input'));
            inputMask |= (1 << inputId);
            
            // Get state from select
            const container = checkbox.closest('.input-container');
            if (container) {
                const stateSelect = container.querySelector('.input-state-select');
                if (stateSelect && stateSelect.value === '1') {
                    inputStates |= (1 << inputId);
                }
            }
        });
        
        trigger.inputMask = inputMask;
        trigger.inputStates = inputStates;
    }
    
    console.log("Saving analog trigger:", trigger);
    
    // Save to server
    fetch('/api/analog-triggers', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ trigger: trigger })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            document.getElementById('trigger-modal').style.display = 'none';
            showToast(`Trigger ${isNew ? 'created' : 'updated'} successfully`);
            
            // Refresh the triggers table
            fetchAnalogTriggers();
        } else {
            showToast(`Failed to ${isNew ? 'create' : 'update'} trigger: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving trigger:', error);
        showToast(`Network error. Could not ${isNew ? 'create' : 'update'} trigger`, 'error');
    });
}


// Global functions for trigger management
window.editTrigger = function(triggerId) {
    openTriggerModal(triggerId);
};

window.deleteTrigger = function(triggerId) {
    if (confirm('Are you sure you want to delete this trigger?')) {
        fetch(`/api/analog-triggers`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ 
                id: triggerId,
                delete: true
            })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showToast('Trigger deleted successfully');
                fetchAnalogTriggers();
            } else {
                showToast(`Failed to delete trigger: ${data.message}`, 'error');
            }
        })
        .catch(error => {
            console.error('Error deleting trigger:', error);
            showToast('Network error. Could not delete trigger', 'error');
        });
    }
};

window.toggleTrigger = function(triggerId, enabled) {
    fetch('/api/analog-triggers', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            id: triggerId,
            enabled: enabled
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`Trigger ${enabled ? 'enabled' : 'disabled'}`);
        } else {
            showToast(`Failed to update trigger: ${data.message}`, 'error');
            fetchAnalogTriggers(); // Refresh to get actual state
        }
    })
    .catch(error => {
        console.error('Error toggling trigger:', error);
        showToast('Network error. Could not update trigger', 'error');
        fetchAnalogTriggers(); // Refresh to get actual state
    });
};

// Initialize communication UI
function initCommunicationUI() {
    // Setup protocol selection form
    const protocolForm = document.getElementById('comm-protocol-form');
    if (protocolForm) {
        protocolForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            // Get selected protocol
            const selectedProtocol = document.querySelector('input[name="protocol"]:checked');
            if (!selectedProtocol) {
                showToast('Please select a communication protocol', 'warning');
                return;
            }
            
            // Save protocol setting
            saveProtocolSetting(selectedProtocol.value);
        });
        
        // Setup protocol selection change event
        document.querySelectorAll('input[name="protocol"]').forEach(radio => {
            radio.addEventListener('change', function() {
                loadProtocolSettings();
            });
        });
    }
    
    // Setup check communication button
    const checkCommBtn = document.getElementById('check-communication');
    if (checkCommBtn) {
        checkCommBtn.addEventListener('click', checkCommunicationStatus);
    }
    
    // Setup scan I2C button
    const scanI2CBtn = document.getElementById('scan-i2c');
    if (scanI2CBtn) {
        scanI2CBtn.addEventListener('click', scanI2CBus);
    }
    
    // Load current protocol setting
    fetchCommunicationStatus();
    
    // Create container for protocol-specific settings if it doesn't exist
    if (!document.getElementById('protocol-config-container')) {
        const commGrid = document.querySelector('.comm-grid');
        if (commGrid) {
            const configContainer = document.createElement('div');
            configContainer.className = 'comm-card protocol-config';
            configContainer.id = 'protocol-config-container';
            configContainer.innerHTML = '<h3>Protocol Configuration</h3><p>Select a protocol to see its settings</p>';
            commGrid.appendChild(configContainer);
        }
    }
}

// Save communication protocol setting
function saveProtocolSetting(protocol) {
    fetch('/api/communication', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            protocol: protocol
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`Communication protocol set to ${protocol.toUpperCase()}`, 'success');
            
            // Update status display
            document.getElementById('protocol-status').textContent = protocol.toUpperCase();
            document.getElementById('active-comm-protocol').textContent = protocol.toUpperCase();
            document.getElementById('active-protocol').textContent = protocol.toUpperCase();
            
            // Load protocol-specific settings
            loadProtocolSettings();
        } else {
            showToast(`Failed to set protocol: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error setting protocol:', error);
        showToast('Network error. Could not set protocol', 'error');
    });
}

// Check communication status
function checkCommunicationStatus() {
    fetch('/api/communication')
        .then(response => response.json())
        .then(data => {
            document.getElementById('usb-status').textContent = data.usb_available ? 'Connected' : 'Disconnected';
            document.getElementById('wifi-comm-status').textContent = data.wifi_connected ? 'Connected' : 'Disconnected';
            document.getElementById('eth-comm-status').textContent = data.eth_connected ? 'Connected' : 'Disconnected';
            document.getElementById('rs485-status').textContent = data.rs485_available ? 'Available' : 'Unavailable';
            document.getElementById('i2c-comm-status').textContent = (data.i2c_error_count === 0) ? 'OK' : 'Issues detected';
            document.getElementById('active-comm-protocol').textContent = data.active_protocol.toUpperCase();
            
            // Set the radio button for the active protocol
            const radioBtn = document.querySelector(`input[name="protocol"][value="${data.active_protocol}"]`);
            if (radioBtn) radioBtn.checked = true;
            
            // Load protocol-specific settings for the active protocol
            loadProtocolSettings();
            
            showToast('Communication status updated', 'success');
        })
        .catch(error => {
            console.error('Error checking communication status:', error);
            showToast('Failed to check communication status', 'error');
        });
}

// Scan I2C bus
function scanI2CBus() {
    const i2cDevices = document.getElementById('i2c-devices');
    if (!i2cDevices) return;
    
    i2cDevices.innerHTML = '<p>Scanning I2C bus...</p>';
    
    fetch('/api/i2c/scan')
        .then(response => response.json())
        .then(data => {
            if (data.devices && data.devices.length > 0) {
                i2cDevices.innerHTML = `<p>Found ${data.total_devices} device(s):</p>`;
                
                const deviceList = document.createElement('ul');
                deviceList.className = 'i2c-device-list';
                
                data.devices.forEach(device => {
                    const deviceItem = document.createElement('li');
                    deviceItem.className = 'i2c-device-item';
                    deviceItem.textContent = `${device.address} - ${device.name}`;
                    deviceList.appendChild(deviceItem);
                });
                
                i2cDevices.appendChild(deviceList);
            } else {
                i2cDevices.innerHTML = '<p>No I2C devices found</p>';
            }
        })
        .catch(error => {
            console.error('Error scanning I2C bus:', error);
            i2cDevices.innerHTML = '<p>Error scanning I2C bus</p>';
            showToast('Failed to scan I2C bus', 'error');
        });
}

// Fetch communication status
function fetchCommunicationStatus() {
    fetch('/api/communication')
        .then(response => response.json())
        .then(data => {
            document.getElementById('protocol-status').textContent = data.active_protocol.toUpperCase();
            document.getElementById('active-protocol').textContent = data.active_protocol.toUpperCase();
            
            // Set the radio button for the active protocol
            const radioBtn = document.querySelector(`input[name="protocol"][value="${data.active_protocol}"]`);
            if (radioBtn) radioBtn.checked = true;
            
            // Load protocol-specific settings
            loadProtocolSettings();
        })
        .catch(error => {
            console.error('Error fetching communication status:', error);
        });
}

// Initialize settings UI
function initSettingsUI() {
    // Load settings
    fetchSettings();
    
    // Setup DHCP toggle
    const dhcpToggle = document.getElementById('dhcp-mode');
    if (dhcpToggle) {
        dhcpToggle.addEventListener('change', function() {
            const staticIpSettings = document.getElementById('static-ip-settings');
            if (staticIpSettings) {
                staticIpSettings.style.display = this.checked ? 'none' : 'block';
            }
        });
    }
    
    // Setup form submission
    const settingsForm = document.getElementById('settings-form');
    if (settingsForm) {
        settingsForm.addEventListener('submit', function(e) {
            e.preventDefault();
            saveSettings();
        });
    }
    
    // Setup reset button
    const resetSettingsBtn = document.getElementById('reset-settings');
    if (resetSettingsBtn) {
        resetSettingsBtn.addEventListener('click', function() {
            if (confirm('Are you sure you want to reset all settings to default?')) {
                resetSettings();
            }
        });
    }
    
    // Setup time setting buttons
    const setBrowserTimeBtn = document.getElementById('set-browser-time');
    if (setBrowserTimeBtn) {
        setBrowserTimeBtn.addEventListener('click', function(e) {
            e.preventDefault();
            setTimeFromBrowser();
        });
    }
    
    const setManualTimeBtn = document.getElementById('set-manual-time');
    if (setManualTimeBtn) {
        setManualTimeBtn.addEventListener('click', function(e) {
            e.preventDefault();
            setTimeManually();
        });
    }
    
    const syncNtpBtn = document.getElementById('sync-ntp');
    if (syncNtpBtn) {
        syncNtpBtn.addEventListener('click', function(e) {
            e.preventDefault();
            syncNTPTime();
        });
    }
    
    // Fetch device time
    fetchDeviceTime();
}

// Initialize diagnostics UI
function initDiagnosticsUI() {
    // Setup run diagnostics button
    const runDiagnosticsBtn = document.getElementById('run-diagnostics');
    if (runDiagnosticsBtn) {
        runDiagnosticsBtn.addEventListener('click', runDiagnostics);
    }
    
    // Setup send command button
    const sendCommandBtn = document.getElementById('send-command');
    if (sendCommandBtn) {
        sendCommandBtn.addEventListener('click', sendCommand);
    }
    
    // Setup command input field with enter key press
    const commandInput = document.getElementById('command-input');
    if (commandInput) {
        commandInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendCommand();
            }
        });
    }
    
    // Setup firmware update form
    const firmwareForm = document.getElementById('firmware-form');
    if (firmwareForm) {
        firmwareForm.addEventListener('submit', function(e) {
            e.preventDefault();
            updateFirmware();
        });
    }
}

// Initialize debug console
function initDebugConsole() {
    debugConsole = document.getElementById('debug-output');
    if (debugConsole) {
        writeToConsole('Debug console initialized');
        writeToConsole('Type HELP for available commands');
    }
}

// Write to debug console
function writeToConsole(message) {
    if (debugConsole) {
        const timestamp = new Date().toLocaleTimeString();
        debugConsole.innerHTML += `<div>[${timestamp}] ${message}</div>`;
        debugConsole.scrollTop = debugConsole.scrollHeight;
    }
}

// Send command to device
function sendCommand() {
    const commandInput = document.getElementById('command-input');
    if (!commandInput) return;
    
    const command = commandInput.value.trim();
    
    if (command) {
        writeToConsole(`> ${command}`);
        
        fetch('/api/debug', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ command: command })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                writeToConsole(data.response);
            } else {
                writeToConsole(`Error: ${data.message}`);
            }
        })
        .catch(error => {
            writeToConsole(`Error: ${error.message}`);
        });
        
        commandInput.value = '';
    }
}

// Run system diagnostics
function runDiagnostics() {
    writeToConsole('Running system diagnostics...');
    
    fetch('/api/debug')
        .then(response => response.json())
        .then(data => {
            document.getElementById('i2c-status').textContent = data.i2c_errors > 0 ? 'Issues detected' : 'OK';
            document.getElementById('wifi-signal').textContent = systemData.wifi_rssi ? `${systemData.wifi_rssi} dBm` : 'Not connected';
            document.getElementById('internet-status').textContent = data.internet_connected ? 'Connected' : 'Not connected';
            
            // Update system information
            document.getElementById('cpu-freq').textContent = data.cpu_freq;
            document.getElementById('free-heap').textContent = data.free_heap;
            document.getElementById('i2c-errors').textContent = data.i2c_errors;
            document.getElementById('last-error').textContent = data.last_error || 'None';
            
            // Update firmware version if available
            if (data.firmware_version) {
                document.getElementById('firmware-version').textContent = data.firmware_version;
                document.getElementById('firmware-version-display').textContent = data.firmware_version;
            }
            
            writeToConsole('Diagnostics complete');
            writeToConsole(`I2C Errors: ${data.i2c_errors}`);
            writeToConsole(`Free Heap: ${data.free_heap} bytes`);
            writeToConsole(`CPU Frequency: ${data.cpu_freq} MHz`);
            
            if (data.last_error) {
                writeToConsole(`Last Error: ${data.last_error}`);
            }
        })
        .catch(error => {
            writeToConsole(`Diagnostics error: ${error.message}`);
        });
}

// Initialize analog chart
function initAnalogChart() {
    const ctx = document.getElementById('analog-chart');
    if (!ctx) return;
    
    // Create empty datasets for 4 analog inputs
    analogHistory = {
        labels: Array(30).fill(''),
        datasets: [
            {
                label: 'A1',
                data: Array(30).fill(null),
                borderColor: 'rgb(255, 99, 132)',
                tension: 0.1,
                fill: false
            },
            {
                label: 'A2',
                data: Array(30).fill(null),
                borderColor: 'rgb(54, 162, 235)',
                tension: 0.1,
                fill: false
            },
            {
                label: 'A3',
                data: Array(30).fill(null),
                borderColor: 'rgb(255, 206, 86)',
                tension: 0.1,
                fill: false
            },
            {
                label: 'A4',
                data: Array(30).fill(null),
                borderColor: 'rgb(75, 192, 192)',
                tension: 0.1,
                fill: false
            }
        ]
    };
    
    analogChart = new Chart(ctx, {
        type: 'line',
        data: analogHistory,
        options: {
            scales: {
                y: {
                    beginAtZero: true,
                    max: 4095
                }
            },
            animation: {
                duration: 0
            },
            responsive: true,
            maintainAspectRatio: false
        }
    });
}

// Update chart with new analog data
function updateAnalogChart() {
    if (!analogChart || !systemData.analog) return;
    
    // Get current time for label
    const now = new Date();
    const timeLabel = now.getHours().toString().padStart(2, '0') + ':' + 
                     now.getMinutes().toString().padStart(2, '0') + ':' + 
                     now.getSeconds().toString().padStart(2, '0');
    
    // Add new data point
    analogHistory.labels.push(timeLabel);
    analogHistory.labels.shift();
    
    // Update each analog input
    systemData.analog.forEach((input, index) => {
        analogHistory.datasets[index].data.push(input.value);
        analogHistory.datasets[index].data.shift();
    });
    
    // Update chart
    analogChart.update();
    
    // Update analog bar visualization
    updateAnalogVisualizations(systemData.analog);
}

// Update analog visualizations with improved smoothness
function updateAnalogVisualizations(analogData) {
    if (!analogData) return;
    
    analogData.forEach((input, index) => {
        const percentage = input.percentage || 0;
        
        // Update bar with smooth animation
        const bar = document.getElementById(`analog-bar-${index}`);
        if (bar) {
            bar.style.transition = 'height 0.2s ease-out';
            bar.style.height = `${percentage}%`;
        }
        
        // Update value
        const valueElem = document.getElementById(`analog-bar-value-${index}`);
        if (valueElem) {
            valueElem.textContent = input.value;
        }
        
        // Update label
        const labelElem = document.getElementById(`analog-bar-label-${index}`);
        if (labelElem) {
            labelElem.textContent = `${percentage}%`;
        }
    });
}

// Update relay state visually
function updateRelayState(id, state) {
    // Update toggle
    const toggleSwitch = document.querySelector(`.relay-toggle[data-relay="${id}"]`);
    if (toggleSwitch) {
        toggleSwitch.checked = state;
    }
    
    // Update status indicator
    const statusIndicator = document.getElementById(`relay-status-${id}`);
    if (statusIndicator) {
        statusIndicator.className = state ? 'status-indicator on' : 'status-indicator';
    }
    
    // Update visual box
    const visualBox = document.getElementById(`output-visual-${id}`);
    if (visualBox) {
        visualBox.className = state ? 'visual-box on' : 'visual-box off';
    }
}

// Improved input state visualization
function updateInputState(id, state) {
    // Update status indicator in list
    const inputItems = document.querySelectorAll(`.input-item[data-input="${id}"] .input-state`);
    
    if (inputItems.length === 0) {
        // If input items haven't been created yet, create them
        const digitalInputsGrid = document.getElementById('digital-inputs-grid');
        if (digitalInputsGrid && !digitalInputsGrid.querySelector(`.input-item[data-input="${id}"]`)) {
            const inputItem = document.createElement('div');
            inputItem.className = 'input-item';
            inputItem.setAttribute('data-input', id);
            inputItem.innerHTML = `
                Input ${id + 1}
                <div class="input-state ${state ? 'active' : ''}"></div>
            `;
            digitalInputsGrid.appendChild(inputItem);
        }
    } else {
        // Update existing items
        inputItems.forEach(item => {
            item.className = state ? 'input-state active' : 'input-state';
        });
    }
    
    // Update visual box if it exists
    const visualBox = document.getElementById(`input-visual-${id}`);
    if (visualBox) {
        visualBox.className = state ? 'visual-box on' : 'visual-box off';
        visualBox.setAttribute('title', state ? 'ON' : 'OFF');
    } else {
        // If visual box doesn't exist, create it
        const visualGrid = document.querySelector('.input-visual-grid');
        if (visualGrid && !visualGrid.querySelector(`#input-visual-${id}`)) {
            createVisualInputGrid(); // Recreate the entire grid
        }
    }
}

// Update direct input state with improved visualization
function updateDirectInputState(id, state) {
    // Update status indicator in list
    const inputItems = document.querySelectorAll(`.input-item[data-input="ht${id}"] .input-state`);
    
    if (inputItems.length === 0) {
        // If direct input items haven't been created yet, create them
        const directInputsGrid = document.getElementById('direct-inputs-grid');
        if (directInputsGrid && !directInputsGrid.querySelector(`.input-item[data-input="ht${id}"]`)) {
            const inputItem = document.createElement('div');
            inputItem.className = 'input-item';
            inputItem.setAttribute('data-input', 'ht' + id);
            inputItem.innerHTML = `
                HT${id + 1}
                <div class="input-state ${state ? 'active' : ''}"></div>
            `;
            directInputsGrid.appendChild(inputItem);
        }
    } else {
        // Update existing items
        inputItems.forEach(item => {
            item.className = state ? 'input-state active' : 'input-state';
        });
    }
    
    // Update visual box if it exists
    const visualBox = document.getElementById(`direct-visual-${id}`);
    if (visualBox) {
        visualBox.className = state ? 'visual-box on' : 'visual-box off';
        visualBox.setAttribute('title', state ? 'ON' : 'OFF');
    } else {
        // If visual box doesn't exist, create it
        const visualGrid = document.querySelector('.input-visual-grid');
        if (visualGrid && !visualGrid.querySelector(`#direct-visual-${id}`)) {
            createVisualInputGrid(); // Recreate the entire grid
        }
    }
}

// Update analog value with improved visual feedback
function updateAnalogValue(id, value) {
    // Get voltage and percentage from system data
    const voltage = systemData.analog && systemData.analog[id] ? 
                   systemData.analog[id].voltage : 0;
    const percentage = systemData.analog && systemData.analog[id] ? 
                      systemData.analog[id].percentage : 0;
    
    // Update fill bar with smooth animation
    const fillBar = document.querySelector(`.analog-card[data-input="${id}"] .analog-fill`);
    if (fillBar) {
        fillBar.style.width = `${percentage}%`;
        fillBar.style.transition = 'width 0.2s ease-out'; // Smoother transition
    }
    
    // Update value display - show both raw value and voltage
    const valueDisplay = document.querySelector(`.analog-card[data-input="${id}"] .analog-value`);
    if (valueDisplay) {
        valueDisplay.textContent = `${value} (${voltage.toFixed(2)}V)`;
    }
    
    // Update percentage display
    const percentageDisplay = document.querySelector(`.analog-card[data-input="${id}"] .analog-percentage`);
    if (percentageDisplay) {
        percentageDisplay.textContent = `${percentage}%`;
    }
    
    // Update visual bar with smooth animation
    const bar = document.getElementById(`analog-bar-${id}`);
    if (bar) {
        bar.style.height = `${percentage}%`;
        bar.style.transition = 'height 0.2s ease-out'; // Smoother transition
    }
    
    // Update visual value
    const barValue = document.getElementById(`analog-bar-value-${id}`);
    if (barValue) {
        barValue.textContent = value;
    }
    
    // Update visual label
    const barLabel = document.getElementById(`analog-bar-label-${id}`);
    if (barLabel) {
        barLabel.textContent = `${percentage}%`;
    }
}

// Update clock display
function updateClock() {
    const now = new Date();
    const timeString = now.toLocaleTimeString();
    document.getElementById('current-time').textContent = timeString;
}


// Improved refreshSystemStatus function with better network data handling
function refreshSystemStatus() {
    updateConnectionStatus(webSocketConnected ? 'connected' : 'connecting');
    
    fetch('/api/status')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            // Update timestamp of last successful data refresh
            lastDataTimestamp = Date.now();
            showDataRefreshIndicator();
            
            // Store system data globally
            systemData = data;
            
            // Update connection status indicators
            document.getElementById('wifi-status').className = data.wifi_connected ? 'indicator connected' : 'indicator disconnected';
            document.getElementById('eth-status').className = data.eth_connected ? 'indicator connected' : 'indicator disconnected';
            
            // Update protocol display
            if (data.active_protocol) {
                document.getElementById('protocol-status').textContent = data.active_protocol.toUpperCase();
            }
            
            // Update dashboard with the complete data set
            updateDashboard(data);
            
            // Update relay states
            if (data.outputs) {
                data.outputs.forEach(output => {
                    updateRelayState(output.id, output.state);
                });
            }
            
            // Update digital inputs
            if (data.inputs) {
                data.inputs.forEach(input => {
                    updateInputState(input.id, input.state);
                });
            }
            
            // Update direct inputs
            if (data.direct_inputs) {
                data.direct_inputs.forEach(input => {
                    updateDirectInputState(input.id, input.state);
                });
            }
            
            // Update HT sensors
            if (data.htSensors) {
                renderHTSensors(data.htSensors);
            }
            
            // Update analog inputs
            if (data.analog) {
                data.analog.forEach(input => {
                    updateAnalogValue(input.id, input.value);
                });
                updateAnalogChart();
            }
            
            // Try reconnecting WebSocket if not connected
            if (!webSocketConnected && reconnectAttempts < maxReconnectAttempts) {
                reconnectWebSocket();
            }
        })
        .catch(error => {
            console.error('Error fetching system status:', error);
            updateConnectionStatus('disconnected');
            showToast('Connection error. Retrying...', 'error');
            
            // Increment reconnect attempts count
            reconnectAttempts++;
            
            // Try to reconnect WebSocket if HTTP fails and we haven't exceeded max attempts
            if (!webSocketConnected && reconnectAttempts < maxReconnectAttempts) {
                setTimeout(initWebSocket, 2000);
            }
            else if (reconnectAttempts >= maxReconnectAttempts) {
                // If we've exceeded max attempts, try again after a longer delay
                reconnectAttempts = 0;
                setTimeout(refreshSystemStatus, 10000); // Try again in 10 seconds
            }
        });
}

// Fetch settings from server
function fetchSettings() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            // General settings
            document.getElementById('device-name-setting').value = data.device_name;
            document.getElementById('debug-mode').checked = data.debug_mode;
            
            // Network settings
            document.getElementById('wifi-ssid').value = data.wifi_ssid || '';
            document.getElementById('wifi-password').value = ''; // Don't populate password for security
            document.getElementById('dhcp-mode').checked = data.dhcp_mode;
            
            // Show/hide static IP settings based on DHCP mode
            document.getElementById('static-ip-settings').style.display = data.dhcp_mode ? 'none' : 'block';
            
            if (!data.dhcp_mode) {
                document.getElementById('ip-address').value = data.ip || '';
                document.getElementById('gateway').value = data.gateway || '';
                document.getElementById('subnet').value = data.subnet || '255.255.255.0';
                document.getElementById('dns1').value = data.dns1 || '8.8.8.8';
                document.getElementById('dns2').value = data.dns2 || '8.8.4.4';
            }
            
            // Firmware version
            if (data.firmware_version) {
                document.getElementById('firmware-version').textContent = data.firmware_version;
                document.getElementById('firmware-version-display').textContent = data.firmware_version;
            }
        })
        .catch(error => {
            console.error('Error loading settings:', error);
            showToast('Failed to load settings', 'error');
        });
}

// Save settings
function saveSettings() {
    // Collect settings data
    const config = {
        device_name: document.getElementById('device-name-setting').value,
        debug_mode: document.getElementById('debug-mode').checked,
        dhcp_mode: document.getElementById('dhcp-mode').checked,
        wifi_ssid: document.getElementById('wifi-ssid').value,
        wifi_password: document.getElementById('wifi-password').value
    };
    
    // Add static IP settings if DHCP is disabled
    if (!config.dhcp_mode) {
        config.ip = document.getElementById('ip-address').value;
        config.gateway = document.getElementById('gateway').value;
        config.subnet = document.getElementById('subnet').value;
        config.dns1 = document.getElementById('dns1').value;
        config.dns2 = document.getElementById('dns2').value;
    }
    
    // Send to server
    fetch('/api/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(config)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('Settings saved. Device will restart.', 'success');
            setTimeout(() => {
                showToast('Waiting for device to come back online...', 'info');
                setTimeout(checkDeviceOnline, 5000);
            }, 2000);
        } else {
            showToast(`Failed to save settings: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving settings:', error);
        showToast('Network error. Could not save settings', 'error');
    });
}

// Reset settings to default
function resetSettings() {
    fetch('/api/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ reset: true })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('Settings reset to default. Device will restart.', 'success');
            setTimeout(() => {
                showToast('Waiting for device to come back online...', 'info');
                setTimeout(checkDeviceOnline, 5000);
            }, 2000);
        } else {
            showToast(`Failed to reset settings: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error resetting settings:', error);
        showToast('Network error. Could not reset settings', 'error');
    });
}

// Fetch device time
function fetchDeviceTime() {
    fetch('/api/time')
        .then(response => response.json())
        .then(data => {
            document.getElementById('device-time').textContent = data.formatted;
            document.getElementById('rtc-available').textContent = data.rtc_available ? 'Available' : 'Not Available';
            
            // Set datetime-local input to current device time if available
            const manualDatetime = document.getElementById('manual-datetime');
            if (manualDatetime) {
                const dateStr = `${data.year}-${String(data.month).padStart(2, '0')}-${String(data.day).padStart(2, '0')}`;
                const timeStr = `${String(data.hour).padStart(2, '0')}:${String(data.minute).padStart(2, '0')}`;
                manualDatetime.value = `${dateStr}T${timeStr}`;
            }
        })
        .catch(error => {
            console.error('Error fetching device time:', error);
        });
}

// Set time from browser
function setTimeFromBrowser() {
    const now = new Date();
    
    const timeData = {
        year: now.getFullYear(),
        month: now.getMonth() + 1,
        day: now.getDate(),
        hour: now.getHours(),
        minute: now.getMinutes(),
        second: now.getSeconds()
    };
    
    setDeviceTime(timeData);
}

// Set time manually
function setTimeManually() {
    const datetimeInput = document.getElementById('manual-datetime');
    if (!datetimeInput.value) {
        showToast('Please select a date and time', 'warning');
        return;
    }
    
    const dateObj = new Date(datetimeInput.value);
    
    const timeData = {
        year: dateObj.getFullYear(),
        month: dateObj.getMonth() + 1,
        day: dateObj.getDate(),
        hour: dateObj.getHours(),
        minute: dateObj.getMinutes(),
        second: dateObj.getSeconds()
    };
    
    setDeviceTime(timeData);
}

// Sync time from NTP
function syncNTPTime() {
    fetch('/api/time', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            ntp_sync: true
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('Time synchronized from NTP server', 'success');
            fetchDeviceTime(); // Refresh displayed time
        } else {
            showToast(`Failed to sync time: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error syncing time:', error);
        showToast('Network error. Could not sync time', 'error');
    });
}

// Set device time
function setDeviceTime(timeData) {
    fetch('/api/time', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(timeData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('Device time updated successfully', 'success');
            fetchDeviceTime(); // Refresh displayed time
        } else {
            showToast(`Failed to set time: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error setting time:', error);
        showToast('Network error. Could not set time', 'error');
    });
}

// Update firmware
function updateFirmware() {
    const fileInput = document.getElementById('firmware-file');
    if (!fileInput.files.length) {
        showToast('Please select a firmware file', 'warning');
        return;
    }
    
    const firmwareFile = fileInput.files[0];
    const formData = new FormData();
    formData.append('firmware', firmwareFile);
    
    const progressBar = document.getElementById('update-progress');
    const progressFill = document.querySelector('#update-progress .progress-fill');
    const progressText = document.querySelector('#update-progress .progress-text');
    
    // Show progress bar
    progressBar.style.display = 'block';
    
    // Create AJAX request with progress monitoring
    const xhr = new XMLHttpRequest();
    
    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            const percentComplete = Math.floor((e.loaded / e.total) * 100);
            progressFill.style.width = percentComplete + '%';
            progressText.textContent = percentComplete + '%';
        }
    };
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            try {
                const response = JSON.parse(xhr.responseText);
                if (response.status === 'success') {
                    showToast('Firmware uploaded successfully. Device is updating...', 'success');
                    setTimeout(() => {
                        showToast('Waiting for device to reboot...', 'info');
                        setTimeout(checkDeviceOnline, 10000);
                    }, 2000);
                } else {
                    showToast(`Firmware update failed: ${response.message}`, 'error');
                }
            } catch (e) {
                showToast('Firmware update complete. Device will reboot.', 'success');
                setTimeout(checkDeviceOnline, 10000);
            }
        } else {
            showToast('Firmware update failed with status: ' + xhr.status, 'error');
        }
        
        // Hide progress bar after a delay
        setTimeout(() => {
            progressBar.style.display = 'none';
            progressFill.style.width = '0%';
            progressText.textContent = '0%';
        }, 3000);
    };
    
    xhr.onerror = function() {
        showToast('Network error during firmware update', 'error');
        progressBar.style.display = 'none';
    };
    
    xhr.open('POST', '/api/upload', true);
    xhr.send(formData);
    
    showToast('Uploading firmware...', 'info');
}

// Reboot device
function rebootDevice() {
    if (confirm('Are you sure you want to reboot the device?')) {
        fetch('/api/reboot', {
            method: 'POST'
        })
        .then(response => response.json())
        .then(data => {
            showToast('Rebooting device...', 'info');
            setTimeout(() => {
                showToast('Waiting for device to come back online...', 'info');
                setTimeout(checkDeviceOnline, 5000);
            }, 2000);
        })
        .catch(error => {
            console.error('Error rebooting device:', error);
            showToast('Failed to reboot device', 'error');
        });
    }
}

// Check if device is back online after reboot
function checkDeviceOnline() {
    fetch('/api/status')
        .then(response => {
            if (response.ok) {
                showToast('Device is back online', 'success');
                refreshSystemStatus();
                
                // If we were in schedules section, refresh that too
                if (document.getElementById('schedules').classList.contains('active')) {
                    fetchSchedules();
                }
            } else {
                setTimeout(checkDeviceOnline, 2000);
            }
        })
        .catch(error => {
            setTimeout(checkDeviceOnline, 2000);
        });
}

// Control a relay
function controlRelay(relayId, state) {
    console.log(`Controlling relay ${relayId} - setting to ${state ? "ON" : "OFF"}`);
    
    fetch('/api/relay', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            relay: parseInt(relayId),
            state: state
        })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.status === 'success') {
            showToast(`Relay ${parseInt(relayId) + 1} ${state ? 'ON' : 'OFF'}`);
            updateRelayState(relayId, state);
        } else {
            showToast(`Failed to control relay ${parseInt(relayId) + 1}: ${data.message}`, 'error');
            
            // Reset toggle to match actual state
            const toggleSwitch = document.querySelector(`.relay-toggle[data-relay="${relayId}"]`);
            if (toggleSwitch) toggleSwitch.checked = !state;
        }
    })
    .catch(error => {
        console.error('Error controlling relay:', error);
        showToast(`Network error. Could not control relay ${parseInt(relayId) + 1}`, 'error');
        
        // Reset toggle to match actual state
        const toggleSwitch = document.querySelector(`.relay-toggle[data-relay="${relayId}"]`);
        if (toggleSwitch) toggleSwitch.checked = !state;
    });
}

// Control all relays
function controlAllRelays(state) {
    fetch('/api/relay', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            relay: 99,  // Special value for all relays
            state: state
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`All relays turned ${state ? 'ON' : 'OFF'}`);
            
            // Update all toggles and visuals
            for (let i = 0; i < 16; i++) {
                updateRelayState(i, state);
            }
        } else {
            showToast(`Failed to control all relays`, 'error');
        }
    })
    .catch(error => {
        console.error('Error controlling all relays:', error);
        showToast(`Network error. Could not control relays`, 'error');
    });
}

// Control selected relays
function controlSelectedRelays(state) {
    const selected = [];
    document.querySelectorAll('.relay-checkbox:checked').forEach(checkbox => {
        selected.push(parseInt(checkbox.getAttribute('data-relay')));
    });
    
    if (selected.length === 0) {
        showToast('No relays selected', 'warning');
        return;
    }
    
    // Create promises for each relay
    const promises = selected.map(relayId => {
        return fetch('/api/relay', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                relay: relayId,
                state: state
            })
        })
        .then(response => response.json());
    });
    
    // Wait for all operations to complete
    Promise.all(promises)
        .then(results => {
            const allSuccess = results.every(data => data.status === 'success');
            if (allSuccess) {
                showToast(`${selected.length} relays turned ${state ? 'ON' : 'OFF'}`);
                
                // Update selected toggles and visuals
                selected.forEach(relayId => {
                    updateRelayState(relayId, state);
                });
            } else {
                showToast(`Some relays could not be controlled`, 'warning');
                
                // Refresh to get actual states
                refreshSystemStatus();
            }
        })
        .catch(error => {
            console.error('Error controlling selected relays:', error);
            showToast(`Network error. Could not control selected relays`, 'error');
        });
}

// Show toast notification with improved visibility
function showToast(message, type = 'info') {
    // Clear any existing timeout
    if (toast.timeoutId) clearTimeout(toast.timeoutId);
    
    // Set toast color based on type
    let backgroundColor;
    switch (type) {
        case 'success':
            backgroundColor = 'rgba(0, 128, 0, 0.9)';
            break;
        case 'error':
            backgroundColor = 'rgba(220, 0, 0, 0.9)';
            break;
        case 'warning':
            backgroundColor = 'rgba(255, 140, 0, 0.9)';
            break;
        default:
            backgroundColor = 'rgba(0, 0, 0, 0.8)';
    }
    
    toast.style.backgroundColor = backgroundColor;
    toast.textContent = message;
    toast.className = 'toast show';
    
    // Auto hide after 3 seconds
    toast.timeoutId = setTimeout(() => {
        toast.className = 'toast';
    }, 3000);
}

// Initialize Input Interrupts UI
function initInputInterruptsUI() {
    console.log("Initializing Input Interrupts UI");
    
    // Load interrupt configurations from server
    fetchInterruptConfigs();
    
    // Setup batch configure button
    const configureAllBtn = document.getElementById('configure-all-interrupts');
    if (configureAllBtn) {
        configureAllBtn.addEventListener('click', function() {
            openInterruptModal();
        });
    }
    
    // Setup enable all button
    const enableAllBtn = document.getElementById('enable-all-interrupts');
    if (enableAllBtn) {
        enableAllBtn.addEventListener('click', function() {
            enableAllInterrupts();
        });
    }
    
    // Setup disable all button
    const disableAllBtn = document.getElementById('disable-all-interrupts');
    if (disableAllBtn) {
        disableAllBtn.addEventListener('click', function() {
            disableAllInterrupts();
        });
    }
    
    // Setup modal close buttons
    document.querySelectorAll('#interrupt-modal .close-modal, #interrupt-modal .close-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.getElementById('interrupt-modal').style.display = 'none';
        });
    });
    
    // Setup form submission
    const interruptForm = document.getElementById('interrupt-form');
    if (interruptForm) {
        interruptForm.addEventListener('submit', function(e) {
            e.preventDefault();
            saveInterruptConfig();
        });
    }
}

// Fetch interrupt configurations from the server
function fetchInterruptConfigs() {
    fetch('/api/interrupts')
        .then(response => response.json())
        .then(data => {
            renderInterruptsTable(data.interrupts);
        })
        .catch(error => {
            console.error('Error fetching interrupt configurations:', error);
            showToast('Failed to load interrupt configurations', 'error');
        });
}

// Render interrupts table
function renderInterruptsTable(interrupts) {
    const tableBody = document.querySelector('#interrupts-table tbody');
    if (!tableBody) return;
    
    tableBody.innerHTML = '';
    
    if (!interrupts || interrupts.length === 0) {
        const row = document.createElement('tr');
        row.innerHTML = '<td colspan="5" class="text-center">No interrupt configurations found</td>';
        tableBody.appendChild(row);
        return;
    }
    
    interrupts.forEach(interrupt => {
        const row = document.createElement('tr');
        
        // Format priority
        let priorityBadge = '';
        let priorityText = '';
        
        switch(interrupt.priority) {
            case 1:
                priorityBadge = '<span class="priority-badge high-priority">High</span>';
                priorityText = 'High';
                break;
            case 2:
                priorityBadge = '<span class="priority-badge medium-priority">Medium</span>';
                priorityText = 'Medium';
                break;
            case 3:
                priorityBadge = '<span class="priority-badge low-priority">Low</span>';
                priorityText = 'Low';
                break;
            default:
                priorityBadge = '<span class="priority-badge no-priority">None</span>';
                priorityText = 'None (Polling)';
        }
        
        // Format trigger type
        let triggerText = '';
        let triggerBadge = '';
        
        switch(interrupt.triggerType) {
            case 0:
                triggerText = 'Rising Edge';
                triggerBadge = '<span class="trigger-badge trigger-rising">Rising</span>';
                break;
            case 1:
                triggerText = 'Falling Edge';
                triggerBadge = '<span class="trigger-badge trigger-falling">Falling</span>';
                break;
            case 2:
                triggerText = 'Change (Both Edges)';
                triggerBadge = '<span class="trigger-badge trigger-change">Change</span>';
                break;
            case 3:
                triggerText = 'High Level';
                triggerBadge = '<span class="trigger-badge trigger-high">High Level</span>';
                break;
            case 4:
                triggerText = 'Low Level';
                triggerBadge = '<span class="trigger-badge trigger-low">Low Level</span>';
                break;
        }
        
        // Format input name
        let inputName = `Input ${interrupt.inputIndex + 1}`;
        
        row.innerHTML = `
            <td><input type="checkbox" ${interrupt.enabled ? 'checked' : ''} onchange="toggleInterrupt(${interrupt.id}, this.checked)"></td>
            <td>${interrupt.name}</td>
            <td>${priorityBadge}</td>
            <td>${inputName} ${triggerBadge}</td>
            <td>
                <div class="action-btns">
                    <button class="btn btn-secondary btn-sm" onclick="editInterrupt(${interrupt.id})"><i class="fa fa-edit"></i></button>
                </div>
            </td>
        `;
        
        tableBody.appendChild(row);
    });
}

// Open interrupt configuration modal
function openInterruptModal(interruptId = null) {
    const modal = document.getElementById('interrupt-modal');
    if (!modal) return;
    
    modal.style.display = 'block';
    
    // Reset form
    document.getElementById('interrupt-form').reset();
    document.getElementById('interrupt-id').value = '';
    
    // If editing an existing interrupt
    if (interruptId !== null) {
        document.getElementById('interrupt-id').value = interruptId;
        
        // Fetch the interrupt data
        fetch(`/api/interrupts?id=${interruptId}`)
            .then(response => response.json())
            .then(data => {
                const interrupts = data.interrupts;
                if (interrupts && interrupts.length > 0) {
                    // Find the interrupt with matching ID
                    const interrupt = interrupts.find(i => i.id === parseInt(interruptId));
                    if (interrupt) {
                        // Set form values from interrupt data
                        document.getElementById('interrupt-enabled').checked = interrupt.enabled;
                        document.getElementById('interrupt-name').value = interrupt.name;
                        document.getElementById('interrupt-input').value = interrupt.inputIndex;
                        document.getElementById('interrupt-priority').value = interrupt.priority;
                        document.getElementById('interrupt-trigger-type').value = interrupt.triggerType;
                    }
                }
            })
            .catch(error => {
                console.error('Error loading interrupt configuration:', error);
                showToast('Failed to load interrupt configuration', 'error');
            });
    }
}

// Save interrupt configuration
function saveInterruptConfig() {
    const interruptId = document.getElementById('interrupt-id').value;
    const isNew = !interruptId;
    
    // Create interrupt object
    const interrupt = {
        id: interruptId ? parseInt(interruptId) : null,
        enabled: document.getElementById('interrupt-enabled').checked,
        name: document.getElementById('interrupt-name').value || `Input ${document.getElementById('interrupt-input').value + 1} Interrupt`,
        inputIndex: parseInt(document.getElementById('interrupt-input').value),
        priority: parseInt(document.getElementById('interrupt-priority').value),
        triggerType: parseInt(document.getElementById('interrupt-trigger-type').value)
    };
    
    console.log("Saving interrupt configuration:", interrupt);
    
    // Show saving message
    showToast('Saving interrupt configuration...', 'info');
    
    // Save to server
    fetch('/api/interrupts', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ interrupt: interrupt })
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! Status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.status === 'success') {
            // Hide the modal
            document.getElementById('interrupt-modal').style.display = 'none';
            
            showToast(`Interrupt configuration ${isNew ? 'created' : 'updated'} successfully`, 'success');
            
            // Refresh the interrupts table
            fetchInterruptConfigs();
        } else {
            showToast(`Failed to ${isNew ? 'create' : 'update'} interrupt configuration: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving interrupt configuration:', error);
        showToast(`Network error. Could not ${isNew ? 'create' : 'update'} interrupt configuration`, 'error');
    });
}

// Enable all interrupts
function enableAllInterrupts() {
    fetch('/api/interrupts', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            action: "enable_all"
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('All interrupts enabled');
            fetchInterruptConfigs();
        } else {
            showToast(`Failed to enable all interrupts: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error enabling all interrupts:', error);
        showToast('Network error. Could not enable all interrupts', 'error');
    });
}

// Disable all interrupts
function disableAllInterrupts() {
    fetch('/api/interrupts', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            action: "disable_all"
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('All interrupts disabled');
            fetchInterruptConfigs();
        } else {
            showToast(`Failed to disable all interrupts: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error disabling all interrupts:', error);
        showToast('Network error. Could not disable all interrupts', 'error');
    });
}

// Global functions for interrupt management
window.editInterrupt = function(interruptId) {
    openInterruptModal(interruptId);
};

window.toggleInterrupt = function(interruptId, enabled) {
    fetch('/api/interrupts', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
            id: interruptId,
            enabled: enabled
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast(`Interrupt ${enabled ? 'enabled' : 'disabled'}`);
        } else {
            showToast(`Failed to update interrupt: ${data.message}`, 'error');
            fetchInterruptConfigs(); // Refresh to get actual state
        }
    })
    .catch(error => {
        console.error('Error toggling interrupt:', error);
        showToast('Network error. Could not update interrupt', 'error');
        fetchInterruptConfigs(); // Refresh to get actual state
    });
};

// Initialize HT Sensors UI
function initHTSensorsUI() {
    // Create the configuration button if it doesn't exist
    const analogInputsSection = document.getElementById('analog-inputs');
    if (!analogInputsSection) return;
    
    // Check if HT sensors container already exists
    if (!document.getElementById('ht-sensors-container')) {
        // Create sensors container
        const htSensorsContainer = document.createElement('div');
        htSensorsContainer.id = 'ht-sensors-container';
        htSensorsContainer.className = 'sensors-container';
        htSensorsContainer.innerHTML = `
            <h3>Temperature & Humidity Sensors</h3>
            <div id="ht-sensors-grid" class="sensors-grid">
                <!-- HT sensors will be inserted here -->
            </div>
        `;
        
        // Insert it before the analog inputs grid
        const analogInputsGrid = document.getElementById('analog-inputs-grid');
        if (analogInputsGrid) {
            analogInputsSection.insertBefore(htSensorsContainer, analogInputsGrid);
        } else {
            analogInputsSection.appendChild(htSensorsContainer);
        }
    }
    
    // Create a configuration button for HT sensors
    if (!document.getElementById('configure-ht-sensors')) {
        const configureBtn = document.createElement('button');
        configureBtn.id = 'configure-ht-sensors';
        configureBtn.className = 'btn btn-primary';
        configureBtn.textContent = 'Configure HT Sensors';
        configureBtn.addEventListener('click', showHTSensorConfigModal);
        
        // Add it before the HT sensors container
        const htSensorsContainer = document.getElementById('ht-sensors-container');
        if (htSensorsContainer) {
            htSensorsContainer.parentNode.insertBefore(configureBtn, htSensorsContainer);
        }
    }
    
    // Setup modal close buttons
    const closeButtons = document.querySelectorAll('#ht-sensor-config-modal .close-modal, #ht-sensor-config-modal .close-btn');
    closeButtons.forEach(btn => {
        btn.addEventListener('click', function() {
            document.getElementById('ht-sensor-config-modal').style.display = 'none';
        });
    });
    
    // Setup form submission
    const htSensorConfigForm = document.getElementById('ht-sensor-config-form');
    if (htSensorConfigForm) {
        htSensorConfigForm.addEventListener('submit', function(e) {
            e.preventDefault(); // Prevent form submission
            saveHTSensorConfig();
        });
    }
    
    // Initial fetch of HT sensor data
    fetchHTSensors();
}



// Function to fetch HT sensor data and update the UI
function fetchHTSensors() {
    fetch('/api/ht-sensors')
        .then(response => response.json())
        .then(data => {
            console.log("Fetched HT sensor data:", data);
            
            if (data.htSensors) {
                renderHTSensors(data.htSensors);
            }
        })
        .catch(error => {
            console.error('Error fetching HT sensors data:', error);
            showToast('Failed to load HT sensors data', 'error');
        });
}


// Function to render HT sensors in the UI
function renderHTSensors(sensors) {
    const htSensorsGrid = document.getElementById('ht-sensors-grid');
    if (!htSensorsGrid) return;
    
    htSensorsGrid.innerHTML = '';
    
    sensors.forEach(sensor => {
        const sensorCard = document.createElement('div');
        sensorCard.className = 'sensor-card';
        
        let sensorContent = '';
        
        // Create content based on sensor type
        switch (sensor.sensorType) {
            case 0: // Digital Input
                sensorContent = `
                    <div class="sensor-header">
                        <h4>${sensor.pin}</h4>
                        <span class="sensor-type">${sensor.sensorTypeName}</span>
                    </div>
                    <div class="sensor-value ${sensor.value === 'HIGH' ? 'active' : ''}">
                        ${sensor.value}
                    </div>
                `;
                break;
                
            case 1: // DHT11
            case 2: // DHT22
                sensorContent = `
                    <div class="sensor-header">
                        <h4>${sensor.pin}</h4>
                        <span class="sensor-type">${sensor.sensorTypeName}</span>
                    </div>
                    <div class="sensor-values">
                        <div class="sensor-temp">
                            <i class="fas fa-thermometer-half"></i>
                            <span>${sensor.temperature !== undefined ? sensor.temperature.toFixed(1) : '--'}°C</span>
                        </div>
                        <div class="sensor-humidity">
                            <i class="fas fa-tint"></i>
                            <span>${sensor.humidity !== undefined ? sensor.humidity.toFixed(1) : '--'}%</span>
                        </div>
                    </div>
                `;
                break;
                
            case 3: // DS18B20
                sensorContent = `
                    <div class="sensor-header">
                        <h4>${sensor.pin}</h4>
                        <span class="sensor-type">${sensor.sensorTypeName}</span>
                    </div>
                    <div class="sensor-values">
                        <div class="sensor-temp">
                            <i class="fas fa-thermometer-half"></i>
                            <span>${sensor.temperature !== undefined ? sensor.temperature.toFixed(1) : '--'}°C</span>
                        </div>
                    </div>
                `;
                break;
        }
        
        sensorCard.innerHTML = sensorContent;
        htSensorsGrid.appendChild(sensorCard);
    });
}
// Function to create HT sensor configuration modal
// Replace the createHTSensorConfigModal function with this improved version
function createHTSensorConfigModal() {
    // Check if modal already exists
    if (document.getElementById('ht-sensor-config-modal')) return;
    
    const modal = document.createElement('div');
    modal.id = 'ht-sensor-config-modal';
    modal.className = 'modal';
    
    modal.innerHTML = `
        <div class="modal-content">
            <span class="close-modal">&times;</span>
            <h3>Configure HT Sensors</h3>
            <form id="ht-sensor-config-form">
                <!-- HT1 Sensor -->
                <div class="form-group">
                    <label for="ht1-sensor-type">HT1 Sensor Type</label>
                    <select id="ht1-sensor-type" data-index="0">
                        <option value="0">Digital Input</option>
                        <option value="1">DHT11</option>
                        <option value="2">DHT22</option>
                        <option value="3">DS18B20</option>
                    </select>
                </div>
                
                <!-- HT2 Sensor -->
                <div class="form-group">
                    <label for="ht2-sensor-type">HT2 Sensor Type</label>
                    <select id="ht2-sensor-type" data-index="1">
                        <option value="0">Digital Input</option>
                        <option value="1">DHT11</option>
                        <option value="2">DHT22</option>
                        <option value="3">DS18B20</option>
                    </select>
                </div>
                
                <!-- HT3 Sensor -->
                <div class="form-group">
                    <label for="ht3-sensor-type">HT3 Sensor Type</label>
                    <select id="ht3-sensor-type" data-index="2">
                        <option value="0">Digital Input</option>
                        <option value="1">DHT11</option>
                        <option value="2">DHT22</option>
                        <option value="3">DS18B20</option>
                    </select>
                </div>
                
                <div class="sensor-config-info">
                    <p><strong>Sensor Types:</strong></p>
                    <ul>
                        <li><strong>Digital Input:</strong> Use as a standard digital input</li>
                        <li><strong>DHT11:</strong> Basic temperature/humidity sensor (±2°C, ±5%RH)</li>
                        <li><strong>DHT22:</strong> Higher precision temperature/humidity sensor (±0.5°C, ±2%RH)</li>
                        <li><strong>DS18B20:</strong> Precision temperature sensor (±0.5°C)</li>
                    </ul>
                    <p><strong>Note:</strong> Changing sensor type will reset any associated schedules or triggers.</p>
                </div>
                
                <div class="form-actions">
                    <button type="submit" class="btn btn-success">Save Configuration</button>
                    <button type="button" class="btn btn-secondary close-btn">Cancel</button>
                </div>
            </form>
        </div>
    `;
    
    document.body.appendChild(modal);
    
    // Setup modal close events
    const closeButtons = modal.querySelectorAll('.close-modal, .close-btn');
    closeButtons.forEach(btn => {
        btn.addEventListener('click', function(e) {
            e.preventDefault(); // Prevent form submission
            modal.style.display = 'none';
        });
    });
    
    // Setup form submission handler properly
    const form = document.getElementById('ht-sensor-config-form');
    if (form) {
        form.addEventListener('submit', function(e) {
            e.preventDefault(); // Critical - prevent the default form submission
            saveHTSensorConfig();
        });
    }
    
    // When modal is clicked outside content area, close it
    modal.addEventListener('click', function(e) {
        if (e.target === modal) {
            modal.style.display = 'none';
        }
    });
}

// Replace the showHTSensorConfigModal function with this improved version
function showHTSensorConfigModal() {
    console.log("Opening HT sensor configuration modal");
    const modal = document.getElementById('ht-sensor-config-modal');
    if (!modal) {
        console.error("HT sensor configuration modal not found");
        return;
    }
    
    // Fetch current sensor configuration
    fetch('/api/ht-sensors')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            console.log("Loaded HT sensor data:", data);
            
            // Populate form with current settings
            if (data.htSensors && data.htSensors.length > 0) {
                data.htSensors.forEach(sensor => {
                    const selectElement = document.getElementById(`ht${sensor.index + 1}-sensor-type`);
                    if (selectElement) {
                        selectElement.value = sensor.sensorType;
                    }
                });
            }
            
            // Display the modal
            modal.style.display = 'block';
        })
        .catch(error => {
            console.error('Error fetching sensor configuration:', error);
            showToast('Failed to load sensor configuration', 'error');
            
            // Still show the modal with default values
            modal.style.display = 'block';
        });
}

// Save HT sensor configuration - FIXED FUNCTION
function saveHTSensorConfig() {
    console.log('Saving HT sensor configuration');
    showToast('Saving sensor configuration...', 'info');
    
    // Create promises array to save all sensors
    const savePromises = [];
    
    // Process each sensor one by one
    for (let i = 0; i < 3; i++) {
        const select = document.getElementById(`ht${i + 1}-sensor-type`);
        if (select) {
            const sensorType = parseInt(select.value);
            console.log(`HT${i+1} sensor type: ${sensorType}`);
            
            // Create config object for this sensor
            const config = {
                index: i,
                sensorType: sensorType
            };
            
            // Create promise for saving this sensor
            const savePromise = fetch('/api/ht-sensors', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ sensor: config })
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                return response.json();
            });
            
            savePromises.push(savePromise);
        }
    }
    
    // Wait for all sensor configurations to be saved
    Promise.all(savePromises)
        .then(results => {
            // Check if all sensors were saved successfully
            const allSuccess = results.every(data => data.status === "success");
            
            // Hide the modal
            const modal = document.getElementById('ht-sensor-config-modal');
            if (modal) {
                modal.style.display = 'none';
            }
            
            if (allSuccess) {
                showToast('Sensor configuration saved successfully', 'success');
                
                // Refresh sensor display after a delay to allow sensors to initialize
                setTimeout(() => {
                    fetchHTSensors();
                }, 2000);
            } else {
                showToast('Some sensors could not be updated', 'warning');
                fetchHTSensors();
            }
        })
        .catch(error => {
            console.error('Error saving sensor configuration:', error);
            showToast('Error saving sensor configuration', 'error');
        });
}

// Modified initNetworkUI function to refresh network status periodically
function initNetworkUI() {
    console.log("Initializing Network UI");
    
    // Load network settings immediately
    fetchNetworkSettings();
    
    // Set up periodic refresh of network status when on network page
    const refreshNetworkStatus = function() {
        if (document.getElementById('network').classList.contains('active')) {
            fetchNetworkSettings();
        }
    };
    
    // Refresh network status every 5 seconds when on network page
    setInterval(refreshNetworkStatus, 5000);
    
    // Setup manual refresh button
    const refreshNetworkBtn = document.getElementById('refresh-network');
    if (refreshNetworkBtn) {
        refreshNetworkBtn.addEventListener('click', fetchNetworkSettings);
    }
    
    // Set up DHCP toggle
    const dhcpToggle = document.getElementById('network-dhcp-mode');
    if (dhcpToggle) {
        dhcpToggle.addEventListener('change', function() {
            const staticSettings = document.getElementById('network-static-settings');
            if (staticSettings) {
                staticSettings.style.display = this.checked ? 'none' : 'block';
            }
        });
    }
    
    // Set up WiFi connection form
    const wifiForm = document.getElementById('wifi-connect-form');
    if (wifiForm) {
        wifiForm.addEventListener('submit', function(e) {
            e.preventDefault();
            connectToWiFi();
        });
    }
    
    // Set up network settings form
    const networkForm = document.getElementById('network-settings-form');
    if (networkForm) {
        networkForm.addEventListener('submit', function(e) {
            e.preventDefault();
            saveNetworkSettings();
        });
    }
}

// Improved fetchNetworkSettings function to use system status data for network display
function fetchNetworkSettings() {
    // Get system status data that includes network information
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Use the complete system data to update network status display
            updateNetworkStatusDisplay(data);
            
            // Update DHCP mode toggle
            const dhcpToggle = document.getElementById('network-dhcp-mode');
            if (dhcpToggle) {
                dhcpToggle.checked = data.dhcp_mode !== undefined ? data.dhcp_mode : true;
            }
            
            // Show/hide static settings
            const staticSettings = document.getElementById('network-static-settings');
            if (staticSettings) {
                staticSettings.style.display = data.dhcp_mode !== false ? 'none' : 'block';
            }
            
            // Fill in static IP settings if available
            if (data.network && !data.dhcp_mode) {
                if (document.getElementById('network-ip')) {
                    document.getElementById('network-ip').value = 
                        data.ip || data.network.eth_ip || data.network.wifi_ip || '';
                }
                if (document.getElementById('network-gateway')) {
                    document.getElementById('network-gateway').value = 
                        data.gateway || data.network.eth_gateway || data.network.wifi_gateway || '';
                }
                if (document.getElementById('network-subnet')) {
                    document.getElementById('network-subnet').value = 
                        data.subnet || data.network.eth_subnet || data.network.wifi_subnet || '255.255.255.0';
                }
                if (document.getElementById('network-dns1')) {
                    document.getElementById('network-dns1').value = 
                        data.dns1 || data.network.eth_dns1 || data.network.wifi_dns1 || '8.8.8.8';
                }
                if (document.getElementById('network-dns2')) {
                    document.getElementById('network-dns2').value = 
                        data.dns2 || data.network.eth_dns2 || data.network.wifi_dns2 || '8.8.4.4';
                }
            }
            
            // Fill in WiFi settings
            const wifiSsidField = document.getElementById('wifi-ssid');
            if (wifiSsidField) {
                wifiSsidField.value = data.wifi_ssid || '';
            }
        })
        .catch(error => {
            console.error('Error fetching network settings:', error);
            showToast('Failed to load network settings', 'error');
        });
}


// Enhanced updateNetworkStatusDisplay function to properly show network information
function updateNetworkStatusDisplay(data) {
    // Update WiFi status
    const wifiStatus = document.getElementById('network-wifi-status');
    if (wifiStatus) {
        if (data.wifi_client_mode) {
            wifiStatus.innerHTML = `
                <div class="status-card success">
                    <h4>WiFi Client Connected</h4>
                    <p><strong>SSID:</strong> ${data.wifi_ssid || 'Unknown'}</p>
                    <p><strong>IP:</strong> ${data.wifi_ip || 'Not assigned'}</p>
                    <p><strong>MAC Address:</strong> ${data.mac || 'Unknown'}</p>
                    <p><strong>Gateway:</strong> ${data.wifi_gateway || 'Unknown'}</p>
                    <p><strong>Signal:</strong> ${data.wifi_rssi || '0'} dBm</p>
                </div>
            `;
        } else if (data.wifi_ap_mode) {
            wifiStatus.innerHTML = `
                <div class="status-card warning">
                    <h4>Access Point Mode Active</h4>
                    <p><strong>SSID:</strong> ${data.device || 'KC868-A16'}</p>
                    <p><strong>IP:</strong> ${data.wifi_ip || '192.168.4.1'}</p>
                    <p><strong>MAC Address:</strong> ${data.mac || 'Unknown'}</p>
                    <p><strong>WiFi Client Mode:</strong> Disconnected</p>
                </div>
            `;
        } else {
            wifiStatus.innerHTML = `
                <div class="status-card error">
                    <h4>WiFi Disconnected</h4>
                    <p>Not connected to any wireless network</p>
                    <p><strong>MAC Address:</strong> ${data.mac || 'Unknown'}</p>
                </div>
            `;
        }
    }
    
    // Update Ethernet status
    const ethStatus = document.getElementById('network-eth-status');
    if (ethStatus) {
        if (data.eth_connected) {
            ethStatus.innerHTML = `
                <div class="status-card success">
                    <h4>Ethernet Connected</h4>
                    <p><strong>IP:</strong> ${data.eth_ip || 'Not assigned'}</p>
                    <p><strong>MAC Address:</strong> ${data.mac || 'Unknown'}</p>
                    <p><strong>Gateway:</strong> ${data.eth_gateway || 'Unknown'}</p>
                    <p><strong>Speed:</strong> ${data.eth_speed || 'Unknown'}</p>
                    <p><strong>Duplex:</strong> ${data.eth_duplex || 'Unknown'}</p>
                </div>
            `;
        } else {
            ethStatus.innerHTML = `
                <div class="status-card error">
                    <h4>Ethernet Disconnected</h4>
                    <p>No wired connection detected</p>
                </div>
            `;
        }
    }
    
    // Update connection type badge
    const connectionType = document.getElementById('connection-type');
    if (connectionType) {
        if (data.eth_connected) {
            connectionType.innerHTML = `<span class="badge badge-success">Ethernet</span>`;
        } else if (data.wifi_client_mode) {
            connectionType.innerHTML = `<span class="badge badge-primary">WiFi Client</span>`;
        } else if (data.wifi_ap_mode) {
            connectionType.innerHTML = `<span class="badge badge-warning">Access Point</span>`;
        } else {
            connectionType.innerHTML = `<span class="badge badge-danger">No Connection</span>`;
        }
    }
    
    // Update DHCP status
    const dhcpStatus = document.getElementById('dhcp-status');
    if (dhcpStatus) {
        dhcpStatus.innerHTML = data.dhcp_mode ? 
            `<span class="badge badge-info">DHCP Enabled</span>` : 
            `<span class="badge badge-secondary">Static IP</span>`;
    }
}

// Connect to WiFi network
function connectToWiFi() {
    const ssid = document.getElementById('wifi-ssid').value;
    const password = document.getElementById('wifi-password').value;
    
    if (!ssid) {
        showToast('Please enter a WiFi network name (SSID)', 'warning');
        return;
    }
    
    showToast('Connecting to WiFi...', 'info');
    
    const wifiData = {
        wifi_ssid: ssid,
        wifi_password: password
    };
    
    fetch('/api/network/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(wifiData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('WiFi settings updated. Device is connecting...', 'success');
            setTimeout(() => {
                showToast('Waiting for connection. Page will reload in 10 seconds...', 'info');
                setTimeout(() => {
                    window.location.reload();
                }, 10000);
            }, 2000);
        } else {
            showToast(`Failed to update WiFi settings: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error updating WiFi settings:', error);
        showToast('Network error. Could not update WiFi settings', 'error');
    });
}

// Save network settings
function saveNetworkSettings() {
    const dhcpMode = document.getElementById('network-dhcp-mode').checked;
    
    const networkData = {
        dhcp_mode: dhcpMode
    };
    
    // Add static IP settings if DHCP is disabled
    if (!dhcpMode) {
        networkData.ip = document.getElementById('network-ip').value;
        networkData.gateway = document.getElementById('network-gateway').value;
        networkData.subnet = document.getElementById('network-subnet').value;
        networkData.dns1 = document.getElementById('network-dns1').value;
        networkData.dns2 = document.getElementById('network-dns2').value;
    }
    
    showToast('Saving network settings...', 'info');
    
    fetch('/api/network/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(networkData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.status === 'success') {
            showToast('Network settings saved. Device will restart.', 'success');
            setTimeout(() => {
                showToast('Waiting for device to restart. Page will reload in 15 seconds...', 'info');
                setTimeout(() => {
                    window.location.reload();
                }, 15000);
            }, 2000);
        } else {
            showToast(`Failed to save network settings: ${data.message}`, 'error');
        }
    })
    .catch(error => {
        console.error('Error saving network settings:', error);
        showToast('Network error. Could not save settings', 'error');
    });
}

// Create Digital+HT Sensor section for the schedule modal
function createDigitalHTSensorSection() {
    const form = document.getElementById('schedule-form');
    if (!form) return;
    
    // Create the section
    const digitalHTSensorSection = document.createElement('div');
    digitalHTSensorSection.id = 'digital-ht-sensor-section';
    digitalHTSensorSection.style.display = 'none';
    digitalHTSensorSection.innerHTML = `
        <h4>Digital Inputs Configuration</h4>
        <div class="form-group">
            <label>Select Digital Inputs</label>
            <div id="digital-ht-input-checkboxes" class="checkbox-grid">
                <!-- Will be filled by JavaScript -->
            </div>
        </div>
        <div class="form-group">
            <label>Select Direct Inputs</label>
            <div id="digital-ht-direct-input-checkboxes" class="checkbox-grid">
                <!-- Will be filled by JavaScript -->
            </div>
        </div>
        <div class="form-group">
            <label for="digital-ht-input-logic">Logic</label>
            <select id="digital-ht-input-logic">
                <option value="0">AND (All conditions must be met)</option>
                <option value="1">OR (Any condition triggers)</option>
            </select>
        </div>
        
        <h4>HT Sensor Configuration</h4>
        <div class="form-group">
            <label for="digital-ht-sensor">Select Sensor</label>
            <select id="digital-ht-sensor" required>
                <option value="0">HT1</option>
                <option value="1">HT2</option>
                <option value="2">HT3</option>
            </select>
        </div>
        <div class="form-group">
            <label for="digital-ht-sensor-type">Measurement Type</label>
            <select id="digital-ht-sensor-type" required>
                <option value="0">Temperature (°C)</option>
                <option value="1">Humidity (%)</option>
            </select>
        </div>
        <div class="form-group">
            <label for="digital-ht-sensor-condition">Condition</label>
            <select id="digital-ht-sensor-condition" required>
                <option value="0">Above</option>
                <option value="1">Below</option>
                <option value="2">Equal to (±0.5)</option>
            </select>
        </div>
        <div class="form-group">
            <label for="digital-ht-sensor-threshold" id="digital-ht-threshold-label">Threshold (°C)</label>
            <input type="number" id="digital-ht-sensor-threshold" step="0.1" min="-40" max="125" value="25.0" required>
        </div>
        <div class="digital-ht-info">
            <p><strong>Combined Digital+HT Sensor Trigger:</strong></p>
            <ul>
                <li>Digital inputs will be combined using the selected logic (AND/OR)</li>
                <li>The HT sensor condition must also be met for the trigger to activate</li>
                <li>Both digital and HT sensor conditions must be satisfied simultaneously</li>
            </ul>
        </div>
    `;
    
    // Find where to insert it
    const sensorSection = document.getElementById('sensor-trigger-section');
    if (sensorSection && sensorSection.parentNode) {
        sensorSection.parentNode.insertBefore(digitalHTSensorSection, sensorSection.nextSibling);
    } else {
        // If sensor section doesn't exist yet, add to the end of form
        form.appendChild(digitalHTSensorSection);
    }
    
    // Setup change handler for HT sensor type
    const htSensorTypeSelect = document.getElementById('digital-ht-sensor-type');
    if (htSensorTypeSelect) {
        htSensorTypeSelect.addEventListener('change', function() {
            const thresholdLabel = document.getElementById('digital-ht-threshold-label');
            const thresholdInput = document.getElementById('digital-ht-sensor-threshold');
            
            if (this.value === "0") { // Temperature
                thresholdLabel.textContent = "Threshold (°C)";
                thresholdInput.min = "-40";
                thresholdInput.max = "125";
                thresholdInput.value = "25.0";
            } else { // Humidity
                thresholdLabel.textContent = "Threshold (%)";
                thresholdInput.min = "0";
                thresholdInput.max = "100";
                thresholdInput.value = "50.0";
            }
        });
    }
    
    // Immediately populate the input checkboxes
    createDigitalHTSensorInputCheckboxes();
}

// Function to create Digital+HT Sensor input checkboxes
function createDigitalHTSensorInputCheckboxes() {
    // Fill digital input checkboxes
    const digitalInputsCheckboxes = document.getElementById('digital-ht-input-checkboxes');
    if (digitalInputsCheckboxes) {
        digitalInputsCheckboxes.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const container = document.createElement('div');
            container.className = 'input-container';
            
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-input', i);
            
            // Create state select that appears when checked
            const stateSelect = document.createElement('select');
            stateSelect.className = 'input-state-select';
            stateSelect.style.display = 'none'; // Initially hidden
            stateSelect.innerHTML = `
                <option value="0">LOW</option>
                <option value="1">HIGH</option>
            `;
            
            // Show/hide state select when checkbox changes
            checkbox.addEventListener('change', function() {
                stateSelect.style.display = this.checked ? 'inline-block' : 'none';
            });
            
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` Input ${i+1} `));
            container.appendChild(label);
            container.appendChild(stateSelect);
            
            digitalInputsCheckboxes.appendChild(container);
        }
    }
    
    // Fill HT direct input checkboxes
    const htDirectInputsCheckboxes = document.getElementById('digital-ht-direct-input-checkboxes');
    if (htDirectInputsCheckboxes) {
        htDirectInputsCheckboxes.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            const container = document.createElement('div');
            container.className = 'input-container';
            
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.setAttribute('data-input', i + 16); // HT inputs are at bits 16-18
            
            // Create state select that appears when checked
            const stateSelect = document.createElement('select');
            stateSelect.className = 'input-state-select';
            stateSelect.style.display = 'none'; // Initially hidden
            stateSelect.innerHTML = `
                <option value="0">LOW</option>
                <option value="1">HIGH</option>
            `;
            
            // Show/hide state select when checkbox changes
            checkbox.addEventListener('change', function() {
                stateSelect.style.display = this.checked ? 'inline-block' : 'none';
            });
            
            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` HT${i+1} `));
            container.appendChild(label);
            container.appendChild(stateSelect);
            
            htDirectInputsCheckboxes.appendChild(container);
        }
    }
}



