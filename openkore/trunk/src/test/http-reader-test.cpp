/*  HttpReader unit test program
 *  Copyright (C) 2006   Written by VCL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#undef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <list>

#ifdef WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	#include <unistd.h>
	#define Sleep(miliseconds) usleep(miliseconds * 1000)
#endif

#include "std-http-reader.h"
#include "mirror-http-reader.h"

using namespace std;
using namespace OpenKore;


typedef HttpReader * (*HttpReaderCreator) (const char *url);

#define SMALL_TEST_URL "http://www.openkore.com/misc/testHttpReader.txt"
#define SMALL_TEST_CONTENT "Hello world!\n"
#define SMALL_TEST_SIZE 13
#define SMALL_TEST_CHECKSUM 2773980202U

#define LARGE_TEST_URL "http://www.openkore.com/misc/testHttpReaderLarge.txt"
#define LARGE_TEST_SIZE 74048
#define LARGE_TEST_CHECKSUM 1690026430U

#define ERROR_URL "http://www.openkore.com/FileNotFound.txt"
#define INVALID_URL "http://111.111.111.111:82/"
#define SECURE_URL "https://sourceforge.net"

/**
 * A class for testing a HttpReader implementation.
 */
class Tester {
public:
	/**
	 * Create a new Tester object.
	 *
	 * @param creatorFunc  A function which creates a HttpReader instance.
	 * @require creatorFunc != NULL
	 */
	Tester(HttpReaderCreator creatorFunc) {
		this->createHttpReader = creatorFunc;
	}

	/** Run the unit tests. */
	void
	run() {
		printf("Testing status transitions (1)...\n");
		assert( testStatusTransitions(SMALL_TEST_URL) );
		printf("Testing status transitions (2)...\n");
		assert( testStatusTransitions(LARGE_TEST_URL) );
		printf("Testing status transitions (3)...\n");
		assert( !testStatusTransitions(ERROR_URL) );
		printf("Testing status transitions (4)...\n");
		assert( testStatusTransitions(SECURE_URL) );
		printf("Testing status transitions (5)...\n");
	
		printf("Testing getData (1)...\n");
		assert( testGetData(SMALL_TEST_URL, SMALL_TEST_CONTENT, SMALL_TEST_SIZE) );
		printf("Testing getData (2)...\n");
		assert( testGetData(LARGE_TEST_URL, NULL, LARGE_TEST_SIZE) );
		printf("Testing getData (3)...\n");
		assert( !testGetData(ERROR_URL, NULL, 0) );
	
		printf("Testing pullData (1)...\n");
		assert( testPullData(SMALL_TEST_URL, SMALL_TEST_SIZE, SMALL_TEST_CHECKSUM) );
		printf("Testing pullData (2)...\n");
		assert( testPullData(LARGE_TEST_URL, LARGE_TEST_SIZE, LARGE_TEST_CHECKSUM) );
		printf("Testing pullData (3)...\n");
		assert( !testPullData(ERROR_URL, 0, 0) );
	}

private:
	HttpReaderCreator createHttpReader;

	// Test whether status transitions behave as documented.
	bool
	testStatusTransitions(const char *url) {
		HttpReader *http = createHttpReader(url);
		HttpReaderStatus status = HTTP_READER_CONNECTING, oldStatus;
		do {
			oldStatus = status;
			status = http->getStatus();
			switch (oldStatus) {
			case HTTP_READER_CONNECTING:
				assert(status == HTTP_READER_CONNECTING
					|| status == HTTP_READER_DOWNLOADING
					|| status == HTTP_READER_DONE
					|| status == HTTP_READER_ERROR);
				break;
			case HTTP_READER_DOWNLOADING:
				assert(status == HTTP_READER_DOWNLOADING
					|| status == HTTP_READER_DONE
					|| status == HTTP_READER_ERROR);
				break;
			case HTTP_READER_DONE:
				assert(status == HTTP_READER_DONE);
				break;
			case HTTP_READER_ERROR:
				assert(status == HTTP_READER_ERROR);
				break;
			default:
				printf("Unknown status %d\n", (int) status);
				abort();
				break;
			};
			Sleep(10);
		} while (status != HTTP_READER_DONE && status != HTTP_READER_ERROR);

		Sleep(1000);
		if (status == HTTP_READER_DONE) {
			assert(http->getStatus() == HTTP_READER_DONE);
		} else {
			assert(http->getStatus() == HTTP_READER_ERROR);
		}
		delete http;
		return status == HTTP_READER_DONE;
	}

	// Test whether getData() works
	bool
	testGetData(const char *url, const char *content, unsigned int size) {
		HttpReader *http = createHttpReader(url);
		while (http->getStatus() != HTTP_READER_DONE
		    && http->getStatus() != HTTP_READER_ERROR) {
			Sleep(10);
		}

		if (http->getStatus() != HTTP_READER_DONE) {
			delete http;
			return false;
		}

		unsigned int downloadedLen = 0;
		const char *downloadedData = http->getData(downloadedLen);
		assert(downloadedLen == size);
		assert(http->getSize() == (int) size);
		if (content != NULL) {
			assert(strcmp(downloadedData, content) == 0);
		}
		delete http;
		return true;
	}

	// Test whether pullData() works
	bool
	testPullData(const char *url, unsigned int expectedSize, unsigned int expectedChecksum) {
		HttpReader *http = createHttpReader(url);
		bool result;
		unsigned int checksum = 0;
		unsigned int size = 0;
		char buffer[1024];
		int ret;
		bool done = false;

		while (http->getStatus() == HTTP_READER_CONNECTING) {
			Sleep(10);
		}
		while (!done) {
			ret = http->pullData(buffer, sizeof(buffer));
			if (ret == -1) {
				Sleep(10);

			} else if (ret > 0) {
				for (int i = 0; i < ret; i++) {
					checksum = checksum * 32 + buffer[i];
				}
				size += ret;

			} else if (ret == -2 || ret == 0) {
				done = true;

			} else {
				printf("pullData() returned an invalid value: %d\n", ret);
				abort();
			}
		}

		result = http->getStatus() == HTTP_READER_DONE;
		if (result) {
			assert(expectedSize == size);
			assert(expectedChecksum == checksum);
		}

		delete http;
		return result;
	}
};

static HttpReader *
createStdHttpReader(const char *url) {
	return StdHttpReader::create(url);
}

static HttpReader *
createMirrorHttpReader(const char *url) {
	list<const char *> urls;
	urls.push_back(url);
	return new MirrorHttpReader(urls);
}

int
main() {
	Tester *tester;

	printf("### StdHttpReader\n");
	tester = new Tester(createStdHttpReader);
	tester->run();
	delete tester;

	printf("### MirrorHttpReader\n");
	tester = new Tester(createMirrorHttpReader);
	tester->run();
	delete tester;

	return 0;
}
