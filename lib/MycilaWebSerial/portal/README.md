# Frontend

The `index.html` is the only page of MycilaWebSerial and you can modify the page yourself and regenerate it.

In addition, I am also very happy that you can participate in fixing the bugs of the library or enhancing the functions of the library.

## Quick Start

You can modify and regenerate the page in three step. The execution of the following commands is based on the project root directory and you should install NodeJS and pnpm first.

```shell
cd .\frontend\
pnpm i
pnpm build
```

The `finalize.js` will compress and html and generate a new `WebSerialWebPage.h` in `../src` floder automatically.

Then you can rebuild your program, the new page ought be embedded in the firmware as expected.
