#ifndef PsychicPrint_h
#define PsychicPrint_h

#ifdef ARDUINO
  // Use the real Arduino Print class
  #include <Print.h>

#else
  // ESP-IDF: minimal self-contained shim for Arduino's Print base class.
  // Only write(uint8_t) must be overridden; all print()/println()/printf()
  // variants are implemented here in terms of write().

  #include <cmath>
  #include <cstdarg>
  #include <cstdint>
  #include <cstdlib>
  #include <cstring>
  #include <string>

  #ifndef DEC
    #define DEC 10
  #endif
  #ifndef HEX
    #define HEX 16
  #endif
  #ifndef OCT
    #define OCT 8
  #endif
  #ifndef BIN
    #define BIN 2
  #endif

class Print
{
  public:
    virtual ~Print() {}

    // ----------------------------------------------------------------
    // Pure virtual – must be implemented by the subclass
    // ----------------------------------------------------------------
    virtual size_t write(uint8_t c) = 0;

    // ----------------------------------------------------------------
    // Bulk write – default loops; subclass may override for efficiency
    // ----------------------------------------------------------------
    virtual size_t write(const uint8_t* buffer, size_t size)
    {
      size_t n = 0;
      while (size--)
        n += write(*buffer++);
      return n;
    }

    // Convenience overloads that delegate to the above
    size_t write(const char* str)
    {
      if (!str)
        return 0;
      return write(reinterpret_cast<const uint8_t*>(str), strlen(str));
    }
    size_t write(const char* buffer, size_t size)
    {
      return write(reinterpret_cast<const uint8_t*>(buffer), size);
    }

    // ----------------------------------------------------------------
    // flush – no-op by default
    // ----------------------------------------------------------------
    virtual void flush() {}

    // ----------------------------------------------------------------
    // print overloads
    // ----------------------------------------------------------------
    size_t print(const char* str)
    {
      return write(str);
    }
    size_t print(const std::string& s)
    {
      return write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
    size_t print(char c)
    {
      return write(static_cast<uint8_t>(c));
    }
    size_t print(unsigned char n, int base = DEC)
    {
      return printULong(static_cast<unsigned long>(n), base);
    }
    size_t print(int n, int base = DEC)
    {
      return printLong(static_cast<long>(n), base);
    }
    size_t print(unsigned int n, int base = DEC)
    {
      return printULong(static_cast<unsigned long>(n), base);
    }
    size_t print(long n, int base = DEC)
    {
      return printLong(n, base);
    }
    size_t print(unsigned long n, int base = DEC)
    {
      return printULong(n, base);
    }
    size_t print(long long n, int base = DEC)
    {
      return printLLong(n, base);
    }
    size_t print(unsigned long long n, int base = DEC)
    {
      return printULLong(n, base);
    }
    size_t print(double n, int digits = 2)
    {
      return printFloat(n, static_cast<uint8_t>(digits));
    }

    // ----------------------------------------------------------------
    // println overloads
    // ----------------------------------------------------------------
    size_t println()
    {
      return write("\r\n", 2);
    }
    size_t println(const char* str) { return print(str) + println(); }
    size_t println(const std::string& s) { return print(s) + println(); }
    size_t println(char c) { return print(c) + println(); }
    size_t println(unsigned char n, int base = DEC) { return print(n, base) + println(); }
    size_t println(int n, int base = DEC) { return print(n, base) + println(); }
    size_t println(unsigned int n, int base = DEC) { return print(n, base) + println(); }
    size_t println(long n, int base = DEC) { return print(n, base) + println(); }
    size_t println(unsigned long n, int base = DEC) { return print(n, base) + println(); }
    size_t println(long long n, int base = DEC) { return print(n, base) + println(); }
    size_t println(unsigned long long n, int base = DEC) { return print(n, base) + println(); }
    size_t println(double n, int digits = 2) { return print(n, digits) + println(); }

    // ----------------------------------------------------------------
    // vprintf / printf
    // ----------------------------------------------------------------
    size_t vprintf(const char* format, va_list arg)
    {
      // First pass: measure
      va_list arg2;
      va_copy(arg2, arg);
      int len = vsnprintf(nullptr, 0, format, arg2);
      va_end(arg2);
      if (len <= 0)
        return 0;
      char* buf = static_cast<char*>(malloc(len + 1));
      if (!buf)
        return 0;
      vsnprintf(buf, len + 1, format, arg);
      size_t written = write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(len));
      free(buf);
      return written;
    }

    int printf(const char* format, ...) __attribute__((format(printf, 2, 3)))
    {
      va_list args;
      va_start(args, format);
      size_t written = vprintf(format, args);
      va_end(args);
      return static_cast<int>(written);
    }

  private:
    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------
    size_t printULong(unsigned long n, int base)
    {
      if (base < 2)
        base = DEC;
      // Largest possible: 64 binary digits + NUL
      char buf[65];
      char* p = buf + sizeof(buf) - 1;
      *p = '\0';
      do {
        unsigned int digit = static_cast<unsigned int>(n % static_cast<unsigned long>(base));
        n /= static_cast<unsigned long>(base);
        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
      } while (n);
      return write(p);
    }

    size_t printLong(long n, int base)
    {
      if (base == DEC && n < 0) {
        size_t s = write('-');
        return s + printULong(static_cast<unsigned long>(-n), base);
      }
      return printULong(static_cast<unsigned long>(n), base);
    }

    size_t printULLong(unsigned long long n, int base)
    {
      if (base < 2)
        base = DEC;
      char buf[65];
      char* p = buf + sizeof(buf) - 1;
      *p = '\0';
      do {
        unsigned int digit = static_cast<unsigned int>(n % static_cast<unsigned long long>(base));
        n /= static_cast<unsigned long long>(base);
        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
      } while (n);
      return write(p);
    }

    size_t printLLong(long long n, int base)
    {
      if (base == DEC && n < 0) {
        size_t s = write('-');
        return s + printULLong(static_cast<unsigned long long>(-n), base);
      }
      return printULLong(static_cast<unsigned long long>(n), base);
    }

    size_t printFloat(double number, uint8_t digits)
    {
      size_t n = 0;

      if (std::isnan(number))
        return print("nan");
      if (std::isinf(number))
        return print("inf");
      if (number > 4294967040.0)
        return print("ovf");
      if (number < -4294967040.0)
        return print("ovf");

      if (number < 0.0) {
        n += write('-');
        number = -number;
      }

      // Round to the requested decimal places
      double rounding = 0.5;
      for (uint8_t i = 0; i < digits; ++i)
        rounding /= 10.0;
      number += rounding;

      unsigned long int_part = static_cast<unsigned long>(number);
      double remainder = number - static_cast<double>(int_part);

      n += printULong(int_part, DEC);

      if (digits > 0) {
        n += write('.');
        for (uint8_t i = 0; i < digits; ++i) {
          remainder *= 10.0;
          int digit = static_cast<int>(remainder);
          n += write(static_cast<uint8_t>('0' + digit));
          remainder -= digit;
        }
      }
      return n;
    }
};

#endif // ARDUINO
#endif // PsychicPrint_h
