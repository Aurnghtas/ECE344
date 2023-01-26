#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

/* make sure to use syserror() when a system call fails. see common.h */

void usage() {
    fprintf(stderr, "Usage: cpr srcdir dstdir\n");
    exit(1);
}

void copy_file (char* src_file, char* dest_file) {
    int fd1, fd2;
    //struct stat *src_stat = NULL;
    
    fd1 = open(src_file, O_RDONLY);
    fd2 = creat(dest_file, 00700);
    
    /*int res = stat(src_file, src_stat);
    if(res == -1) {
        syserror(stat, src_file);
    }
    
    chmod(dest_file, src_stat->st_mode);
    //chmod*/
    
    if(fd1==-1) {
        syserror(open, src_file);
        return;
    }
    
    if(fd2==-1) {
        syserror(creat, dest_file);
        return;
    }
    
    //when read has read all the bytes in the file, it returns 0
    char buf[4096];
    int ret;
    while ( (ret=read(fd1, buf, 4096)) > 0 ){
        if(ret == -1) {
            syserror(read, src_file);
            return;
        }
        
        write(fd2, buf, ret);
    }
    
    close(fd2);
}

void copy_directory (char* src_path, char* dest_path){
    DIR *src_dir;
    struct dirent *src_entry;
    struct stat src_stat;
 
    
    src_dir = opendir(src_path);
    if( src_dir==NULL ){
        syserror(opendir, src_path);
        return;
    }
   
    //("%s\n", src_path);
    
    int res = stat(src_path, &src_stat);
    if(res == -1) {
        syserror(stat, src_path);
    }

    int ans = mkdir(dest_path, 00700);
    if (ans<0) {
        syserror(mkdir, dest_path);
    }
    
    //chmod(dest_path, src_stat->st_mode);
    
    while ( (src_entry=readdir(src_dir)) != NULL ) {

        struct stat src_stat_2;
        
        if( !strcmp(".", src_entry->d_name) || !strcmp("..", src_entry->d_name)) {
            continue;
        }
        
        //get the new path name
        //strcat
        char path[200];
        path[0] = '\0';
        strcat(path, src_path);
        strcat(path, "/");
        strcat(path, src_entry->d_name);

        char sub_dest_path[200];
        sub_dest_path[0] = '\0';
        strcat(sub_dest_path, dest_path);
        strcat(sub_dest_path, "/");
        strcat(sub_dest_path, src_entry->d_name);
        
        ////("this is source path in the while loop:%s\n", src_path);
        ////("this is dest path in the while loop:%s\n", sub_dest_path);

        stat(path, &src_stat_2);
        
        if( S_ISREG(src_stat_2.st_mode) ) {
            copy_file(path, sub_dest_path);
            chmod(sub_dest_path, src_stat_2.st_mode);
        }
        
        if( S_ISDIR(src_stat_2.st_mode)) {
            copy_directory(path, sub_dest_path);
        }
        
        //("hello");
     }
    
    chmod(dest_path, src_stat.st_mode);
    
    closedir(src_dir);
}

int main(int argc, char *argv[]) {
    //argv[1] is the src, argv[2] is the dest
    if (argc != 3) {
	usage();
    }
    
    copy_directory(argv[1], argv[2]);
    
    return 0;
}
