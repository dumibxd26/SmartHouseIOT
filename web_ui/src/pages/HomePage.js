import React from 'react';
import Navbar from '../components/Navbar';
import Stats from '../components/Stats';

function HomePage() {
  return (
    <div>
      <Navbar />
      <h1>Smart House Dashboard</h1>
      <Stats />
    </div>
  );
}

export default HomePage;
