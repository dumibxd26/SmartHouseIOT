import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import axios from 'axios';
import './LoginPage.css';

function LoginPage() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const navigate = useNavigate();

  const HEROKU_URL = 'https://murmuring-citadel-82885-21551507c6aa.herokuapp.com/login'; // Replace with your actual Heroku URL

  const handleLogin = async (e) => {
    e.preventDefault();

    try {
      const response = await axios.post(HEROKU_URL, { username, password });

      if (response.status === 200) {
        console.log(response.data);
        // Navigate to the homepage if access is granted
        sessionStorage.setItem('authenticated', true);
        navigate('/');
      } else {
        setError('Access denied');
      }
    } catch (err) {
      setError('Invalid credentials or server error');
    }
  };

  return (
    <div className="login-container">
      <div className="login-card">
        <h1>Welcome Back</h1>
        <p>Please log in to access your dashboard</p>
        <form onSubmit={handleLogin}>
          <div className="input-group">
            <label htmlFor="username">Username</label>
            <input
              type="text"
              id="username"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              required
            />
          </div>
          <div className="input-group">
            <label htmlFor="password">Password</label>
            <input
              type="password"
              id="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              required
            />
          </div>
          <button type="submit" className="login-button">
            Log In
          </button>
          {error && <p className="error-message">{error}</p>}
        </form>
      </div>
    </div>
  );
}

export default LoginPage;