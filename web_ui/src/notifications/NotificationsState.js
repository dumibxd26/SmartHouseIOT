import React, { createContext, useState, useEffect, useContext } from 'react';
import { io } from 'socket.io-client';

const SOCKET_URL = 'https://murmuring-citadel-82885-21551507c6aa.herokuapp.com/';
const DATABASE_URL = 'http://localhost:5050/notifications';
const NotificationsContext = createContext();

export const NotificationsProvider = ({ children }) => {
  const [notifications, setNotifications] = useState([]);
  const [unreadNotifications, setUnreadNotifications] = useState([]);

  // Fetch notifications from the database server on startup
  useEffect(() => {
    const fetchNotificationsFromDatabase = async () => {
      try {
        const response = await fetch(DATABASE_URL);
        if (response.ok) {
          const data = await response.json();
          setNotifications(data);
          setUnreadNotifications(data.filter((notif) => notif.unread));
        } else {
          console.error('Failed to fetch notifications from the database server');
        }
      } catch (error) {
        console.error('Error fetching notifications:', error);
      }
    };

    fetchNotificationsFromDatabase();
  }, []);

  // Save a new notification to the database server
  const saveNotificationToDatabase = async (notification) => {
    try {
      await fetch(DATABASE_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(notification),
      });
    } catch (error) {
      console.error('Error saving notification to database:', error);
    }
  };

  // Handle incoming notifications from the Heroku WebSocket server
  useEffect(() => {
    const socket = io(SOCKET_URL);

    socket.on('new-notification', (notification) => {
      console.log('New Notification from WebSocket:', notification);

      // Save the new notification to the database server
      saveNotificationToDatabase(notification);

      // Update local state
      setNotifications((prev) => [...prev, notification]);
      setUnreadNotifications((prev) => [...prev, notification]);
    });

    return () => socket.disconnect();
  }, []);

  // Mark a notification as read
  const markAsRead = (id) => {
    setUnreadNotifications((prev) => prev.filter((notif) => notif.id !== id));
    setNotifications((prev) =>
      prev.map((notif) => (notif.id === id ? { ...notif, unread: false } : notif))
    );
  };

  return (
    <NotificationsContext.Provider value={{ notifications, unreadNotifications, markAsRead }}>
      {children}
    </NotificationsContext.Provider>
  );
};

export const useNotifications = () => useContext(NotificationsContext);
