const express = require('express');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

const app = express();
const port = 5050;
const jsonFilePath = path.join(__dirname, 'notifications.json');

// Middleware
app.use(express.json()); // Enable JSON parsing
app.use(cors()); // Enable CORS

// Ensure the JSON file exists
if (!fs.existsSync(jsonFilePath)) {
  fs.writeFileSync(jsonFilePath, JSON.stringify([]), 'utf8'); // Create file with an empty array
}

// Helper function to read the JSON file
const readJsonFile = () => {
  try {
    const data = fs.readFileSync(jsonFilePath, 'utf8');
    return JSON.parse(data);
  } catch (error) {
    console.error('Error reading JSON file:', error);
    return [];
  }
};

// Helper function to write to the JSON file
const writeJsonFile = (data) => {
  try {
    fs.writeFileSync(jsonFilePath, JSON.stringify(data, null, 2), 'utf8');
  } catch (error) {
    console.error('Error writing to JSON file:', error);
  }
};

// Routes

// GET: Fetch all notifications
app.get('/notifications', (req, res) => {
  const notifications = readJsonFile();
  res.status(200).json(notifications);
});

// POST: Add a new notification
app.post('/notifications', (req, res) => {
  const newNotification = req.body;

  if (!newNotification.type || !newNotification.message || !newNotification.timestamp) {
    return res.status(400).json({ error: 'Missing required fields: type, message, timestamp' });
  }

  const notifications = readJsonFile();
  notifications.push(newNotification);
  writeJsonFile(notifications);

  console.log('New Notification:', newNotification);

  res.status(201).json({ message: 'Notification added successfully', notification: newNotification });
});

// PUT: Mark a notification as read
app.put('/notifications/:id/read', (req, res) => {
  const id = parseInt(req.params.id, 10);
  const notifications = readJsonFile();

  const notificationIndex = notifications.findIndex((notif) => notif.id === id);
  if (notificationIndex === -1) {
    return res.status(404).json({ error: 'Notification not found' });
  }

  notifications[notificationIndex].unread = false;
  writeJsonFile(notifications);

  res.status(200).json({ message: 'Notification marked as read', notification: notifications[notificationIndex] });
});

// DELETE: Delete a notification
app.delete('/notifications/:id', (req, res) => {
  const id = parseInt(req.params.id, 10);
  const notifications = readJsonFile();

  const updatedNotifications = notifications.filter((notif) => notif.id !== id);

  console.log('Deleted Notification:', notifications.find((notif) => notif.id === id));

  if (notifications.length === updatedNotifications.length) {
    return res.status(404).json({ error: 'Notification not found' });
  }

  writeJsonFile(updatedNotifications);
  res.status(200).json({ message: 'Notification deleted successfully' });
});

// Start the server
app.listen(port, () => {
  console.log(`JSON Database Server running on http://localhost:${port}`);
});
