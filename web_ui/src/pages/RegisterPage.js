import React from 'react';
import { useNavigate } from 'react-router-dom';
import RegisterForm from '../components/RegisterForm';
import { register } from '../utils/auth';

function RegisterPage() {
  const navigate = useNavigate();

  const handleRegister = async (credentials) => {
    try {
      await register(credentials.email, credentials.password);
      alert('Registration successful. Please log in.');
      navigate('/login');
    } catch (error) {
      alert('Registration failed. Please try again.');
    }
  };

  return (
    <div>
      <h2>Register</h2>
      <RegisterForm onRegister={handleRegister} />
    </div>
  );
}

export default RegisterPage;
