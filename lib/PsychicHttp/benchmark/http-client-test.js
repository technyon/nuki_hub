#!/usr/bin/env node
//stress test the http request code

const axios = require('axios');

const url = 'http://psychic.local/api';
const queryParams = {
  foo: 'bar',
  foo1: 'bar',
  foo2: 'bar',
  foo3: 'bar',
  foo4: 'bar',
  foo5: 'bar',
  foo6: 'bar',
};

const totalRequests = 1000000;
const requestsPerCount = 100;

let requestCount = 0;

function fetchData() {
  axios.get(url, { params: queryParams })
    .then(response => {
      requestCount++;

      if (requestCount % requestsPerCount === 0) {
        console.log(`Requests completed: ${requestCount}`);
      }

      if (requestCount < totalRequests) {
        fetchData();
      } else {
        console.log('All requests completed.');
      }
    })
    .catch(error => {
      console.error('Error making request:', error.message);
    });
}

// Start making requests
console.log(`Starting test`);
fetchData();