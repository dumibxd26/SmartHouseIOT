const express = require('express');
const bodyParser = require('body-parser');
const mqtt = require('mqtt');
const fs = require('fs');
const path = require('path');

const app = express();
const port = 5000;

// Middleware
app.use(bodyParser.json());

// MQTT Broker Connection
const mqttClient = mqtt.connect('mqtt://localhost');

const morgan = require('morgan');

// Use 'combined' format for detailed logs or 'dev' for concise logs
app.use(morgan('combined'));

// Configurations
const PASSWORD = "secure_password"; // Change this to your secure password

// In-Memory Data Structures
const knownBoards = {
    "EntranceCamera": null,    // ESP32-CAM for entrance
    "FrontDoorESP32": null,    // ESP32 for entrance tasks
    "ProximityBoard": null     // ESP32 for proximity detection
};

const notificationQueue = [];      // Queue to store notifications for the web app

// Ensure the images folder exists
const imagesDir = path.join(__dirname, 'images');
if (!fs.existsSync(imagesDir)) {
    fs.mkdirSync(imagesDir);
}

// Subscribe to relevant MQTT topics
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');

    mqttClient.subscribe('front_alarm');
    mqttClient.subscribe('sensor_alarm');
    mqttClient.subscribe('all_alarms');
    mqttClient.subscribe('capture_image');
});

mqttClient.on('message', (topic, message) => {
    const msg = message.toString();
    console.log(`Received message on topic ${topic}: ${msg}`);

    switch (topic) {
        case 'front_alarm':
            notificationQueue.push({ type: 'front_alarm', message: msg });
            break;
        case 'sensor_alarm':
            notificationQueue.push({ type: 'sensor_alarm', message: msg });
            break;
        case 'all_alarms':
            notificationQueue.push({ type: 'all_alarms', message: 'Critical alarm triggered!' });
            break;
        case 'capture_image':
            handleImageCapture(msg);
            break;
        default:
            console.log('Unknown topic received');
    }
});

// Function to handle image capture
function handleImageCapture(msg) {
    const imageData = Buffer.from(msg, 'base64');
    const fileName = `image_${Date.now()}.jpg`;
    const filePath = path.join(imagesDir, fileName);

    fs.writeFileSync(filePath, imageData);
    console.log(`Image saved: ${filePath}`);
}

// HTTP Routes

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
})

app.get('/test', (req, res) => {
    res.status(200).json({
        status: "success",
        message: "Server is running"
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

// Live Video Access (HTTP Route)
app.get('/live_video', async (req, res) => {
    const { password } = req.query;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    // Check if the ESP32-CAM is registered
    if (!knownBoards["EntranceCamera"]) {
        return res.status(400).json({
            status: "failure",
            message: "EntranceCamera not registered",
        });
    }

    const ip = knownBoards["EntranceCamera"];
    const esp32StreamUrl = `http://${ip}/live_video`; // Replace 81 with your ESP32-CAM's video port

    // Forward the video stream
    try {
        // Make a request to the ESP32-CAM
        http.get(esp32StreamUrl, (streamRes) => {
            // Set response headers to match the ESP32-CAM response
            res.writeHead(streamRes.statusCode, {
                'Content-Type': streamRes.headers['content-type'],
                'Content-Length': streamRes.headers['content-length'],
            });

            // Pipe the video stream directly to the client
            streamRes.pipe(res);
        }).on('error', (err) => {
            console.error('Error connecting to ESP32-CAM:', err.message);
            res.status(500).json({
                status: "failure",
                message: "Failed to fetch live video from ESP32-CAM",
            });
        });
    } catch (err) {
        console.error('Unexpected error:', err.message);
        res.status(500).json({
            status: "failure",
            message: "Unexpected server error",
        });
    }
});


// Activate All Alarms
app.post('/activate_alarms', (req, res) => {
    const { password } = req.body;

    // Verify password
    if (password !== PASSWORD) {
        return res.status(401).json({
            status: "failure",
            message: "Unauthorized: Invalid password",
        });
    }

    // Publish MQTT messages to trigger all alarms
    mqttClient.publish('all_alarms', JSON.stringify({ action: 'activate' }));

    res.status(200).json({
        status: "success",
        message: "All alarms activated",
    });
});

// Start the server
app.listen(port, '0.0.0.0', () => {
    console.log(`Server running on http://0.0.0.0:${port}`);
});

