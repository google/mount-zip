#ifndef BIG_BUFFER_H
#define BIG_BUFFER_H

#include <unistd.h>
#include <zip.h>

#include <vector>

class BigBuffer {
private:
    typedef std::vector<char*> chunks_t;

    static const int chunkSize = 4*1024; //4 Kilobytes

    chunks_t chunks;

    static ssize_t zipUserFunctionCallback(void *state, void *data, size_t len, enum zip_source_cmd cmd);
public:
    ssize_t len;

    BigBuffer();
    BigBuffer(struct zip *z, int nodeId, ssize_t length);
    ~BigBuffer();

    int read(char *buf, size_t size, off_t offset) const;
    int write(const char *buf, size_t size, off_t offset);
    int saveToZip(struct zip *z, const char *fname);
};

#endif

