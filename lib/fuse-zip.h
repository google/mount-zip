////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2008-2010 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software; you can redistribute it and/or modify  //
//  it under the terms of the GNU Lesser General Public License as        //
//  published by the Free Software Foundation; either version 3 of the    //
//  License, or (at your option) any later version.                       //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU Lesser General Public      //
//  License along with this program; if not, write to the                 //
//  Free Software Foundation, Inc.,                                       //
//  51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               //
////////////////////////////////////////////////////////////////////////////

#ifndef FUSE_ZIP_H
#define FUSE_ZIP_H

/**
 * Main functions of fuse-zip file system (to be called by FUSE library)
 */

extern "C" {

/**
 * Initialize libzip and fuse-zip structures.
 *
 * @param program   Program name
 * @param fileName  ZIP file name
 * @return NULL if an error occured, otherwise pointer to FuseZipData structure.
 */
class FuseZipData *initFuseZip(const char *program, const char *fileName,
        bool readonly);

/**
 * Initialize filesystem
 *
 * Report current working dir and archive file name to syslog.
 *
 * @return filesystem-private data
 */
void *fusezip_init(struct fuse_conn_info *conn);

/**
 * Destroy filesystem
 *
 * Save all modified data back to ZIP archive and report to syslog about completion.
 * Note that filesystem unmounted before this method finishes
 * (see http://code.google.com/p/fuse-zip/issues/detail?id=7).
 */
void fusezip_destroy(void *data);

int fusezip_getattr(const char *path, struct stat *stbuf);

int fusezip_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);

int fusezip_statfs(const char *path, struct statvfs *buf);

int fusezip_open(const char *path, struct fuse_file_info *fi);

int fusezip_create(const char *path, mode_t mode, struct fuse_file_info *fi);

int fusezip_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int fusezip_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int fusezip_release (const char *path, struct fuse_file_info *fi);

int fusezip_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi);

int fusezip_truncate(const char *path, off_t offset);

int fusezip_unlink(const char *path);

int fusezip_rmdir(const char *path);

int fusezip_mkdir(const char *path, mode_t mode);

int fusezip_rename(const char *path, const char *new_path);

int fusezip_utimens(const char *path, const struct timespec tv[2]);

#if ( __APPLE__ )
int fusezip_setxattr(const char *, const char *, const char *, size_t, int, uint32_t);
int fusezip_getxattr(const char *, const char *, char *, size_t, uint32_t);
#else
int fusezip_setxattr(const char *, const char *, const char *, size_t, int);
int fusezip_getxattr(const char *, const char *, char *, size_t);
#endif

int fusezip_listxattr(const char *, char *, size_t);

int fusezip_removexattr(const char *, const char *);

int fusezip_chmod(const char *, mode_t);

int fusezip_chown(const char *, uid_t, gid_t);

int fusezip_flush(const char *, struct fuse_file_info *);

int fusezip_fsync(const char *, int, struct fuse_file_info *);

int fusezip_fsyncdir(const char *, int, struct fuse_file_info *);

int fusezip_opendir(const char *, struct fuse_file_info *);

int fusezip_releasedir(const char *, struct fuse_file_info *);

int fusezip_access(const char *, int);

}

#endif

