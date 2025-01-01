const fs = require('fs');
const createCsvWriter = require('csv-writer').createObjectCsvWriter;

// Get the input and output file paths from the command line arguments
const inputFilePath = process.argv[2];
const outputFilePath = process.argv[3];

if (!inputFilePath || !outputFilePath) {
    console.error('Usage: node script.js <inputFilePath> <outputFilePath>');
    process.exit(1);
}

// Read and parse the JSON file
fs.readFile(inputFilePath, 'utf8', (err, data) => {
    if (err) {
        console.error('Error reading the input file:', err);
        return;
    }

    // Parse the JSON data
    const jsonData = JSON.parse(data);

    // Extract the desired fields
    const { url, connections, latency, requests, errors } = jsonData;
    const latencyMean = latency.mean;
    const requestsMean = requests.mean;

    // Set up the CSV writer
    const csvWriter = createCsvWriter({
        path: outputFilePath,
        header: [
            {id: 'url', title: 'URL'},
            {id: 'connections', title: 'Connections'},
            {id: 'requestsMean', title: 'Requests Mean'},
            {id: 'latencyMean', title: 'Latency Mean'},
            {id: 'errors', title: 'Errors'},
        ],
        append: true // this will append to the existing file
    });

    // Prepare the data to be written
    const records = [
        { url: url, connections: connections, latencyMean: latencyMean, requestsMean: requestsMean, errors: errors }
    ];

    // Write the data to the CSV file
    csvWriter.writeRecords(records)
        .then(() => {
            console.log('Data successfully appended to CSV file.');
        })
        .catch(err => {
            console.error('Error writing to the CSV file:', err);
        });
});

