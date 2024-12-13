import React, { useState, useEffect } from 'react';

function Stats() {
  const [stats, setStats] = useState({
    alerts: 0,
    fingerprints: 0,
    temperature: 0,
  });

  useEffect(() => {
    // Mock API call
    const fetchStats = async () => {
      const data = {
        alerts: 5,
        fingerprints: 12,
        temperature: 22.5,
      };
      setStats(data);
    };

    fetchStats();
  }, []);

  return (
    <div>
      <h2>House Stats</h2>
      <p>Alerts (last 24h): {stats.alerts}</p>
      <p>Fingerprints saved: {stats.fingerprints}</p>
      <p>Current temperature: {stats.temperature}°C</p>
    </div>
  );
}

export default Stats;
