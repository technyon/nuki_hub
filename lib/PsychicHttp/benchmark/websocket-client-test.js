#!/usr/bin/env node
//stress test the client open/close for websockets

const WebSocket = require('ws');

const uri = 'ws://psychic.local/ws';

async function websocketClient() {
  console.log(`Starting test`);
  for (let i = 0; i < 1000000; i++) {
    const ws = new WebSocket(uri);

    if (i % 100 == 0)
      console.log(`Count: ${i}`);

    ws.on('open', () => {
      //console.log(`Connected`);
    });

    ws.on('message', (message) => {
      //console.log(`Message: ${message}`);
      ws.close();
    });

    ws.on('error', (error) => {
      console.error(`Error: ${error.message}`);
    });

    await new Promise((resolve) => {
      ws.on('close', () => {
        resolve();
      });
    });
  }
}

websocketClient();