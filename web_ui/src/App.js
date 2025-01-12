import React from 'react';
import { BrowserRouter as Router, Route, Routes, Navigate } from 'react-router-dom';
import HomePage from './pages/HomePage';
import LoginPage from './pages/LoginPage';
import NotificationTableGraph from './notifications/NotificationTableGraph';
import { NotificationsProvider } from './notifications/NotificationsState';

const ProtectedRoute = ({ children }) => {
  const isAuthenticated = sessionStorage.getItem('authenticated'); // Check for authentication flag
  return isAuthenticated ? children : <Navigate to="/login" />;
};

function App() {
  return (
    <NotificationsProvider>
      <Router>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route
            path="/"
            element={
              <ProtectedRoute>
                <HomePage />
              </ProtectedRoute>
            }
          />
          <Route path="/notifications" element={<ProtectedRoute><NotificationTableGraph /></ProtectedRoute>} />
        </Routes>
      </Router>
    </NotificationsProvider>
  );
}

export default App;
