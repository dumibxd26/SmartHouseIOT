import React, { useState } from 'react';
import { useNotifications } from './NotificationsState';
import { Line } from 'react-chartjs-2';
import { Chart as ChartJS, CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend } from 'chart.js';
import './NotificationTableGraph.css';

ChartJS.register(CategoryScale, LinearScale, PointElement, LineElement, Title, Tooltip, Legend);

const NotificationTableGraph = () => {
  const { notifications } = useNotifications();
  const [viewMode, setViewMode] = useState('table');
  const [currentPage, setCurrentPage] = useState(1);
  const [timeRange, setTimeRange] = useState('lastHour');
  const itemsPerPage = 10;

  // Pagination logic
  const totalPages = Math.ceil(notifications.length / itemsPerPage);
  const startIndex = (currentPage - 1) * itemsPerPage;
  const endIndex = startIndex + itemsPerPage;
  const paginatedNotifications = notifications.slice(startIndex, endIndex);

  const handlePageChange = (direction) => {
    if (direction === 'prev' && currentPage > 1) {
      setCurrentPage(currentPage - 1);
    } else if (direction === 'next' && currentPage < totalPages) {
      setCurrentPage(currentPage + 1);
    }
  };

  const filterNotifications = (timeRange) => {
    const now = new Date();
    const filteredData = {
      movement_event: {},
      three_wrong_guesses: {},
      front_door_alarm: {},
    };

    const isWithinTimeRange = (timestamp) => {
      const eventTime = new Date(timestamp);

      switch (timeRange) {
        case 'lastHour':
          return now - eventTime <= 60 * 60 * 1000; // Last 1 hour
        case 'lastDay':
          return now - eventTime <= 24 * 60 * 60 * 1000; // Last 24 hours
        case 'lastMonth':
          const oneMonthAgo = new Date();
          oneMonthAgo.setMonth(now.getMonth() - 1);
          return eventTime >= oneMonthAgo;
        case 'lastYear':
          const oneYearAgo = new Date();
          oneYearAgo.setFullYear(now.getFullYear() - 1);
          return eventTime >= oneYearAgo;
        default:
          return false;
      }
    };

    notifications
      .filter((notif) => isWithinTimeRange(notif.timestamp))
      .forEach((notif) => {
        const eventTime = new Date(notif.timestamp);
        let key;
        switch (timeRange) {
          case 'lastHour':
            key = eventTime.toISOString().substring(14, 16); // Minutes
            break;
          case 'lastDay':
            key = eventTime.toISOString().substring(11, 13); // Hours
            break;
          case 'lastMonth':
            key = eventTime.toISOString().substring(8, 10); // Days
            break;
          case 'lastYear':
            key = eventTime.toISOString().substring(5, 7); // Months
            break;
          default:
            key = '';
        }

        filteredData[notif.type][key] = (filteredData[notif.type][key] || 0) + 1;
      });

    return filteredData;
  };

  const getGraphData = () => {
    const filteredData = filterNotifications(timeRange);

    const datasets = Object.keys(filteredData).map((type) => {
      const labels = Object.keys(filteredData[type]).sort();
      const data = labels.map((label) => filteredData[type][label]);

      const colors = {
        movement_event: { background: 'rgba(75, 192, 192, 0.5)', border: 'rgba(75, 192, 192, 1)' },
        three_wrong_guesses: { background: 'rgba(255, 205, 86, 0.5)', border: 'rgba(255, 205, 86, 1)' },
        front_door_alarm: { background: 'rgba(255, 99, 132, 0.5)', border: 'rgba(255, 99, 132, 1)' },
      };

      return {
        label: `${type.replace('_', ' ').toUpperCase()} (${timeRange})`,
        data,
        backgroundColor: colors[type].background,
        borderColor: colors[type].border,
      };
    });

    const labels = Object.keys(filteredData.movement_event).sort(); // Use movement_event labels for X-axis

    return {
      labels,
      datasets,
    };
  };

  return (
    <div className="notification-container">
      <h1 className="notification-title">Notifications Dashboard</h1>
      <div className="button-container">
        <button className="view-button" onClick={() => setViewMode('table')}>
          Table View
        </button>
        <button className="view-button" onClick={() => setViewMode('graph')}>
          Graph View
        </button>
        <button className="filter-button" onClick={() => setTimeRange('lastHour')}>
          Last Hour
        </button>
        <button className="filter-button" onClick={() => setTimeRange('lastDay')}>
          Last Day
        </button>
        <button className="filter-button" onClick={() => setTimeRange('lastMonth')}>
          Last Month
        </button>
        <button className="filter-button" onClick={() => setTimeRange('lastYear')}>
          Last Year
        </button>
      </div>
      {viewMode === 'table' ? (
        <div>
          <table className="notification-table">
            <thead>
              <tr>
                <th>Type</th>
                <th>Message</th>
                <th>Timestamp</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              {paginatedNotifications.map((notif) => (
                <tr key={notif.id} className={notif.unread ? 'unread-row' : ''}>
                  <td>{notif.type}</td>
                  <td>{notif.message}</td>
                  <td>{new Date(notif.timestamp).toLocaleString()}</td>
                  <td>{notif.unread ? 'Unread' : 'Read'}</td>
                </tr>
              ))}
            </tbody>
          </table>
          <div className="pagination-controls">
            <button
              className="pagination-button"
              onClick={() => handlePageChange('prev')}
              disabled={currentPage === 1}
            >
              Previous
            </button>
            <span className="pagination-info">
              Page {currentPage} of {totalPages}
            </span>
            <button
              className="pagination-button"
              onClick={() => handlePageChange('next')}
              disabled={currentPage === totalPages}
            >
              Next
            </button>
          </div>
        </div>
      ) : (
        <div className="graph-container">
          <Line data={getGraphData()} />
        </div>
      )}
    </div>
  );
};

export default NotificationTableGraph;
