/*
Copyright (c) 2015-2017 Alternative Games Ltd / Turo Lamminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#include <cstring>
#include <cassert>

#include <sys/stat.h>

#include "Utils.h"

#include <SDL.h>


static FILE *logFile;

void logInit() {
	assert(!logFile);

	char *logFilePath = SDL_GetPrefPath(nullptr, "SMAADemo");
	std::string logFileName(logFilePath);
	SDL_free(logFilePath);
	logFileName += "logfile.txt";
	logFile = fopen(logFileName.c_str(), "wb");
}


void logShutdown() {
	assert(logFile);

	fflush(logFile);
	fclose(logFile);
	logFile = nullptr;
}


std::vector<char> readTextFile(std::string filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (!file) {
		// TODO: better exception
		throw std::runtime_error("file not found " + filename);
	}

	int fd = fileno(file.get());
	if (fd < 0) {
		// TODO: better exception
		throw std::runtime_error("no fd");
	}

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	int retval = fstat(fd, &statbuf);
	if (retval < 0) {
		// TODO: better exception
		throw std::runtime_error("fstat failed");
	}

	unsigned int filesize = static_cast<unsigned int>(statbuf.st_size);
	// ensure NUL -termination
	std::vector<char> buf(filesize + 1, '\0');

	size_t ret = fread(&buf[0], 1, filesize, file.get());
	if (ret != filesize)
	{
		// TODO: better exception
		throw std::runtime_error("fread failed");
	}

	return buf;
}


std::vector<char> readFile(std::string filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (!file) {
		// TODO: better exception
		throw std::runtime_error("file not found " + filename);
	}

	int fd = fileno(file.get());
	if (fd < 0) {
		// TODO: better exception
		throw std::runtime_error("no fd");
	}

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	int retval = fstat(fd, &statbuf);
	if (retval < 0) {
		// TODO: better exception
		throw std::runtime_error("fstat failed");
	}

	unsigned int filesize = static_cast<unsigned int>(statbuf.st_size);
	std::vector<char> buf(filesize, '\0');

	size_t ret = fread(&buf[0], 1, filesize, file.get());
	if (ret != filesize)
	{
		// TODO: better exception
		throw std::runtime_error("fread failed");
	}

	return buf;
}


void writeFile(const std::string &filename, const void *contents, size_t size) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "wb"));

	fwrite(contents, 1, size, file.get());
}


bool fileExists(const std::string &filename) {
	std::unique_ptr<FILE, FILEDeleter> file(fopen(filename.c_str(), "rb"));

	if (file) {
		return true;
	} else {
		return false;
	}
}
