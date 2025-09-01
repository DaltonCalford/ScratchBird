#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

TEST(LockConflict, DoubleOpenSameFile) {
	std::string path = "/tmp/sb_lock_conflict.db";
	::unlink(path.c_str());
	int fd1 = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
	ASSERT_GE(fd1, 0);
	// Opening second handle should succeed at OS level; in future we will enforce lock files
	int fd2 = ::open(path.c_str(), O_RDWR);
	ASSERT_GE(fd2, 0);
	::close(fd2);
	::close(fd1);
	::unlink(path.c_str());
}

