//#define PNG_MAX_BUFFERED_PIXELS ((1320*4 + 1)*2) --> overrwirte does here not work for some reason, so changed in PNGdec.h directly
#include <SD.h>
#include <PNGdec.h>


void *pngOpen(const char *filename, int32_t *size)
{
   //Serial.println("1");
  File *f = new File(SD.open(filename));

  *size = f->size();
  return (void *)f;
}

void pngClose(void *handle)
{
  // Serial.println("2");
  File *f = (File *)handle;
    f->close();
}

int32_t pngRead(PNGFILE *handle, uint8_t *buffer, int32_t length)
{
  //Serial.println("3");
  File *f = (File *)handle->fHandle;
  return f->read(buffer, length);
}

int32_t pngSeek(PNGFILE *handle, int32_t position)
{
  //Serial.println("4");
  File *f = (File *)handle->fHandle;
  return f->seek(position);
}
