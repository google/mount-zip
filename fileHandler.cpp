#include "fileHandler.h"

FileHandler:: FileHandler(struct zip *_z, struct zip_file *_zf): z(_z), zf(_zf), pos(0) {
}

FileHandler::~FileHandler() {
}

