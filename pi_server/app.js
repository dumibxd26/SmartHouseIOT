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

// Global Variables
let send_activate_all_alarms = false;
let send_deactivate_all_alamrs = false;

// Create an HTTP server and bind it to the express app
const server = http.createServer(app);

app.use(bodyParser.json());

// Attach Socket.IO to the server
const io = new Server(server, {
    cors: {
      origin: '*', // Allow all origins (adjust this for production)
      methods: ['GET', 'POST']
    }
  });

// Configurations

// In-Memory Data Structures
const knownBoards = {
    "EntranceCamera": null,    // ESP32-CAM for entrance
    "FrontDoorESP32": null,    // ESP32 for entrance task
    "ProximityBoard": null     // ESP32 for proximity detection
};

const boardStatus = {
    "EntranceCamera": { state: "down", lastUpdate: null},
    "FrontDoorESP32": { state: "down", lastUpdate: null},
    "ProximityBoard": { state: "down", lastUpdate: null}
};

const boards_with_alarms = ["FrontDoorESP32", "ProximityBoard"];

// Timeout configuration
const HEARTBEAT_INTERVAL = 5000; // 10 seconds
const MAX_FAILED_PINGS = 2;

const notificationQueue = [];      // Queue to store notifications for the web app

// Ensure the images folder exists
const imagesDir = path.join(__dirname, 'images');
if (!fs.existsSync(imagesDir)) {
    fs.mkdirSync(imagesDir);
}

const getCurrentTimestamp = () => new Date().toISOString();

// HTTP Routes

// Dummy credentials for authentication
const USERNAME = 'admin';
const PASSWORD = 'admin';

const generateNotification = (type, message) => ({
    id: Date.now(),
    type,
    message,
    timestamp: new Date().toISOString(),
  });

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

// Endpoint to get board statuses
app.get('/board_statuses', (req, res) => {
    // sets board_statues to down if no update for 5 seconds
    Object.keys(boardStatus).forEach((boardName) => {
        if (boardStatus[boardName].lastUpdate) {
            const lastUpdate = new Date(boardStatus[boardName].lastUpdate);
            const now = new Date();
            const diff = now - lastUpdate;
            if (diff > 5000) {
                boardStatus[boardName].state = "down";
                boardStatus[boardName].lastUpdate = null;
                knownBoards[boardName] = null;
            } else 
                boardStatus[boardName].state = "up";
            }
        }
    );

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


// Get Notifications (For the web app to retrieve alarm notifications)
app.get('/notifications', (req, res) => {
    res.status(200).json({
        status: "success",
        notifications: notificationQueue,
    });

    // Clear the queue after sending
    // notificationQueue.length = 0;
});

app.get("/activate_alarms", (req, res) => {

    // Activate alarms on all boards with alarms
    if (send_deactivate_all_alamrs) {
        send_deactivate_all_alamrs = false;
    }
    send_activate_all_alarms = true;

    res.status(200).json({
        status: "success",
        message: "All alarms activated",
    });
});

app.get("/deactivate_alarms", (req, res) => {
    
    // Deactivate alarms on all boards with alarms
    if (send_activate_all_alarms) {
        send_activate_all_alarms = false;
    }
    send_deactivate_all_alamrs = true;

    res.status(200).json({
        status: "success",
        message: "All alarms deactivated",
    });
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

    if (send_deactivate_all_alamrs) {
        send_deactivate_all_alamrs = false;
    }

    send_activate_all_alarms = true;

    res.status(200).json({
        status: "success",
        message: "Three Wrong Guesses",
    });
});

app.post('/send_status', (req, res) => {
    const { name } = req.body;

    console.log(`Received status from ${name}`);

    if (!boardStatus[name]) {
        return res.status(400).json({ status: 'failure', message: 'Unknown board name' });
    }

    boardStatus[name].lastUpdate = getCurrentTimestamp();

    // Determine response based on global alarm states
    if (name === 'ProximityBoard') {
        boardStatus[name].lastUpdate = getCurrentTimestamp();
        if (send_activate_all_alarms) {
            send_activate_all_alarms = false;
            return res.status(203).json({ command: 'activate_alarm' });
        }
        else if (send_deactivate_all_alamrs) {
            send_deactivate_all_alamrs = false;
            return res.status(204).json({ command: 'deactivate_alarm' });
        }
        else {
            return res.status(202).json({ command: 'no_command' });
        }

    } else if (name === 'FrontDoorESP32') {
        boardStatus[name].lastUpdate = getCurrentTimestamp();
        if (send_activate_all_alarms) {
            send_activate_all_alarms = false;
            return res.status(203).json({ command: 'activate_alarm' });
        }
        else if (send_deactivate_all_alamrs) {
            send_deactivate_all_alamrs = false;
            return res.status(204).json({ command: 'deactivate_alarm' });
        }
        else {
            return res.status(202).json({ command: 'no_command' });
        }
    } else {
        // camera
        boardStatus[name].lastUpdate = getCurrentTimestamp();
    }
    return res.status(202).json({ command: 'no_command' });
});

app.get('/get_esp_camera_address', (req, res) => {
    // get address if available
    if (knownBoards["EntranceCamera"]) {
        return res.status(200).json({ address: knownBoards["EntranceCamera"] });
    }
    return res.status(400).json({ address: null });
});


// Start the server
server.listen(port, '0.0.0.0', () => {
    console.log(`Server running on http://0.0.0.0:${port}`);
});

