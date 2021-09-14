
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <cstdio>
#include <cmath>

using namespace std;

#define DISK_SIZE 256
#define R true
#define W false


//======================================================================================================================
/// fsFile area

class FsFile {

    int file_size;
    int block_in_use;
    int index_block;
    // int block_size;   never used

public:

    FsFile(int _block_size) {
        file_size = 0;
        block_in_use = 0;
        //block_size = _block_size; << never used
        index_block = -1;
    }

    int getfile_size() const {
        return file_size;
    }

    void setfile_size(int size) {
        file_size = size;
    }

    int getIndex_block() const {
        return this->index_block;
    }

    void setIndex_block(int block) {
        this->index_block = block;
    }

    int getBlockInUse() const {
        return block_in_use;
    }

    void setBlockInUse(int blockInUse) {
        block_in_use = blockInUse;
    }

};

//======================================================================================================================
///  FileDescriptor area

class FileDescriptor {
    string file_name;
    FsFile *fs_file;
    bool inUse;

public:

    FileDescriptor(string FileName, FsFile *fsi) {
        file_name = FileName;
        fs_file = fsi;
        inUse = true;
    }

    string getFileName() {

        return file_name;
    }

    bool getIsUse() const {
        return inUse;
    }

    void setIsUse(bool val) {
        this->inUse = val;
    }

    FsFile *getFsFile() {
        return fs_file;
    }

};

#define DISK_SIM_FILE "DISK_SIM_FILE.txt"

//======================================================================================================================
/// fsDisk area

class fsDisk {

    FILE *sim_disk_fd;
    bool is_formated;

    // BitVector - "bit" (int) vector, indicate which block in the disk is free
    //              or not.  (i.e. if BitVector[0] == 1 , means that the
    //             first block is occupied.

    int BitVectorSize;
    int *BitVector;

    // filename and one fsFile.
    // MainDir;
    map<string, FileDescriptor *> MainDir;

    // OpenFileDescriptors --  when you open a file,
    // the operating system creates an entry to represent that file
    // This entry number is the file descriptor.


    // OpenFileDescriptors;
    map<int, FileDescriptor *> OpenFileDescriptors;

    int block_size;
    int freeBlocks;

    // ------------------------------------------------------------------------
public:

    // constructor and destructor
    fsDisk() {

        sim_disk_fd = fopen(DISK_SIM_FILE, "r+");
        assert(sim_disk_fd);

        for (int i = 0; i < DISK_SIZE; i++) {

            fseek(sim_disk_fd, i, SEEK_SET);
            int ret_val = (int) fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }

        BitVector = nullptr;
        fflush(sim_disk_fd);
        block_size = 0;
        is_formated = false;
        BitVectorSize = 0;
        freeBlocks = 0;
    }

    // ------------------------------------------------------------------------
    ~fsDisk() {

        delete[] BitVector;
        for (const auto &file:MainDir) {
            delete file.second->getFsFile();
            delete file.second;
        }
        fclose(sim_disk_fd);
    }
    // ------------------------------------------------------------------------

    // Main functions
    // ------------------------------------------------------------------------
    void listAll() {

        int i = 0;
        for (auto j = MainDir.begin(); j != MainDir.end(); j++, i++) {
            cout << "index: " << i << ": FileName: " << j->first << " , isInUse: " << j->second->getIsUse() << endl;
        }

        char bufy;
        cout << "Disk content: '";

        for (i = 0; i < DISK_SIZE; i++) {

            fseek(sim_disk_fd, i, SEEK_SET);
            (int) fread(&bufy, 1, 1, sim_disk_fd);
            cout << bufy;
        }

        cout << "'" << endl;
    }

    // ------------------------------------------------------------------------
    void fsFormat(int blockSize = 4) {

        if (blockSize <= 0 || blockSize > DISK_SIZE) {
            return;
        }
        // clean files delete and delete allocated data
        for (const auto &file:MainDir) {

            delete file.second->getFsFile();
            delete file.second;
        }
        delete[] BitVector;
        MainDir.clear();
        OpenFileDescriptors.clear();

        //  reset the disk with '\0'
        for (int i = 0, ret_val; i < DISK_SIZE; i++) {

            fseek(sim_disk_fd, i, SEEK_SET);
            ret_val = (int) fwrite("\0", 1, 1, sim_disk_fd);
            assert(ret_val == 1);
        }

        //  initializing
        this->block_size = blockSize;
        freeBlocks = DISK_SIZE / block_size;
        BitVectorSize = DISK_SIZE / block_size;
        BitVector = new int[BitVectorSize];

        for (int i = 0; i < BitVectorSize; i++) {
            BitVector[i] = 0;
        }
        is_formated = true;

    }

    // ------------------------------------------------------------------------
    int CreateFile(string fileName) {

        //check if disk is ready to use
        if (!is_formated) {
            return -1;  // file did not made
        }

        //checking if the file already exist
        if (MainDir.find(fileName) != MainDir.end()) {
            return -1;
        }

        auto *newfsFile = new FsFile(this->block_size);

        /// update data bases
        int newFD = findFreeFD();
        MainDir[fileName] = new FileDescriptor(fileName, newfsFile);
        OpenFileDescriptors[newFD] = MainDir[fileName];
        return newFD;
    }

    // ------------------------------------------------------------------------
    int OpenFile(string fileName) {

        //check if disk is ready to use
        if (!is_formated) {
            return -1;  // file did not made
        }

        // check if the file exist
        if (MainDir.find(fileName) == MainDir.end()) {
            return -1;
        }

        // check if file is opened
        if (MainDir[fileName]->getIsUse()) {
            return -1;
        }

        int newFD = findFreeFD();
        OpenFileDescriptors[newFD] = MainDir[fileName];
        MainDir[fileName]->setIsUse(true);

        return newFD;
    }

    // ------------------------------------------------------------------------
    string CloseFile(int fd) {

        //check if disk is ready to use
        if (!is_formated) {
            return "-1";  // file did not made
        }

        // to check if the file is open and exist, ill compare fd = vector[fd] is valid in vector
        if (fd < 0 || OpenFileDescriptors.find(fd) == OpenFileDescriptors.end() ||
            !OpenFileDescriptors[fd]->getIsUse()) {
            return "-1";
        }

        string fileName = OpenFileDescriptors[fd]->getFileName();
        OpenFileDescriptors[fd]->setIsUse(false);
        OpenFileDescriptors.erase(fd);
        return fileName;
    }

    // ------------------------------------------------------------------------
    int WriteToFile(int fd, char *buf, int len) {

        //check if disk is ready to use
        if (!is_formated) {
            return -1;  // file did not made
        }

        // checking the len size in range
        if (len <= 0) {
            return 0;
        }

        // to check if the file is open and exist, ill compare fd = vector[fd] is valid in vector
        if (fd < 0 || OpenFileDescriptors.find(fd) == OpenFileDescriptors.end() ||
            !OpenFileDescriptors[fd]->getIsUse()) {
            return -1;
        }

        string fileName = OpenFileDescriptors[fd]->getFileName();
        int maxFileSize = block_size * block_size;

        // getting data
        int indexBlock, fileSize = MainDir[fileName]->getFsFile()->getfile_size();
        int blockInUse = MainDir[fileName]->getFsFile()->getBlockInUse();

        // check if need to set index block
        if (MainDir[fileName]->getFsFile()->getIndex_block() == -1) {
            if (!isEnoughBlocksOnDisk()) {
                return -1;
            }
            indexBlock = findFreeBlock();
            MainDir[fileName]->getFsFile()->setIndex_block(indexBlock);
        } else {
            indexBlock = MainDir[fileName]->getFsFile()->getIndex_block();
        }

        int write, newBlockToUse;
        int i = 0, lenTemp = len;
        int offSet = fileSize % block_size;
        char str[1];

        while (fileSize < maxFileSize && i < len && blockInUse <= block_size) {
            // if offSet is 0 then get new block to store in, write as much as you can and repeat
            if (offSet == 0) {
                newBlockToUse = setBlock(indexBlock, blockInUse);
                if (newBlockToUse == -1) {
                    return i;
                }
                blockInUse++;
                // blockoffset: which block to write, i: where to point in buff, offset: which block we are in
                write = readWrite(buf, newBlockToUse, i, lenTemp, offSet, W);
                i += write;
                fileSize += write;
            }
                // else keep entering into the incomplete block
            else {
                // get an occupied block from index block
                readWrite(str, indexBlock, 0, 1, fileSize / block_size, R);
                newBlockToUse = ((int) str[0]);
                write = readWrite(buf, newBlockToUse, i, lenTemp, offSet, W);
                i += write;
                fileSize += write;
                offSet = fileSize % block_size;
            }
            lenTemp = lenTemp - write;
        }
        // not enough file space to store everything
        if (MainDir[fileName]->getFsFile()->getfile_size() + len > maxFileSize) {
            MainDir[fileName]->getFsFile()->setfile_size(maxFileSize);
            MainDir[fileName]->getFsFile()->setBlockInUse(blockInUse);
            return i;
        }

        MainDir[fileName]->getFsFile()->setfile_size(fileSize);
        MainDir[fileName]->getFsFile()->setBlockInUse(blockInUse);
        return i;
    }

    // ------------------------------------------------------------------------
    int DelFile(string FileName) {

        //check if disk is ready to use
        if (!is_formated) {
            return 0;  // file did not made
        }

        // check if the file exist
        if (MainDir.find(FileName) == MainDir.end()) {
            return 0;
        }

        // getting data
        int blockInUse = MainDir[FileName]->getFsFile()->getBlockInUse();
        int indexBlock = MainDir[FileName]->getFsFile()->getIndex_block();
        FsFile *fsFile = MainDir[FileName]->getFsFile();
        int blockReadFrom;
        char str[1], reset[block_size];
        // making a reset template to override data
        for (int i = 0; i < block_size; i++) {
            reset[i] = '\0';
        }

        //free all blocks of the file
        if (fsFile->getIndex_block() >= 0) {
            for (int i = 0; i < blockInUse; i++) {

                // get an occupied block from index block
                readWrite(str, indexBlock, 0, 1, i, R);

                // delete its content with '\0'
                blockReadFrom = ((int) str[0]);
                readWrite(reset, blockReadFrom, 0, block_size, 0, W);

                BitVector[blockReadFrom] = 0;
                freeBlocks++;
            }
            // free index block and clear index block area
            readWrite(reset, indexBlock, 0, block_size, 0, W);

            BitVector[indexBlock] = 0;
            freeBlocks++;
        }
        // delete fileDescriptor allocation, and its FD

        int fd = 0;
        for (auto &OpenFileDescriptor : OpenFileDescriptors) {
            if (OpenFileDescriptor.second->getFileName() == FileName) {
                fd = OpenFileDescriptor.first;
                break;
            }
        }
        OpenFileDescriptors.erase(fd);
        delete (MainDir[FileName]->getFsFile());
        delete (MainDir[FileName]);
        MainDir.erase(FileName);

        return 1;
    }

    // ------------------------------------------------------------------------
    int ReadFromFile(int fd, char *buf, int len) {

        //check if disk is ready to use
        if (!is_formated) {
            return -1;  // file did not made
        }

        // checking the len size in range
        if (len <= 0) {
            return 0;
        }

        // to check if the file is open and exist, ill compare fd = vector[fd] is valid in vector
        if (fd < 0 || OpenFileDescriptors.find(fd) == OpenFileDescriptors.end() ||
            !OpenFileDescriptors[fd]->getIsUse()) {
            return -1;
        }

        // check it has index block
        if (OpenFileDescriptors[fd]->getFsFile()->getIndex_block() == -1) {
            return 0;
        }

        // clean buffer
        memset(buf, '\0', len);

        // getting data
        string fileName = OpenFileDescriptors[fd]->getFileName();
        int indexBlock = MainDir[fileName]->getFsFile()->getIndex_block();
        int fileSize = MainDir[fileName]->getFsFile()->getfile_size();

        int read = 0, blockReadFrom;
        int i = 0;
        int blockOffSet = 0;
        char str[1];
        int x = fileSize;

        while (i < len && i < fileSize) {
            // getting from which block to read
            readWrite(str, indexBlock, 0, 1, blockOffSet, R);
            blockReadFrom = (int) ((unsigned) str[0]);

            // read from the block
            if (len - i >= block_size) {
                read = readWrite(buf, blockReadFrom, i, block_size, 0, R);
                i += read;
                x = x - block_size;
            } else {
                read = readWrite(buf, blockReadFrom, i, len - read, 0, R);
                i += read;
            }
            blockOffSet++;
        }
        return i;
    }
    //------------------------------------------------------------------------

    // Private methods
private:
    bool isEnoughBlocksOnDisk() const {
        if (this->freeBlocks - 1 < 0) {
            return false;
        }
        return true;
    }

    //------------------------------------------------------------------------
    int findFreeBlock() {
        for (int i = 0; i < BitVectorSize; i++) {
            if (BitVector[i] == 0) {
                BitVector[i] = 1;
                freeBlocks = freeBlocks - 1;
                return i;
            }
        }
        return -1;
    }

    //------------------------------------------------------------------------
    int findFreeFD() {
        int i = 0;
        for (auto it: OpenFileDescriptors) {
            if (it.first == i) {
                i++;
            }
        }
        return i;
    }

    //------------------------------------------------------------------------
    int setBlock(int setInBlock, int blockOffSet) {
        int freeBlock = findFreeBlock();
        if (freeBlock != -1) {
            char str[1];
            str[0] = (char) freeBlock;
            readWrite(str, setInBlock, 0, 1, blockOffSet, W);
        }
        return freeBlock;
    }

    //------------------------------------------------------------------------
    int readWrite(char *buffer, int block, int startInBuf, int len, int offSetInBlock, bool function) {
        if (fseek(sim_disk_fd, block * block_size + offSetInBlock, SEEK_SET) < 0) {
            return 0;
        }
        int res = 0;
        for (int i = startInBuf, val; res < len && res < block_size - offSetInBlock; i++, res++) {
            if (function == R) {
                val = (int) fread(&buffer[i], 1, 1, sim_disk_fd);
            } else {
                val = (int) fwrite(&buffer[i], 1, 1, sim_disk_fd);
            }
            assert(val == 1);
        }
        return res;
    }

};

//======================================================================================================================
int main() {
    int blockSize;
    string fileName;
    char str_to_write[DISK_SIZE];
    char str_to_read[DISK_SIZE];
    int size_to_read;
    int _fd;
    int writed;

    auto *fs = new fsDisk();
    int cmd_;
    while (true) {
        cin >> cmd_;
        switch (cmd_) {
            case 0:   // exit
                delete fs;
                exit(0);
                break;

            case 1:  // list-file
                fs->listAll();
                break;

            case 2:    // format
                cin >> blockSize;
                fs->fsFormat(blockSize);
                break;

            case 3:    // creat-file
                cin >> fileName;
                _fd = fs->CreateFile(fileName);
                cout << "CreateFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 4:  // open-file
                cin >> fileName;
                _fd = fs->OpenFile(fileName);
                cout << "OpenFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 5:  // close-file
                cin >> _fd;
                fileName = fs->CloseFile(_fd);
                cout << "CloseFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;

            case 6:   // write-file
                cin >> _fd;
                cin >> str_to_write;
                writed = fs->WriteToFile(_fd, str_to_write, (int) strlen(str_to_write));
                cout << "Writed: " << writed << " Char's into File Descriptor #: " << _fd << endl;
                break;

            case 7:    // read-file
                cin >> _fd;
                cin >> size_to_read;
                memset(str_to_read, '\0', DISK_SIZE);
                fs->ReadFromFile(_fd, str_to_read, size_to_read);
                cout << "ReadFromFile: " << str_to_read << endl;
                break;

            case 8:   // delete file
                cin >> fileName;
                _fd = fs->DelFile(fileName);
                cout << "DeletedFile: " << fileName << " with File Descriptor #: " << _fd << endl;
                break;
            default:
                break;
        }
    }

}