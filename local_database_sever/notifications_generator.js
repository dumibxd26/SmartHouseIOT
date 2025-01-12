const fs = require('fs');
const path = require('path');

// File path for the notifications.json
const filePath = path.join(__dirname, 'notifications.json');

// Notification types
const notificationTypes = ['movement_event', 'three_wrong_guesses', 'front_door_alarm'];

// Helper function to generate a random timestamp within a specific time range
const generateRandomTimestamp = (start, end) => {
  return new Date(start.getTime() + Math.random() * (end.getTime() - start.getTime())).toISOString();
};

// Helper function to generate notifications for a specific time range
const generateNotificationsForTimeRange = (count, type, start, end) => {
  const notifications = [];
  for (let i = 0; i < count; i++) {
    notifications.push({
      id: Date.now() + i, // Unique ID
      type,
      message: `This is a ${type} notification.`,
      timestamp: generateRandomTimestamp(start, end),
      unread: false,
    });
  }
  return notifications;
};

// Read existing notifications
const oldNotifications = fs.existsSync(filePath)
  ? JSON.parse(fs.readFileSync(filePath, 'utf8'))
  : [];
const oldNotificationsCount = oldNotifications.length;

// Generate notifications for the last year
const now = new Date();
const lastYear = new Date(now.getFullYear() - 1, now.getMonth(), now.getDate());
const lastMonth = new Date(now.getFullYear(), now.getMonth() - 1, now.getDate());
const lastDay = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1);
const lastHour = new Date(now.getFullYear(), now.getMonth(), now.getDate(), now.getHours() - 1);

// Generate data for each time range
const notifications = [
  ...generateNotificationsForTimeRange(500, 'movement_event', lastYear, now), // Year
  ...generateNotificationsForTimeRange(200, 'three_wrong_guesses', lastMonth, now), // Month
  ...generateNotificationsForTimeRange(100, 'front_door_alarm', lastDay, now), // Day
  ...generateNotificationsForTimeRange(50, 'movement_event', lastHour, now), // Hour
];

// Merge with existing notifications
const updatedNotifications = [...oldNotifications, ...notifications];

// Write to the JSON file
fs.writeFileSync(filePath, JSON.stringify(updatedNotifications, null, 2), 'utf8');

console.log(`Added ${notifications.length} notifications. Total: ${updatedNotifications.length} notifications.`);
