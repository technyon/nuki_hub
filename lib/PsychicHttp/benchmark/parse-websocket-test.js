const fs = require('fs');
const readline = require('readline');

if (process.argv.length !== 4) {
    console.error('Usage: node parse-websocket-test.js <input_file> <output_file>');
    process.exit(1);
}

const inputFile = process.argv[2];
const outputFile = process.argv[3];

async function parseFile() {
    const fileStream = fs.createReadStream(inputFile);

    const rl = readline.createInterface({
        input: fileStream,
        crlfDelay: Infinity
    });

    let targetUrl = null;
    let totalErrors = null;
    let meanLatency = null;
    let effectiveRps = null;
    let concurrentClients = null;

    for await (const line of rl) {
        if (line.startsWith('Target URL:')) {
            targetUrl = line.split(':').slice(1).join(':').trim();
        }
        if (line.startsWith('Total errors:')) {
            totalErrors = parseInt(line.split(':')[1].trim(), 10);
        }
        if (line.startsWith('Mean latency:')) {
            meanLatency = parseFloat(line.split(':')[1].trim());
        }
        if (line.startsWith('Effective rps:')) {
            effectiveRps = parseInt(line.split(':')[1].trim(), 10);
        }
        if (line.startsWith('Concurrent clients:')) {
            concurrentClients = parseInt(line.split(':')[1].trim(), 10);
        }
    }

    if (targetUrl === null || totalErrors === null || meanLatency === null || effectiveRps === null || concurrentClients === null) {
        console.error('Failed to extract necessary data from the input file');
        process.exit(1);
    }

    const csvLine = `${targetUrl},${concurrentClients},${effectiveRps},${meanLatency},${totalErrors}\n`;

    fs.appendFile(outputFile, csvLine, (err) => {
        if (err) {
            console.error('Failed to append to CSV file:', err);
            process.exit(1);
        }
        console.log('Data successfully appended to CSV file.');
    });
}

parseFile().catch(err => {
    console.error('Error reading file:', err);
    process.exit(1);
});

