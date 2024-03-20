#include "filesys/fsutil.h"
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* List files in the root directory. */
void
fsutil_ls (char **argv UNUSED) {
	struct dir *dir;
	char name[NAME_MAX + 1];

	printf ("Files in the root directory:\n");
	dir = dir_open_root ();
	if (dir == NULL)
		PANIC ("root dir open failed");
	while (dir_readdir (dir, name))
		printf ("%s\n", name);
	printf ("End of listing.\n");
}

/* Prints the contents of file ARGV[1] to the system console as
 * hex and ASCII. */
void
fsutil_cat (char **argv) {
	const char *file_name = argv[1];

	struct file *file;
	char *buffer;

	printf ("Printing '%s' to the console...\n", file_name);
	file = filesys_open (file_name);
	if (file == NULL)
		PANIC ("%s: open failed", file_name);
	buffer = palloc_get_page (PAL_ASSERT);
	for (;;) {
		off_t pos = file_tell (file);
		off_t n = file_read (file, buffer, PGSIZE);
		if (n == 0)
			break;

		hex_dump (pos, buffer, n, true); 
	}
	palloc_free_page (buffer);
	file_close (file);
}

/* Deletes file ARGV[1]. */
void
fsutil_rm (char **argv) {
	const char *file_name = argv[1];

	printf ("Deleting '%s'...\n", file_name);
	if (!filesys_remove (file_name))
		PANIC ("%s: delete failed\n", file_name);
}

/* Copies from the "scratch" disk, hdc or hd1:0 to file ARGV[1]
 * in the file system.
 * "스크래치" 디스크, hdc 또는 hd1:0에서 파일 시스템의 ARGV[1] 파일로 복사합니다.
 * 
 * The current sector on the scratch disk must begin with the
 * string "PUT\0" followed by a 32-bit little-endian integer
 * indicating the file size in bytes.  Subsequent sectors hold
 * the file content.
 * 스크래치 디스크의 현재 섹터는 문자열 "PUT\0"으로 시작하여 
 * 파일 크기(바이트)를 나타내는 32비트 little-endian 정수로 시작해야 합니다.
 * 후속 섹터는 파일 내용을 유지합니다.
 * 
 * The first call to this function will read starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_get(), so all `put's should precede all `get's. 
 * 이 함수에 대한 첫 번째 호출은 스크래치 디스크의 처음부터 읽습니다. 
 * 나중에 호출은 디스크 전체에서 진행됩니다. 
 * 이 디스크 위치는 fsutil_get()에 사용되는 위치와는 독립적이므로 
 * 모든 'put's가 모든 'get's보다 앞에 와야 합니다.
 */
void
fsutil_put (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	struct disk *src;
	struct file *dst;
	off_t size;
	void *buffer;

	printf ("Putting '%s' into the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* Open source disk and read file size. */
	src = disk_get (1, 0);
	if (src == NULL)
		PANIC ("couldn't open source disk (hdc or hd1:0)");

	/* Read file size. */
	disk_read (src, sector++, buffer);
	if (memcmp (buffer, "PUT", 4))
		PANIC ("%s: missing PUT signature on scratch disk", file_name);
	size = ((int32_t *) buffer)[1];
	if (size < 0)
		PANIC ("%s: invalid file size %d", file_name, size);

	/* Create destination file. */
	if (!filesys_create (file_name, size))
		PANIC ("%s: create failed", file_name);
	dst = filesys_open (file_name);
	if (dst == NULL)
		PANIC ("%s: open failed", file_name);

	/* Do copy. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		disk_read (src, sector++, buffer);
		if (file_write (dst, buffer, chunk_size) != chunk_size)
			PANIC ("%s: write failed with %"PROTd" bytes unwritten",
					file_name, size);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (dst);
	free (buffer);
}

/* Copies file FILE_NAME from the file system to the scratch disk.
 * 파일 시스템에서 파일 시스템의 파일 이름을 스크래치 디스크로 복사합니다.
 *
 * The current sector on the scratch disk will receive "GET\0"
 * followed by the file's size in bytes as a 32-bit,
 * little-endian integer.  Subsequent sectors receive the file's
 * data.
 * 스크래치 디스크의 현재 섹터는 "GET\0"을 받은 후 파일의 크기(바이트)는 32비트, 
 * 작은 엔디안 정수로 표시됩니다. 이후의 섹터는 파일의 데이터를 받습니다.
 *
 * The first call to this function will write starting at the
 * beginning of the scratch disk.  Later calls advance across the
 * disk.  This disk position is independent of that used for
 * fsutil_put(), so all `put's should precede all `get's. 
 * 이 함수에 대한 첫 번째 호출은 스크래치 디스크의 처음부터 기록합니다. 
 * 나중에 호출은 디스크 전체에서 진행됩니다. 
 * 이 디스크 위치는 fsutil_put()에 사용되는 위치와는 독립적이므로 
 * 모든 'put's가 모든 'get's보다 앞에 와야 합니다.
 */
void
fsutil_get (char **argv) {
	static disk_sector_t sector = 0;

	const char *file_name = argv[1];
	void *buffer;
	struct file *src;
	struct disk *dst;
	off_t size;

	printf ("Getting '%s' from the file system...\n", file_name);

	/* Allocate buffer. */
	buffer = malloc (DISK_SECTOR_SIZE);
	if (buffer == NULL)
		PANIC ("couldn't allocate buffer");

	/* Open source file. */
	src = filesys_open (file_name);
	if (src == NULL)
		PANIC ("%s: open failed", file_name);
	size = file_length (src);

	/* Open target disk. */
	dst = disk_get (1, 0);
	if (dst == NULL)
		PANIC ("couldn't open target disk (hdc or hd1:0)");

	/* Write size to sector 0. */
	memset (buffer, 0, DISK_SECTOR_SIZE);
	memcpy (buffer, "GET", 4);
	((int32_t *) buffer)[1] = size;
	disk_write (dst, sector++, buffer);

	/* Do copy. */
	while (size > 0) {
		int chunk_size = size > DISK_SECTOR_SIZE ? DISK_SECTOR_SIZE : size;
		if (sector >= disk_size (dst))
			PANIC ("%s: out of space on scratch disk", file_name);
		if (file_read (src, buffer, chunk_size) != chunk_size)
			PANIC ("%s: read failed with %"PROTd" bytes unread", file_name, size);
		memset (buffer + chunk_size, 0, DISK_SECTOR_SIZE - chunk_size);
		disk_write (dst, sector++, buffer);
		size -= chunk_size;
	}

	/* Finish up. */
	file_close (src);
	free (buffer);
}
