import axios from 'axios';

// API Base URL
const API_BASE = 'https://your-api-url.com'; // Replace with your API base URL

// Utility functions
export const isAuthenticated = () => {
  const accessToken = localStorage.getItem('accessToken');
  return !!accessToken;
};

export const login = async (email, password) => {
  try {
    const response = await axios.post(`${API_BASE}/auth/login`, {
      email,
      password,
    });
    const { accessToken, refreshToken } = response.data;

    // Store tokens
    localStorage.setItem('accessToken', accessToken);
    localStorage.setItem('refreshToken', refreshToken);

    return response.data;
  } catch (error) {
    console.error('Login failed:', error);
    throw error;
  }
};

export const register = async (email, password) => {
  try {
    const response = await axios.post(`${API_BASE}/auth/register`, {
      email,
      password,
    });
    return response.data;
  } catch (error) {
    console.error('Registration failed:', error);
    throw error;
  }
};

export const logout = () => {
  localStorage.removeItem('accessToken');
  localStorage.removeItem('refreshToken');
};

export const refreshAccessToken = async () => {
  try {
    const refreshToken = localStorage.getItem('refreshToken');
    if (!refreshToken) throw new Error('No refresh token found');

    const response = await axios.post(`${API_BASE}/auth/refresh-token`, {
      refreshToken,
    });

    const { accessToken } = response.data;
    localStorage.setItem('accessToken', accessToken);

    return accessToken;
  } catch (error) {
    console.error('Failed to refresh access token:', error);
    logout();
    throw error;
  }
};

// Axios interceptor to handle token expiration
axios.interceptors.response.use(
  (response) => response,
  async (error) => {
    const originalRequest = error.config;

    if (
      error.response &&
      error.response.status === 401 &&
      !originalRequest._retry
    ) {
      originalRequest._retry = true;
      try {
        const newAccessToken = await refreshAccessToken();
        originalRequest.headers['Authorization'] = `Bearer ${newAccessToken}`;
        return axios(originalRequest);
      } catch (err) {
        logout();
        window.location.href = '/login'; // Redirect to login on failure
        return Promise.reject(err);
      }
    }

    return Promise.reject(error);
  }
);

export const fetchWithAuth = async (url, options = {}) => {
  const accessToken = localStorage.getItem('accessToken');

  const headers = {
    ...options.headers,
    Authorization: `Bearer ${accessToken}`,
  };

  return axios({
    ...options,
    url: `${API_BASE}${url}`,
    headers,
  });
};
