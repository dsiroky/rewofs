# -*- coding: utf-8 -*-
"""
Test server run.
"""

import os
import posix
import shutil
import tempfile
import threading
import time
import unittest

from subprocess import Popen

#===========================================================================

REWOFS_PROG = os.environ["REWOFS_PROG"]

#===========================================================================

def write_file(filename, content):
    with open(filename, "wb") as fw:
        fw.write(content)

def read_file(filename):
    with open(filename, "rb") as fr:
        return fr.read()

#===========================================================================

class TestClientServer(unittest.TestCase):
    def setUp(self):
        self.work_dir = tempfile.TemporaryDirectory(prefix="rewofs_")
        self.source_dir = os.path.join(self.work_dir.name, "source")
        self.mount_dir = os.path.join(self.work_dir.name, "mount")

        os.mkdir(self.source_dir)
        os.mkdir(self.mount_dir)

        self.server = Popen([REWOFS_PROG, "--serve", self.source_dir,
                                            "--listen", "ipc:///tmp/rewofs.ipc"])
        self.client = None

    def run_client(self):
        self.client = Popen([REWOFS_PROG, "--mountpoint", self.mount_dir,
                                            "--connect", "ipc:///tmp/rewofs.ipc"])
        # let it settle
        time.sleep(0.5)

    def tearDown(self):
        if self.client is not None:
            self.client.kill()
        self.server.kill()
        os.system("fusermount -u " + self.mount_dir)

#===========================================================================

class TestBrowsing(TestClientServer):
    def test_directories_and_files_presence(self):
        os.makedirs(self.source_dir + "/a/b/c")
        os.makedirs(self.source_dir + "/a/bb")
        os.makedirs(self.source_dir + "/x/y")
        write_file(self.source_dir + "/data", b"abc")
        write_file(self.source_dir + "/a/b/file", b"")
        write_file(self.source_dir + "/x/bin.box", b"")
        os.symlink("hurdygurdy", self.source_dir + "/a/bb/link")

        self.run_client()

        self.assertTrue(os.path.isdir(self.mount_dir + "/a"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/b"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/b/c"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/bb"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/x"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/x/y"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/data"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/data"), 3)
        self.assertTrue(os.path.isfile(self.mount_dir + "/a/b/file"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/x/bin.box"))
        self.assertTrue(os.path.islink(self.mount_dir + "/a/bb/link"))

        self.assertFalse(os.path.exists(self.mount_dir + "/u"))
        self.assertFalse(os.path.exists(self.mount_dir + "/u/sdf"))
        self.assertFalse(os.path.exists(self.mount_dir + "/hurdygurdy"))

        self.assertEqual(os.lstat(self.source_dir + "/a").st_mtime,
                         os.lstat(self.mount_dir + "/a").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_mtime,
                         os.lstat(self.mount_dir + "/a/b").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b/c").st_mtime,
                         os.lstat(self.mount_dir + "/a/b/c").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/a").st_ctime,
                         os.lstat(self.mount_dir + "/a").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_ctime,
                         os.lstat(self.mount_dir + "/a/b").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b/c").st_ctime,
                         os.lstat(self.mount_dir + "/a/b/c").st_ctime)

    def test_read_symlinks(self):
        os.symlink("a", self.source_dir + "/lnk")
        os.symlink("a/b/c", self.source_dir + "/lnk2")
        os.symlink("/someroot/a/b/c", self.source_dir + "/lnk3")
        os.symlink("abcd" * 50 + "efgh", self.source_dir + "/lnk4")
        write_file(self.source_dir + "/notlink", b"")

        self.run_client()

        self.assertEqual(os.readlink(self.mount_dir + "/lnk"), "a")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk2"), "a/b/c")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk3"), "/someroot/a/b/c")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk4"), "abcd" * 50 + "efgh")

        with self.assertRaises(Exception):
            os.readlink(self.mount_dir + "/notlink")

    def test_mkdir(self):
        self.run_client()

        os.mkdir(self.mount_dir + "/x")
        os.makedirs(self.mount_dir + "/a/b")
        time.sleep(0.2) # affect /a/b mtime
        os.makedirs(self.mount_dir + "/a/b/c")

        self.assertTrue(os.path.isdir(self.source_dir + "/x"))
        self.assertTrue(os.path.isdir(self.source_dir + "/a/b/c"))

        self.assertEqual(os.lstat(self.source_dir + "/x").st_mtime,
                         os.lstat(self.mount_dir + "/x").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_mtime,
                         os.lstat(self.mount_dir + "/a/b").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b/c").st_mtime,
                         os.lstat(self.mount_dir + "/a/b/c").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/x").st_ctime,
                         os.lstat(self.mount_dir + "/x").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_ctime,
                         os.lstat(self.mount_dir + "/a/b").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b/c").st_ctime,
                         os.lstat(self.mount_dir + "/a/b/c").st_ctime)

    def test_mkdir_on_existing(self):
        with open(self.source_dir + "/somefile", "w"):
            pass

        self.run_client()

        os.mkdir(self.mount_dir + "/x")
        with self.assertRaises(FileExistsError):
            os.mkdir(self.mount_dir + "/x")
        with self.assertRaises(FileExistsError):
            os.mkdir(self.mount_dir + "/somefile")

    def test_rmdir(self):
        os.makedirs(self.source_dir + "/a/b/c")
        os.makedirs(self.source_dir + "/d")
        write_file(self.source_dir + "/a/b/c/x", b"")
        write_file(self.source_dir + "/y", b"")

        self.run_client()

        with self.assertRaises(OSError):
            os.rmdir(self.mount_dir + "/a/b/c")
        with self.assertRaises(NotADirectoryError):
            os.rmdir(self.mount_dir + "/a/b/c/x")
        with self.assertRaises(NotADirectoryError):
            os.rmdir(self.mount_dir + "/y")

        os.unlink(self.mount_dir + "/a/b/c/x")
        os.rmdir(self.mount_dir + "/a/b/c")
        self.assertFalse(os.path.isdir(self.mount_dir + "/a/b/c"))
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_mtime,
                         os.lstat(self.mount_dir + "/a/b").st_mtime)
        os.rmdir(self.mount_dir + "/a/b")
        self.assertFalse(os.path.isdir(self.mount_dir + "/a/b"))
        os.rmdir(self.mount_dir + "/a")
        self.assertFalse(os.path.isdir(self.mount_dir + "/a"))
        os.rmdir(self.mount_dir + "/d")
        self.assertFalse(os.path.isdir(self.mount_dir + "/d"))


    def test_unlink(self):
        os.makedirs(self.source_dir + "/a/b/c")
        os.makedirs(self.source_dir + "/d")
        write_file(self.source_dir + "/a/b/c/x", b"")
        write_file(self.source_dir + "/y", b"")

        self.run_client()

        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/a/b/c")
        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/d")

        os.unlink(self.mount_dir + "/a/b/c/x")
        os.unlink(self.mount_dir + "/y")

        self.assertFalse(os.path.isfile(self.mount_dir + "/a/b/c/x"))
        self.assertFalse(os.path.isfile(self.mount_dir + "/y"))

        self.assertEqual(os.lstat(self.source_dir + "/a/b/c").st_mtime,
                         os.lstat(self.mount_dir + "/a/b/c").st_mtime)

    def test_create_symlink(self):
        self.run_client()

        os.symlink("abc", self.mount_dir + "/link")
        os.mkdir(self.mount_dir + "/d")
        os.symlink("sdgf/wer", self.mount_dir + "/d/link")

        self.assertTrue(os.path.islink(self.source_dir + "/link"))
        self.assertEqual(os.readlink(self.source_dir + "/link"), "abc")
        self.assertTrue(os.path.islink(self.source_dir + "/d/link"))
        self.assertEqual(os.readlink(self.source_dir + "/d/link"), "sdgf/wer")

        self.assertEqual(os.lstat(self.source_dir + "/d").st_mtime,
                         os.lstat(self.mount_dir + "/d").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/d/link").st_mtime,
                         os.lstat(self.mount_dir + "/d/link").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/d/link").st_ctime,
                         os.lstat(self.mount_dir + "/d/link").st_ctime)

    def test_rename(self):
        os.makedirs(self.source_dir + "/d")
        os.makedirs(self.source_dir + "/d2")
        os.makedirs(self.source_dir + "/s1")
        os.makedirs(self.source_dir + "/s2")
        write_file(self.source_dir + "/d2/fle", b"")
        write_file(self.source_dir + "/x", b"")
        write_file(self.source_dir + "/x2", b"")
        os.makedirs(self.source_dir + "/y/suby")

        self.run_client()

        with self.assertRaises(OSError):
            os.rename(self.mount_dir + "/d", self.mount_dir + "/d2")
        with self.assertRaises(OSError):
            os.rename(self.mount_dir + "/x", self.mount_dir + "/a/b/c")

        os.rename(self.mount_dir + "/d", self.mount_dir + "/d3")
        os.rename(self.mount_dir + "/x", self.mount_dir + "/x3")
        os.rename(self.mount_dir + "/s1", self.mount_dir + "/s2/s")
        os.rename(self.mount_dir + "/y", self.mount_dir + "/s2/s/yy")

        self.assertFalse(os.path.isdir(self.source_dir + "/d"))
        self.assertTrue(os.path.isdir(self.source_dir + "/d3"))
        self.assertFalse(os.path.isfile(self.source_dir + "/x"))
        self.assertTrue(os.path.isfile(self.source_dir + "/x3"))
        self.assertFalse(os.path.isdir(self.source_dir + "/s1"))
        self.assertTrue(os.path.isdir(self.source_dir + "/s2/s"))
        self.assertTrue(os.path.isdir(self.source_dir + "/s2/s/yy/suby"))

        self.assertFalse(os.path.isdir(self.mount_dir + "/d"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/d3"))
        self.assertFalse(os.path.isfile(self.mount_dir + "/x"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/x3"))
        self.assertFalse(os.path.isdir(self.mount_dir + "/s1"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/s2/s"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/s2/s/yy/suby"))

        self.assertEqual(os.lstat(self.source_dir + "/d3").st_mtime,
                         os.lstat(self.mount_dir + "/d3").st_mtime)

    def test_chmod(self):
        with open(self.source_dir + "/x", "w"):
            pass

        self.run_client()

        os.chmod(self.mount_dir + "/x", 0o777)
        self.assertEqual(os.lstat(self.source_dir + "/x").st_mode & 0o777, 0o777)
        self.assertEqual(os.lstat(self.mount_dir + "/x").st_mode & 0o777, 0o777)

        os.chmod(self.mount_dir + "/x", 0o741)
        self.assertEqual(os.lstat(self.source_dir + "/x").st_mode & 0o777, 0o741)
        self.assertEqual(os.lstat(self.mount_dir + "/x").st_mode & 0o777, 0o741)

#===========================================================================

class TestIO(TestClientServer):
    @staticmethod
    def random_data_for_read():
        # add a certain pattern for easier debugging
        IO_FRAGMENT_SIZE = 32 * 1024
        data = []
        for i in range(10000000 // IO_FRAGMENT_SIZE):
            mark = bytes([i % 256]) * 16
            data.append(mark + os.urandom(IO_FRAGMENT_SIZE - len(mark)))
        return b''.join(data)

    def test_create_open_close(self):
        write_file(self.source_dir + "/existing", b"")

        self.run_client()

        with self.assertRaises(FileNotFoundError):
            posix.open(self.mount_dir + "/x", posix.O_RDONLY)

        f = posix.open(self.mount_dir + "/x", posix.O_WRONLY | posix.O_CREAT)
        posix.close(f)
        with self.assertRaises(OSError):
            posix.close(f)

        f = posix.open(self.mount_dir + "/x", posix.O_RDONLY)
        posix.close(f)

        self.assertTrue(os.path.isfile(self.mount_dir + "/x"))
        self.assertTrue(os.path.isfile(self.source_dir + "/x"))

        self.assertEqual(os.lstat(self.source_dir + "/x").st_ctime,
                         os.lstat(self.mount_dir + "/x").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/x").st_mtime,
                         os.lstat(self.mount_dir + "/x").st_mtime)

    def test_read_open_separate(self):
        data1 = self.random_data_for_read()
        write_file(self.source_dir + "/f1", data1)
        data2 = self.random_data_for_read()
        write_file(self.source_dir + "/f2", data2)

        self.run_client()

        self.assertEqual(data1, read_file(self.mount_dir + "/f1"))
        self.assertEqual(data2, read_file(self.mount_dir + "/f2"))

    def test_read_open_together(self):
        data1 = self.random_data_for_read()
        write_file(self.source_dir + "/f1", data1)
        data2 = self.random_data_for_read()
        write_file(self.source_dir + "/f2", data2)

        self.run_client()

        with open(self.mount_dir + "/f1", "rb") as fr1:
            with open(self.mount_dir + "/f2", "rb") as fr2:
                self.assertEqual(data1, fr1.read())
                self.assertEqual(data2, fr2.read())

    def test_random_read(self):
        data = self.random_data_for_read()
        write_file(self.source_dir + "/f", data)

        self.run_client()

        with open(self.mount_dir + "/f", "rb") as fr:
            fr.seek(50000)
            self.assertEqual(fr.read(2000), data[50000:52000])
            fr.seek(500000)
            self.assertEqual(fr.read(2000), data[500000:502000])
            fr.seek(5000000)
            self.assertEqual(fr.read(1000000), data[5000000:6000000])

    def test_write_open_separate(self):
        self.run_client()

        data1 = os.urandom(10000000)
        write_file(self.mount_dir + "/f1", data1)
        data2 = os.urandom(10000000)
        write_file(self.mount_dir + "/f2", data2)

        self.assertTrue(os.path.isfile(self.mount_dir + "/f1"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f1"), 10000000)
        self.assertTrue(os.path.isfile(self.mount_dir + "/f2"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f2"), 10000000)

        self.assertEqual(data1, read_file(self.source_dir + "/f1"))
        self.assertEqual(data2, read_file(self.source_dir + "/f2"))

        self.assertEqual(os.lstat(self.source_dir + "/f1").st_size,
                         os.lstat(self.mount_dir + "/f1").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f1").st_ctime,
                         os.lstat(self.mount_dir + "/f1").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/f1").st_mtime,
                         os.lstat(self.mount_dir + "/f1").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/f2").st_size,
                         os.lstat(self.mount_dir + "/f2").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f2").st_ctime,
                         os.lstat(self.mount_dir + "/f2").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/f2").st_mtime,
                         os.lstat(self.mount_dir + "/f2").st_mtime)

    def test_write_open_together(self):
        self.run_client()

        data1 = os.urandom(10000000)
        data2 = os.urandom(10000000)
        with open(self.mount_dir + "/f1", "wb") as fw1:
            with open(self.mount_dir + "/f2", "wb") as fw2:
                fw1.write(data1)
                fw2.write(data2)

        self.assertTrue(os.path.isfile(self.mount_dir + "/f1"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f1"), 10000000)
        self.assertTrue(os.path.isfile(self.mount_dir + "/f2"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f2"), 10000000)

        self.assertEqual(data1, read_file(self.source_dir + "/f1"))
        self.assertEqual(data2, read_file(self.source_dir + "/f2"))

        self.assertEqual(os.lstat(self.source_dir + "/f1").st_size,
                         os.lstat(self.mount_dir + "/f1").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f1").st_ctime,
                         os.lstat(self.mount_dir + "/f1").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/f1").st_mtime,
                         os.lstat(self.mount_dir + "/f1").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/f2").st_size,
                         os.lstat(self.mount_dir + "/f2").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f2").st_ctime,
                         os.lstat(self.mount_dir + "/f2").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/f2").st_mtime,
                         os.lstat(self.mount_dir + "/f2").st_mtime)

    def test_sparse_write(self):
        self.run_client()

        with open(self.mount_dir + "/f", "wb") as fw:
            fw.seek(1000)
            fw.write(b"abc")
            fw.seek(10000000)
            fw.write(b"123")

        data = (b'\0' * 1000) + b"abc" + (b'\0' * (10000000 - 1000 - 3)) + b"123"
        self.assertEqual(data, read_file(self.source_dir + "/f"))

        self.assertEqual(os.lstat(self.source_dir + "/f").st_size,
                         os.lstat(self.mount_dir + "/f").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f").st_mtime,
                         os.lstat(self.mount_dir + "/f").st_mtime)

    def test_mixed_read_write(self):
        self.run_client()

        with open(self.mount_dir + "/f", "w+b") as frw:
            frw.seek(1000)
            frw.write(b"abc")
            frw.seek(950)
            self.assertEqual(frw.read(100), b'\0' * 50 + b"abc")
            frw.seek(2000)
            frw.write(b"efg")

        data = (b'\0' * 1000) + b"abc" + (b'\0' * (1000 - 3)) + b"efg"
        self.assertEqual(data, read_file(self.source_dir + "/f"))

        self.assertEqual(os.lstat(self.source_dir + "/f").st_size,
                         os.lstat(self.mount_dir + "/f").st_size)
        self.assertEqual(os.lstat(self.source_dir + "/f").st_mtime,
                         os.lstat(self.mount_dir + "/f").st_mtime)

    def test_truncate(self):
        with open(self.source_dir + "/f", "wb") as fw:
            fw.write(b"a" * 1024)

        self.run_client()

        posix.truncate(self.mount_dir + "/f", 100)

        self.assertEqual(os.path.getsize(self.source_dir + "/f"), 100)
        self.assertEqual(os.path.getsize(self.mount_dir + "/f"), 100)

        self.assertEqual(os.lstat(self.source_dir + "/f").st_mtime,
                         os.lstat(self.mount_dir + "/f").st_mtime)

    def test_parallel_writes(self):
        self.run_client()

        error_in_thread = [None]
        def __run(idx):
            fn = self.mount_dir + "/f" + str(idx)
            try:
                for i in range(1000):
                    with open(fn, "wb") as fw:
                        fw.write(b"a" * 1024)
            except Exception as exc:
                error_in_thread[0] = exc

        threads = []
        N = 10
        for i in range(N):
            threads.append(threading.Thread(target=__run, args=(i,)))
            threads[-1].start()

        for thr in threads:
            thr.join()

        self.assertIsNone(error_in_thread[0])

        for i in range(N):
            sidx = str(i)
            self.assertEqual(os.lstat(self.source_dir + "/f" + sidx).st_size,
                             os.lstat(self.mount_dir + "/f" + sidx).st_size)
            self.assertEqual(os.lstat(self.source_dir + "/f" + sidx).st_mtime,
                             os.lstat(self.mount_dir + "/f" + sidx).st_mtime)

    def test_copy_file(self):
        with open(self.source_dir + "/f", "wb") as fw:
            fw.write(b"a" * 1024)

        self.run_client()

        shutil.copyfile(self.mount_dir + "/f", self.mount_dir + "/fcopy")

        self.assertEqual(read_file(self.source_dir + "/fcopy"), b"a" * 1024)
        self.assertEqual(read_file(self.mount_dir + "/fcopy"), b"a" * 1024)

        self.assertEqual(os.lstat(self.source_dir + "/fcopy").st_ctime,
                         os.lstat(self.mount_dir + "/fcopy").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/fcopy").st_mtime,
                         os.lstat(self.mount_dir + "/fcopy").st_mtime)

#===========================================================================

class TestRemoteInvalidations(TestClientServer):
    """
    Tree or data modified on the server.
    """

    def test_mkdir(self):
        self.run_client()

        os.mkdir(self.source_dir + "/x")
        os.makedirs(self.source_dir + "/a/b")

        # let it propagate
        time.sleep(1)

        self.assertTrue(os.path.isdir(self.mount_dir + "/x"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/b"))

        self.assertEqual(os.lstat(self.source_dir + "/x").st_mtime,
                         os.lstat(self.mount_dir + "/x").st_mtime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_mtime,
                         os.lstat(self.mount_dir + "/a/b").st_mtime)

        self.assertEqual(os.lstat(self.source_dir + "/x").st_ctime,
                         os.lstat(self.mount_dir + "/x").st_ctime)
        self.assertEqual(os.lstat(self.source_dir + "/a/b").st_ctime,
                         os.lstat(self.mount_dir + "/a/b").st_ctime)

    def test_rmdir(self):
        os.mkdir(self.source_dir + "/x")
        os.makedirs(self.source_dir + "/a/b")

        self.run_client()

        os.rmdir(self.source_dir + "/x")
        os.rmdir(self.source_dir + "/a/b")

        # let it propagate
        time.sleep(1)

        self.assertFalse(os.path.exists(self.mount_dir + "/x"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a"))
        self.assertFalse(os.path.exists(self.mount_dir + "/a/b"))

        self.assertEqual(os.lstat(self.source_dir + "/a").st_mtime,
                         os.lstat(self.mount_dir + "/a").st_mtime)

    def test_unlink(self):
        with open(self.source_dir + "/f", "wb") as fw:
            fw.write(b"x")
        os.mkdir(self.source_dir + "/x")
        with open(self.source_dir + "/x/f2", "wb") as fw:
            fw.write(b"y")
        os.symlink("hurdygurdy", self.source_dir + "/link")

        self.run_client()

        os.unlink(self.source_dir + "/f")
        os.unlink(self.source_dir + "/x/f2")
        os.unlink(self.source_dir + "/link")

        # let it propagate
        time.sleep(1)

        self.assertFalse(os.path.exists(self.mount_dir + "/f"))
        self.assertFalse(os.path.exists(self.mount_dir + "/x/f2"))
        self.assertFalse(os.path.exists(self.mount_dir + "/link"))

        self.assertEqual(os.lstat(self.source_dir + "/x").st_mtime,
                         os.lstat(self.mount_dir + "/x").st_mtime)

    def test_create_symlink(self):
        self.run_client()

        os.symlink("hurdygurdy", self.source_dir + "/link")

        # let it propagate
        time.sleep(1)

        self.assertTrue(os.path.islink(self.mount_dir + "/link"))

        self.assertEqual(os.lstat(self.source_dir + "/link").st_ctime,
                         os.lstat(self.mount_dir + "/link").st_ctime)

    def test_create_file(self):
        self.run_client()

        with open(self.source_dir + "/f", "wb") as fw:
            fw.write("g")

        # let it propagate
        time.sleep(1)

        self.assertTrue(os.path.islink(self.mount_dir + "/link"))

        self.assertEqual(os.lstat(self.source_dir + "/link").st_ctime,
                         os.lstat(self.mount_dir + "/link").st_ctime)
