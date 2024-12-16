import React, { useState, useEffect } from 'react';
import './HomePage.css';
import axios from 'axios';

function HomePage() {
  const [password, setPassword] = useState('');
  const [showModal, setShowModal] = useState(false);
  const [hubStatus, setHubStatus] = useState('Checking...');
  const [esp32camStatus, setEsp32camStatus] = useState('Checking...');
  const [entranceEspStatus, setEntranceEspStatus] = useState('Checking...');
  const [proximityEspStatus, setProximityEspStatus] = useState('Checking...');
  const HUB_IP = '192.168.1.100';
  const HUB_PORT = '5000';

  // Function to check the status of the boards
  const checkStatus = async () => {
    try {
      const response = await axios.get(`http://${HUB_IP}:${HUB_PORT}/board_statuses`);
      setHubStatus('Online');
      setEsp32camStatus(response.data.boardStatuses.EntranceCamera.state);
      setEntranceEspStatus(response.data.boardStatuses.FrontDoorESP32.state);
      setProximityEspStatus(response.data.boardStatuses.ProximityBoard.state);
    } catch (error) {
      setHubStatus('Offline');
      setEsp32camStatus('Offline');
      setEntranceEspStatus('Offline');
      setProximityEspStatus('Offline');
    }
  };

  useEffect(() => {
    checkStatus(); 
    const interval = setInterval(() => {
      checkStatus();
    }, 5000); 

    // Cleanup interval on component unmount
    return () => clearInterval(interval);
  }, []);

  // Activate alarms
  const handleActivateAlarms = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    try {
      await axios.post(`http://${HUB_IP}:${HUB_PORT}/trigger_all_alarms`, { password: pass });
      alert('All alarms activated.');
    } catch (error) {
      alert('Failed to activate alarms.');
    }
  };

  // Deactivate alarms
  const handleDeactivateAlarms = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    try {
      await axios.post(`http://${HUB_IP}:${HUB_PORT}/deactivate_all_alarms`, { password: pass });
      alert('All alarms deactivated.');
    } catch (error) {
      alert('Failed to deactivate alarms.');
    }
  };

  // Live Camera
  const handleLiveCamera = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    window.open(`http://${HUB_IP}:${HUB_PORT}/live_video`, '_blank');
  };

  // Capture Image
  const handleCaptureImage = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    try {
      const response = await axios.post(`http://${HUB_IP}:${HUB_PORT}/capture_image`, { password: pass });
      alert(response.data.message);
    } catch (error) {
      alert('Failed to capture image.');
    }
  };

  return (
    <div>
      <h1>Smart House Dashboard</h1>

      {/* Status Boxes */}
      <div style={{ display: 'flex', justifyContent: 'space-around', marginTop: '20px' }}>
        <div className="status-box">
          <h3>Hub Status</h3>
          <p>{hubStatus}</p>
        </div>
        <div className="status-box" onClick={() => setShowModal(true)} style={{ cursor: 'pointer' }}>
          <h3>ESP32-CAM Status</h3>
          <p>{esp32camStatus}</p>
        </div>
        <div className="status-box">
          <h3>Entrance ESP32 Status</h3>
          <p>{entranceEspStatus}</p>
        </div>
        <div className="status-box">
          <h3>Proximity ESP32 Status</h3>
          <p>{proximityEspStatus}</p>
        </div>
      </div>

      {/* Activate/Deactivate Alarms */}
      <div style={{ textAlign: 'center', marginTop: '40px' }}>
        <button onClick={handleActivateAlarms} style={{ margin: '10px', padding: '10px 20px' }}>
          Activate All Alarms
        </button>
        <button onClick={handleDeactivateAlarms} style={{ margin: '10px', padding: '10px 20px' }}>
          Deactivate All Alarms
        </button>
      </div>

      {/* Modal for ESP32-CAM */}
      {showModal && (
        <div className="modal">
          <div className="modal-content">
            <h2>ESP32-CAM Actions</h2>
            <button onClick={handleLiveCamera} style={{ margin: '10px' }}>
              Live Camera
            </button>
            <button onClick={handleCaptureImage} style={{ margin: '10px' }}>
              Capture Image
            </button>
            <button onClick={() => setShowModal(false)} style={{ marginTop: '20px' }}>
              Close
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default HomePage;
