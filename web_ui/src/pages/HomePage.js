import React, { useState, useEffect } from 'react';
import './HomePage.css';
import axios from 'axios';
import NotificationBell from '../components/NotificationBell';
import { useNavigate } from 'react-router-dom';

function HomePage() {
  const [password, setPassword] = useState('');
  const [showModal, setShowModal] = useState(false);
  const [hubStatus, setHubStatus] = useState('Checking...');
  const [esp32camStatus, setEsp32camStatus] = useState('Checking...');
  const [entranceEspStatus, setEntranceEspStatus] = useState('Checking...');
  const [proximityEspStatus, setProximityEspStatus] = useState('Checking...');
  const REQ_ADDRESS = 'https://murmuring-citadel-82885-21551507c6aa.herokuapp.com';

  const navigate = useNavigate();

  const handleSeeHistory = () => {
    navigate('/notifications');
  };

  // Function to check the status of the boards
  const checkStatus = async () => {
    try {
      const response = await axios.get(`${REQ_ADDRESS}/board_statuses`);
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

    return () => clearInterval(interval);
  }, []);

  // Activate alarms
  const handleActivateAlarms = async () => {
    const pass = prompt('Enter Password:');
    
    // make a checkpassword request
    // if it returns true, then activate all alarms
    // else return an error message
    try {
      const response = await axios.post(`${REQ_ADDRESS}/check_password`, { password: pass });
      if (response.status === 200) {
        await axios.post(`${REQ_ADDRESS}/trigger_all_alarms`, { password: pass });
        alert('All alarms activated.');
      } else {
        alert('Incorrect password');
      }
    }
    catch (error) {
      alert('Failed to activate alarms.');
    }
  };


  // Deactivate alarms
  const handleDeactivateAlarms = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    try {
      await axios.post(`${REQ_ADDRESS}/deactivate_all_alarms`, { password: pass });
      alert('All alarms deactivated.');
    } catch (error) {
      alert('Failed to deactivate alarms.');
    }
  };

  // Live Camera
  const handleLiveCamera = () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    window.open(`${REQ_ADDRESS}/live_video`, '_blank');
  };

  // Capture Image
  const handleCaptureImage = async () => {
    const pass = prompt('Enter Password:');
    if (!pass) return;

    try {
      const response = await axios.post(`${REQ_ADDRESS}/capture_image`, { password: pass });
      alert(response.data.message);
    } catch (error) {
      alert('Failed to capture image.');
    }
  };

  return (
    <div className="homepage">
      
      <header style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '20px' }}>
      <h1 style={{ color: 'white' }}>Smart House Dashboard</h1>
        <NotificationBell />
      </header>

      {/* Status Grid */}
      <div className="status-grid">
        <div className={`status-box ${hubStatus === 'Online' ? 'online' : 'offline'}`}>
          <h3>Hub<br/>Status</h3>
          <p>{hubStatus}</p>
        </div>
        <div
          className={`status-box ${esp32camStatus === 'up' ? 'online' : 'offline'}`}
          onClick={() => setShowModal(true)}
          style={{ cursor: 'pointer' }}
        >
          <h3>ESP32-CAM Status</h3>
          <p>{esp32camStatus}</p>
        </div>
        <div className={`status-box ${entranceEspStatus === 'up' ? 'online' : 'offline'}`}>
          <h3>Entrance ESP32 Status</h3>
          <p>{entranceEspStatus}</p>
        </div>
        <div className={`status-box ${proximityEspStatus === 'up' ? 'online' : 'offline'}`}>
          <h3>Proximity ESP32 Status</h3>
          <p>{proximityEspStatus}</p>
        </div>
      </div>

      {/* Activate/Deactivate Alarms */}
      <div className="actions">
        <button className="primary-button" onClick={handleActivateAlarms}>
          Activate All Alarms
        </button>
        <button className="secondary-button" onClick={handleDeactivateAlarms}>
          Deactivate All Alarms
        </button>
        <button onClick={handleSeeHistory} className="close-button">
        See history of alarms
      </button>
      </div>

      {/* Modal for ESP32-CAM */}
      {showModal && (
        <div className="modal">
          <div className="modal-content">
            <h2>ESP32-CAM Actions</h2>
            <button className="primary-button" onClick={handleLiveCamera}>
              Live Camera
            </button>
            <button className="secondary-button" onClick={handleCaptureImage}>
              Capture Image
            </button>
            <button className="close-button" onClick={() => setShowModal(false)}>
              Close
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default HomePage;
