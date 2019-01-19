#ifndef CRC32_H
#define CRC32_H

unsigned int xcrc32 (const unsigned char *buf, int len, unsigned int init);


inline unsigned int crc32 (const unsigned char *buf, int len) {
  return xcrc32(buf, len, 0xffffffff);
}


#endif /* CRC32_H */
