# bin2array

Converts binary file to C-style array initializer.

Ever wanted to embed a binary file in your program? Trying to serve images and executables from a tiny web server on Arduino or ESP8266? This utility is here to help.

## Requirements

 * Python 3

## Usage

I guess it is self-explanatory.

```
usage: bin2array.py [-h] [-O OUTPUT] [-l LINEBREAK] [-L LINEBREAK_STRING]
                    [-S SEPARATOR_STRING] [-H ELEMENT_PREFIX]
                    [-T ELEMENT_SUFFIX] [-U] [-n]
                    filename

Convert binary file to C-style array initializer.

positional arguments:
  filename              the file to be converted

optional arguments:
  -h, --help            show this help message and exit
  -O OUTPUT, --output OUTPUT
                        write output to a file
  -l LINEBREAK, --linebreak LINEBREAK
                        add linebreak after every N element
  -L LINEBREAK_STRING, --linebreak-string LINEBREAK_STRING
                        use what to break link, defaults to "\n"
  -S SEPARATOR_STRING, --separator-string SEPARATOR_STRING
                        use what to separate elements, defaults to ", "
  -H ELEMENT_PREFIX, --element-prefix ELEMENT_PREFIX
                        string to be added to the head of element, defaults to
                        "0x"
  -T ELEMENT_SUFFIX, --element-suffix ELEMENT_SUFFIX
                        string to be added to the tail of element, defaults to
                        none
  -U, --force-uppercase
                        force uppercase HEX representation
  -n, --newline         add a newline on file end
```

## Caveats

### Arduino IDE

**Do not put large source code files in the root folder of your project.** Otherwise some of the following events will happen:

 * One of your CPU cores been eaten up by java
 * The splash screen shows up but never loads
 * 3rd World War

Make a new folder inside project root, put the converted file (use `.h` as extension, otherwise may not be recognized) in, then use the following grammer to use it:

```C++
const char great_image[] PROGMEM = {
#include "data/great_image.png.h"
}
```

If you are using ESP8266WebServer to serve static binary files, you can use the following code:

```C++
#include <ESP8266WebServer.h>

// create server
ESP8266WebServer server(80);

// include the image data
const char image[] PROGMEM = {
#include "data/image.png.h"
};

// statis binary file handler
void handleImage() {
  server.sendHeader("Cache-Control", "max-age=31536000", false);
  server.send_P(200, "image/png", image, sizeof(image));
}

void setup() {
  // do other things...
  // register image handler before server.begin()
  server.on("/image.png", handleImage);
  // do other things...
  server.begin();
}
```

### Windows

Do not use command line redirection (`python bin2array.py test.png > test.png.h`) since CMD will save the file using UTF-16 which is not recognized by some compiler. Use `-O` option to save output to file, or manually convert UTF-16 to UTF-8 for maximum compatibility.