// Import necessary modules
const http = require('http'); // For creating the HTTP server
const fs = require('fs');   // For reading the HTML file
const path = require('path'); // For resolving file paths
const { WebSocketServer } = require('ws'); // For WebSocket server

// Determine the port to listen on. Render.com provides this via process.env.PORT.
// Default to 8080 for local testing if PORT is not set.
const PORT = process.env.PORT || 8080;

// --- HTTP Server Setup ---
// Create an HTTP server that will serve your HTML file
const httpServer = http.createServer((req, res) => {
    // Serve the robotic_arm.html file when the root URL is accessed
    if (req.url === '/' || req.url === '/index.html') {
        // Construct the full path to the HTML file
        const htmlFilePath = path.join(__dirname, 'robotic_arm.html');

        // Read the HTML file asynchronously
        fs.readFile(htmlFilePath, (err, data) => {
            if (err) {
                // If there's an error reading the file (e.g., file not found)
                console.error(`Error reading HTML file: ${err}`);
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end('Internal Server Error: Could not load UI.');
            } else {
                // Serve the HTML file with a 200 OK status
                res.writeHead(200, { 'Content-Type': 'text/html' });
                res.end(data); // Send the HTML content
            }
        });
    } else {
        // For any other requests (e.g., favicon.ico, or other assets if you add them later)
        // For now, we'll just return a 404 Not Found
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not Found');
    }
});

// Start the HTTP server listening on the specified port
httpServer.listen(PORT, () => {
    console.log(`HTTP Server listening on port ${PORT}`);
});

// --- WebSocket Server Setup ---
// Create a new WebSocket server instance, attached to the same HTTP server
const wss = new WebSocketServer({ server: httpServer });

// --- Client Management ---
let esp32Client = null;
const uiClients = new Set();

console.log(`WebSocket Relay Server starting on port ${PORT} (attached to HTTP server)`);
console.log('Waiting for connections...');

// --- WebSocket Server Event Handlers ---
wss.on('connection', function connection(ws, req) {
    const clientIp = req.socket.remoteAddress;
    console.log(`Client connected: ${clientIp}`);

    const urlParams = new URLSearchParams(req.url.split('?')[1]);
    const clientType = urlParams.get('type');

    if (clientType === 'esp32') {
        if (esp32Client && esp32Client.readyState === ws.OPEN) {
            console.warn('New ESP32 connection, closing previous ESP32 client.');
            esp32Client.close(1000, 'New ESP32 connected');
        }
        esp32Client = ws;
        console.log(`ESP32 client connected from ${clientIp}.`);
        uiClients.forEach(client => {
            if (client.readyState === ws.OPEN) {
                client.send(JSON.stringify({ type: 'log', level: 'success', message: 'ESP32 connected to relay.' }));
            }
        });
    } else {
        uiClients.add(ws);
        console.log(`UI client connected from ${clientIp}. Total UI clients: ${uiClients.size}`);
        if (esp32Client && esp32Client.readyState === ws.OPEN) {
            ws.send(JSON.stringify({ type: 'log', level: 'success', message: 'ESP32 is already connected to relay.' }));
        } else {
            ws.send(JSON.stringify({ type: 'log', level: 'warning', message: 'Waiting for ESP32 to connect to relay...' }));
        }
    }

    ws.on('message', function message(data) {
        const messageString = data.toString();
        if (ws === esp32Client) {
            uiClients.forEach(client => {
                if (client.readyState === ws.OPEN) {
                    client.send(messageString);
                }
            });
        } else if (uiClients.has(ws)) {
            if (esp32Client && esp32Client.readyState === ws.OPEN) {
                esp32Client.send(messageString);
            } else {
                ws.send(JSON.stringify({ type: 'error', code: 'ESP32_DISCONNECTED', message: 'ESP32 not connected to relay. Command not sent.' }));
                console.warn('UI sent command, but no ESP32 client is connected.');
            }
        }
    });

    ws.on('close', function close(code, reason) {
        const reasonString = reason.toString();
        console.log(`Client disconnected: ${clientIp} (Code: ${code}, Reason: ${reasonString})`);

        if (ws === esp32Client) {
            esp32Client = null;
            console.log('ESP32 client disconnected.');
            uiClients.forEach(client => {
                if (client.readyState === ws.OPEN) {
                    client.send(JSON.stringify({ type: 'log', level: 'error', message: 'ESP32 disconnected from relay.' }));
                }
            });
        } else if (uiClients.has(ws)) {
            uiClients.delete(ws);
            console.log(`UI client disconnected. Remaining UI clients: ${uiClients.size}`);
        }
    });

    ws.on('error', function error(err) {
        console.error(`WebSocket error for client ${clientIp}:`, err);
    });
});

wss.on('error', function error(err) {
    console.error('WebSocket Server error:', err);
});

// --- Keep-Alive / Ping-Pong ---
const interval = setInterval(function ping() {
    wss.clients.forEach(function each(ws) {
        if (ws.isAlive === false) {
            console.warn('Client not alive, terminating connection.');
            return ws.terminate();
        }
        ws.isAlive = false;
        ws.ping();
    });
}, 30000);

wss.on('close', function close() {
    clearInterval(interval);
});

wss.clients.forEach(function each(ws) {
    ws.isAlive = true;
    ws.on('pong', function heartbeat() {
        this.isAlive = true;
    });
});
