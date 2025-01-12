const express = require('express');
const bodyParser = require('body-parser');
const fs = require('fs');
const path = require('path');
const axios = require('axios');
const morgan = require('morgan');
const cors = require('cors');
const { Server } = require('socket.io');
const http = require('http');

const app = express();
const port = process.env.PORT || 5000;

// Middleware
app.use(bodyParser.json());
app.use(morgan('combined'));
app.use(cors());

// Create an HTTP server and bind it to the express app
const server = http.createServer(app);

// Attach Socket.IO to the server
const io = new Server(server, {
cors: {
    origin: '*', // Allow all origins (adjust this for production)
    methods: ['GET', 'POST']
}
});

app.use(bodyParser.json());

const generateNotification = (type, message) => ({
    id: Date.now(),
    type,
    message,
    timestamp: new Date().toISOString(),
  });


// Configurations

// In-Memory Data Structures
const knownBoards = {
    "EntranceCamera": null,    // ESP32-CAM for entrance
    "FrontDoorESP32": null,    // ESP32 for entrance task
    "ProximityBoard": null     // ESP32 for proximity detection
};

const boardStatus = {
    "EntranceCamera": { state: "down", failedPings: 0 },
    "FrontDoorESP32": { state: "down", failedPings: 0 },
    "ProximityBoard": { state: "down", failedPings: 0 }
};

const boards_with_alarms = ["FrontDoorESP32", "ProximityBoard"];

// Timeout configuration
const HEARTBEAT_INTERVAL = 10000; // 10 seconds
const MAX_FAILED_PINGS = 2;

const notificationQueue = [];      // Queue to store notifications for the web app

// Ensure the images folder exists
const imagesDir = path.join(__dirname, 'images');
if (!fs.existsSync(imagesDir)) {
    fs.mkdirSync(imagesDir);
}

// HTTP Routes

// Dummy credentials for authentication
const USERNAME = 'admin';
const PASSWORD = 'admin';

// Login route
app.post('/login', (req, res) => {
  const { username, password } = req.body;

  // Check if credentials match
  if (username === USERNAME && password === PASSWORD) {
    return res.status(200).json({ status: 'access', message: 'Login successful' });
  } else {
    return res.status(401).json({ status: 'denied', message: 'Invalid credentials' });
  }
});

app.post('/check_password', (req, res) => {
    const { password } = req.body;

    // Check if password matches
    if (password === PASSWORD) {
        return res.status(200).json({ status: 'success', message: 'Password is correct' });
    } else {
        return res.status(401).json({ status: 'failure', message: 'Incorrect password' });
    }
});

// Helper function to send heartbeat
const checkBoardState = async (boardName, boardIP) => {
    try {
        const response = await axios.get(`http://${boardIP}/test`);
        if (response.status === 200) {
            console.log(`${boardName} is up`);
            boardStatus[boardName].state = "up";
            boardStatus[boardName].failedPings = 0; // Reset failed ping count
        }
    } catch (error) {
        console.error(`${boardName} is down or unreachable: ${error.message}`);
        boardStatus[boardName].failedPings += 1;

        if (boardStatus[boardName].failedPings >= MAX_FAILED_PINGS) {
            boardStatus[boardName].state = "down";
        }
    }
};

// Function to start the heartbeat
const startHeartbeat = () => {
    setInterval(() => {
        Object.keys(knownBoards).forEach((boardName) => {
            const boardIP = knownBoards[boardName];
            if (boardIP) {
                checkBoardState(boardName, boardIP);
            }
        });
    }, HEARTBEAT_INTERVAL);
};

// Endpoint to get board statuses
app.get('/board_statuses', (req, res) => {
    res.status(200).json({
        status: "success",
        boardStatuses: boardStatus
    });
});

// test function to test hub connection

app.get('/test', (req, res) => {
    res.status(200).json({
        status: "success",
        message: "Hub is up"
    });
});

// Register a board
app.post('/register', (req, res) => {
    const { name, ip } = req.body;

    if (name in knownBoards) {
        knownBoards[name] = ip;
        res.status(200).json({
            status: "success",
            message: `${name} registered with IP ${ip}`
        });
    } else {
        res.status(400).json({
            status: "failure",
            message: "Unknown board name"
        });
    }
});

// trigger all alarms
app.post('/trigger_all_alarms', async (req, res) => {
    const { password } = req.body;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    const results = {
        success: [],
        failures: []
    };

    for (const board of boards_with_alarms) {
        if (!knownBoards[board] || !boardStatus[board].state === "up") {
            console.error(`${board} is not registered or reachable.`);
            results.failures.push({
                board,
                error: `${board} is not registered or reachable.`,
            });
            continue; // Skip this board
        }

        try {
            const boardIp = knownBoards[board];
            const response = await axios.post(`http://${boardIp}/activate_alarm`, {
                action: "activate"
            });

            console.log(`Alarm triggered on ${board}: ${response.data.message}`);
            results.success.push({
                board,
                message: response.data.message,
            });
        } catch (error) {
            console.error(`Error triggering alarm on ${board}:`, error.message);
            results.failures.push({
                board,
                error: error.message,
            });
        }
    }

    // Respond with results
    if (results.failures.length > 0) {
        return res.status(207).json({
            status: "partial_success",
            message: "Some alarms could not be triggered.",
            results,
        });
    }

    res.status(200).json({
        status: "success",
        message: "All alarms triggered successfully.",
        results,
    });
});

app.post('/deactivate_all_alarms', async (req, res) => {
    const { password } = req.body;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    const results = {
        success: [],
        failures: []
    };

    for (const board of boards_with_alarms) {
        if (!knownBoards[board] || !boardStatus[board].state === "up") {
            console.error(`${board} is not registered or reachable.`);
            results.failures.push({
                board,
                error: `${board} is not registered or reachable.`,
            });
            continue; // Skip this board
        }

        try {
            const boardIp = knownBoards[board];
            const response = await axios.post(`http://${boardIp}/deactivate_alarm`, {
                action: "deactivate"
            });

            console.log(`Alarm deactivated on ${board}: ${response.data.message}`);
            results.success.push({
                board,
                message: response.data.message,
            });
        } catch (error) {
            console.error(`Error deactivating alarm on ${board}:`, error.message);
            results.failures.push({
                board,
                error: error.message,
            });
        }
    }

    // Respond with results
    if (results.failures.length > 0) {
        return res.status(207).json({
            status: "partial_success",
            message: "Some alarms could not be deactivated.",
            results,
        });
    }

    res.status(200).json({
        status: "success",
        message: "All alarms deactivated successfully.",
        results,
    });
});

// Get Notifications (For the web app to retrieve alarm notifications)
app.get('/notifications', (req, res) => {
    res.status(200).json({
        status: "success",
        notifications: notificationQueue,
    });

    // Clear the queue after sending
    // notificationQueue.length = 0;
});

// Trigger alarms on a specific board via HTTP requests
app.post('/trigger_alarm', async (req, res) => {
    const { board, password } = req.body;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    if (!knownBoards[board] || !knownBoards[board]) {
        return res.status(400).json({
            status: "failure",
            message: `${board} is not registered or reachable.`,
        });
    }

    try {
        const boardIp = knownBoards[board];
        const response = await axios.post(`http://${boardIp}/trigger_alarm`, {
            action: "activate"
        });

        res.status(200).json({
            status: "success",
            message: `Alarm triggered on ${board}: ${response.data.message}`,
        });
    } catch (error) {
        console.error(`Error triggering alarm on ${board}:`, error.message);
        res.status(500).json({
            status: "failure",
            message: `Failed to trigger alarm on ${board}`,
        });
    }
});

// Live Video Access (HTTP Route)
app.get('/live_video', async (req, res) => {
    if (!knownBoards["EntranceCamera"]) {
        return res.status(400).json({
            status: "failure",
            message: "EntranceCamera not registered",
        });
    }

    const ip = knownBoards["EntranceCamera"];
    const esp32StreamUrl = `http://${ip}/live_video`;

    console.log('Forwarding video stream from ESP32-CAM:', esp32StreamUrl);

    try {
        const response = await axios.get(esp32StreamUrl, { responseType: 'stream' });

        // Forward necessary headers only
        res.setHeader('Content-Type', response.headers['content-type']);
        // Don't forward Content-Length if it's undefined
        if (response.headers['content-length']) {
            res.setHeader('Content-Length', response.headers['content-length']);
        }

        // Forward the stream
        response.data.pipe(res);
    } catch (err) {
        console.error('Error connecting to ESP32-CAM:', err.message);
        res.status(500).json({
            status: "failure",
            message: "Failed to fetch live video from ESP32-CAM",
        });
    }
});


// Capture Image from Camera
app.post('/capture_image', async (req, res) => {
    const { password } = req.body;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    if (!knownBoards["EntranceCamera"]) {
        return res.status(400).json({
            status: "failure",
            message: "EntranceCamera not registered",
        });
    }

    try {
        const ip = knownBoards["EntranceCamera"];
        const response = await axios.get(`http://${ip}/capture`, { responseType: 'arraybuffer' });

        const fileName = `image_${Date.now()}.jpg`;
        const filePath = path.join(imagesDir, fileName);
        fs.writeFileSync(filePath, response.data);

        res.status(200).json({
            status: "success",
            message: `Image captured and saved as ${fileName}`,
        });
    } catch (error) {
        console.error('Error capturing image:', error.message);
        res.status(500).json({
            status: "failure",
            message: "Failed to capture image from camera",
        });
    }
});

// adds a notification to the queue
app.post('/front_door_alarm', async (req, res) => {
    const notification = generateNotification('front_door_alarm', 'Front Door Alarm Triggered');
    io.emit('new-notification', notification);

    res.status(200).json({
        status: "success",
        message: "Front Door Alarm Triggered",
    });

});

// movement event added to the queue
app.post('/movement_event', async (req, res) => {

//    distance obtain from the proximity sensor
    const { distance } = req.body;
    const notification = generateNotification(
        'movement_event',
        `Movement Detected: ${distance} cm`
      );
    io.emit('new-notification', notification);

    res.status(200).json({
        status: "success",
        message: "Movement Detected",
    });
});

// three_wrong_guesses activates all alarms
// frontdoor alarm in this case because the proximity alarm is already activated
app.post ('/three_wrong_guesses', async (req, res) => {
    const notification = generateNotification(
        'three_wrong_guesses',
        'Proximity sensor three wrong guesses'
      );
    io.emit('new-notification', notification);

    // Trigger the front door alarm
    const board = "FrontDoorESP32";

    if (!knownBoards[board] || !boardStatus[board].state === "up") {
        return res.status(400).json({
            status: "failure",
            message: `${board} is not registered or reachable.`,
        });
    }

    try {
        const boardIp = knownBoards[board];
        const response = await axios.post(`http://${boardIp}/trigger_alarm`, {
            action: "activate"
        });

        res.status(200).json({
            status: "success",
            message: `Alarm triggered on ${board}: ${response.data.message}`,
        });
    } catch (error) {
        console.error(`Error triggering alarm on ${board}:`, error.message);
        res.status(500).json({
            status: "failure",
            message: `Failed to trigger alarm on ${board}`,
        });
    }

    res.status(200).json({
        status: "success",
        message: "Three Wrong Guesses",
    });
});

// Start the server
server.listen(port, '0.0.0.0', () => {
    console.log(`Server running on http://0.0.0.0:${port}`);
    startHeartbeat();
});

