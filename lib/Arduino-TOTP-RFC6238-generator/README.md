# Arduino libary for TOTP generation

Example use:

     // Seed value - as per the QR code; which is in fact a base32 encoded
     // byte array (i.e. it is binary).
     //
     const char * seed = "ORUGKU3FMNZGK5CTMVSWI===";
   
     // Example of the same thing - but as usually formatted when shown
     // as the 'alternative text to enter'
     //
     // const char * seed = "ORU GKU 3FM NZG K5C TMV SWI";
   
     String * otp = TOTP::currentOTP(seed);
   
     Serial.print(ctime(&t));
     Serial.print("   TOTP at this time is: ");
     Serial.println(*otp);
     Serial.println();
  
This assumes a normal RFC compliant TOTP. It is possible that the Qr code provides
different values for the interval (default is 30 seconds), epoch or the hash (sha1). 
These can be passwd as optional arguments.
