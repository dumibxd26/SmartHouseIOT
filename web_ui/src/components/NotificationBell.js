import React, { useState, useEffect } from 'react';
import { io } from 'socket.io-client';
import './NotificationBell.css';

const SOCKET_URL = 'https://murmuring-citadel-82885-21551507c6aa.herokuapp.com/'; // Replace with your Heroku server URL

function NotificationBell() {
  const [notifications, setNotifications] = useState([]);
  const [unreadCount, setUnreadCount] = useState(0);
  const [isOpen, setIsOpen] = useState(false);

  useEffect(() => {
    // Connect to WebSocket server
    const socket = io(SOCKET_URL);

    // Load notification history on connection
    socket.on('notification-history', (data) => {
      setNotifications(data);
      setUnreadCount(data.length);
    });

    // Listen for new notifications
    socket.on('new-notification', (data) => {
      setNotifications(data);
      setUnreadCount(data.length);
    });

    // Cleanup socket connection on unmount
    return () => {
      socket.disconnect();
    };
  }, []);

  // Mark all notifications as read
  const handleOpenNotifications = () => {
    setIsOpen(!isOpen);
    setUnreadCount(0); // Reset unread count
  };

  return (
    <div className="notification-bell">
      {/* Bell Icon */}
      <div className="bell-icon" onClick={handleOpenNotifications}>
        🛎️
        {unreadCount > 0 && <span className="badge">{unreadCount}</span>}
      </div>

      {/* Notification Dropdown */}
      {isOpen && (
        <div className="dropdown">
          <h3>Notifications</h3>
          <ul>
            {notifications.length > 0 ? (
              notifications.map((notif) => (
                <li key={notif.id}>
                  <strong>{notif.message}</strong>
                  <em>{new Date(notif.timestamp).toLocaleString()}</em>
                </li>
              ))
            ) : (
              <li>No notifications</li>
            )}
          </ul>
        </div>
      )}
    </div>
  );
}

export default NotificationBell;
