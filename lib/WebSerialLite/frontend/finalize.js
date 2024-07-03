let path = require('path');
let fs = require('fs');
const {minify} = require('html-minifier-terser');
let gzipAsync = require('@gfx/zopfli').gzipAsync;

const SAVE_PATH = '../src';

function chunkArray(myArray, chunk_size) {
    let index = 0;
    let arrayLength = myArray.length;
    let tempArray = [];
    for (index = 0; index < arrayLength; index += chunk_size) {
        let myChunk = myArray.slice(index, index + chunk_size);
        tempArray.push(myChunk);
    }
    return tempArray;
}

function addLineBreaks(buffer) {
    let data = '';
    let chunks = chunkArray(buffer, 30);
    chunks.forEach((chunk, index) => {
        data += chunk.join(',');
        if (index + 1 !== chunks.length) {
            data += ',\n';
        }
    });
    return data;
}

(async function(){
    const indexHtml = fs.readFileSync(path.resolve(__dirname, './index.html')).toString();
    const indexHtmlMinify = await minify(indexHtml, {
        collapseWhitespace: true,
        removeComments: true,
        removeAttributeQuotes: true,
        removeRedundantAttributes: true,
        removeScriptTypeAttributes: true,
        removeStyleLinkTypeAttributes: true,
        useShortDoctype: true,
        minifyCSS: true,
        minifyJS: true,
        sortAttributes: true, // 不会改变生成的html长度 但会优化压缩后体积
        sortClassName: true, // 不会改变生成的html长度 但会优化压缩后体积
    });
    console.log(`[finalize.js] Minified index.html | Original Size: ${(indexHtml.length / 1024).toFixed(2) }KB | Minified Size: ${(indexHtmlMinify.length / 1024).toFixed(2) }KB`);

    try{
        const GZIPPED_INDEX = await gzipAsync(indexHtmlMinify, { numiterations: 15 });

        const FILE = 
`#ifndef _webserial_webapge_h
#define _webserial_webpage_h
const uint32_t WEBSERIAL_HTML_SIZE = ${GZIPPED_INDEX.length};
const uint8_t WEBSERIAL_HTML[] PROGMEM = { 
${ addLineBreaks(GZIPPED_INDEX) }
};
#endif
`;
  
        fs.writeFileSync(path.resolve(__dirname, SAVE_PATH+'/WebSerialWebPage.h'), FILE);
        console.log(`[finalize.js] Compressed Bundle into WebSerialWebPage.h header file | Total Size: ${(GZIPPED_INDEX.length / 1024).toFixed(2) }KB`)
    }catch(err){
        return console.error(err);
    }
  })();