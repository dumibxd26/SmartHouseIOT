import React, { useState } from 'react';
import { useNotifications } from '../notifications/NotificationsState';
import './NotificationBell.css';

const NotificationBell = () => {
  const { notifications, unreadNotifications, markAsRead } = useNotifications();
  const max_visible_notifications = 4;
  const [isOpen, setIsOpen] = useState(false);

  // Toggle dropdown and mark all notifications as read
  const handleOpenNotifications = () => {
    setIsOpen(!isOpen);

    unreadNotifications.forEach((notif) => markAsRead(notif.id));
  };

  return (
    <div className="notification-bell">
      {/* Bell Icon */}
      <div className="bell-icon" onClick={handleOpenNotifications}>
        🛎️
        {unreadNotifications.length > 0 && <span className="badge">{unreadNotifications.length}</span>}
      </div>

      {/* Notification Dropdown */}
      {isOpen && (
        <div className="dropdown">
          <h3>Notifications</h3>
          <ul>
            {/* {notifications.length > 0 ? (
              notifications.map((notif) => (
                <li key={notif.id}>
                  <strong>{notif.message}</strong>
                  <em>{new Date(notif.timestamp).toLocaleString()}</em>
                </li>
              ))
            ) : (
              <li>No notifications</li>
            )} */}

            {notifications.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
              .slice(0, max_visible_notifications).map((notif) => (
              <li key={notif.id}>
                <strong>{notif.message}</strong>
                <em>{new Date(notif.timestamp).toLocaleString()}</em>
              </li>
            ))}
          </ul>
        </div>
      )}
    </div>
  );
};

export default NotificationBell;
