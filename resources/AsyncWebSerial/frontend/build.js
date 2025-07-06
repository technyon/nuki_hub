const fs = require("fs");
const path = require("path");
const zlib = require("zlib");
const htmlMinifier = require("html-minifier-terser").minify;

// Define paths
const inputPath = path.join(__dirname, "index.html");
const outputPath = path.join(__dirname, "../src/AsyncWebSerialHTML.h");

// Function to split buffer into 64-byte chunks
function splitIntoChunks(buffer, chunkSize) {
  let chunks = [];
  for (let i = 0; i < buffer.length; i += chunkSize) {
    chunks.push(buffer.slice(i, i + chunkSize));
  }
  return chunks;
}

(async function () {
  // read the index.html file
  const indexHtml = fs.readFileSync(inputPath, "utf8").toString();

  // Minify the HTML content
  const minifiedHtml = await htmlMinifier(indexHtml, {
    collapseWhitespace: true,
    removeComments: true,
    removeAttributeQuotes: true,
    removeRedundantAttributes: true,
    removeScriptTypeAttributes: true,
    removeStyleLinkTypeAttributes: true,
    useShortDoctype: true,
    minifyCSS: true,
    minifyJS: true,
    shortAttributes: true,
    shortClassName: true,
  });

  let oldSize = (indexHtml.length / 1024).toFixed(2);
  let newSize = (minifiedHtml.length / 1024).toFixed(2);

  console.log(`[Minifier] Original: ${oldSize}KB | Minified: ${newSize}KB`);

  // Gzip the minified HTML content
  let gzippedHtml = zlib.gzipSync(minifiedHtml);

  // Recreate the AsyncWebSerialHTML.h file with the new gzipped content
  // the content is stored as a byte array split into 64 byte chunks to avoid issues with the IDE
  let content = `#ifndef AsyncWebSerial_HTML_H
#define AsyncWebSerial_HTML_H

#include <Arduino.h>

const uint8_t ASYNCWEBSERIAL_HTML[] PROGMEM = {\n`;

  // Split gzipped HTML into 64-byte chunks
  let chunks = splitIntoChunks(gzippedHtml, 64);
  chunks.forEach((chunk, index) => {
    content += `  ${Array.from(chunk)
      .map((byte) => `0x${byte.toString(16).padStart(2, "0")}`)
      .join(", ")}`;
    if (index < chunks.length - 1) {
      content += ",\n";
    }
  });

  content += `\n};

#endif // AsyncWebSerial_HTML_H`;

  // Write the content to the output file
  fs.writeFileSync(outputPath, content);

  console.log("AsyncWebSerialHTML.h file created successfully!");
})();
