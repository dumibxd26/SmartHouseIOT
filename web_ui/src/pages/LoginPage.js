// import React from 'react';
// import { useNavigate } from 'react-router-dom';
// import LoginForm from '../components/LoginForm';
// import { login } from '../utils/auth';

// function LoginPage() {
//   const navigate = useNavigate();

//   const handleLogin = async (credentials) => {
//     try {
//       await login(credentials.email, credentials.password);
//       navigate('/');
//     } catch (error) {
//       alert('Login failed. Please check your credentials.');
//     }
//   };

//   return (
//     <div>
//       <h2>Login</h2>
//       <LoginForm onLogin={handleLogin} />
//     </div>
//   );
// }

// export default LoginPage;
