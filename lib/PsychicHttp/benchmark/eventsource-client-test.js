#!/usr/bin/env node
//stress test the client opening/closing code

const EventSource = require('eventsource');
const url = 'http://psychic.local/events';

async function eventSourceClient() {
  console.log(`Starting test`);
  for (let i = 0; i < 1000000; i++)
  {
    if (i % 100 == 0)
      console.log(`Count: ${i}`);

    let eventSource = new EventSource(url);

    eventSource.onopen = () => {
        //console.log('EventSource connection opened.');
    };
    
    eventSource.onerror = (error) => {
        console.error('EventSource error:', error);
        
        // Close the connection on error
        eventSource.close();
    };

    await new Promise((resolve) => {
        eventSource.onmessage = (event) => {
            //console.log('Received message:', event.data);
        
            // Close the connection after receiving the first message
            eventSource.close();
    
            resolve();
        }
    });  
  }
}

eventSourceClient();