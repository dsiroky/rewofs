# -*- coding: utf-8 -*-
"""
Test server run.
"""

import os
import posix
import tempfile
import threading
import time
import unittest

from subprocess import Popen

#===========================================================================

REWOFS_PROG = os.environ["REWOFS_PROG"]

#===========================================================================

class KillAfterTimeout(object):
    def __init__(self, process, timeout):
        """
        @param timeout [s]
        """
        self.process = process
        self.timer = threading.Timer(timeout, self.kill_him)
        self.timer.start()

    def __enter__(self):
        pass

    def __exit__(self, tp, value, traceback):
        self.cancel()

    def cancel(self):
        self.timer.cancel()

    def kill_him(self):
        try:
            self.process.terminate()
        except:
            pass

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
        with open(self.source_dir + "/data", "w") as fw:
            fw.write("abc")
        with open(self.source_dir + "/a/b/file", "w"):
            pass
        with open(self.source_dir + "/x/bin.box", "w"):
            pass
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

    def test_read_symlinks(self):
        os.symlink("a", self.source_dir + "/lnk")
        os.symlink("a/b/c", self.source_dir + "/lnk2")
        os.symlink("/someroot/a/b/c", self.source_dir + "/lnk3")
        os.symlink("abcd" * 50 + "efgh", self.source_dir + "/lnk4")
        with open(self.source_dir + "/notlink", "w"):
            pass

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
        os.makedirs(self.mount_dir + "/a/b/c")

        self.assertTrue(os.path.isdir(self.source_dir + "/x"));
        self.assertTrue(os.path.isdir(self.source_dir + "/a/b/c"));

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
        with open(self.source_dir + "/a/b/c/x", "w"):
            pass
        with open(self.source_dir + "/y", "w"):
            pass

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
        os.rmdir(self.mount_dir + "/a/b")
        self.assertFalse(os.path.isdir(self.mount_dir + "/a/b"))
        os.rmdir(self.mount_dir + "/a")
        self.assertFalse(os.path.isdir(self.mount_dir + "/a"))
        os.rmdir(self.mount_dir + "/d")
        self.assertFalse(os.path.isdir(self.mount_dir + "/d"))

    def test_unlink(self):
        os.makedirs(self.source_dir + "/a/b/c")
        os.makedirs(self.source_dir + "/d")
        with open(self.source_dir + "/a/b/c/x", "w"):
            pass
        with open(self.source_dir + "/y", "w"):
            pass

        self.run_client()

        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/a/b/c")
        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/d")

        os.unlink(self.mount_dir + "/a/b/c/x")
        os.unlink(self.mount_dir + "/y")

        self.assertFalse(os.path.isfile(self.mount_dir + "/a/b/c/x"))
        self.assertFalse(os.path.isfile(self.mount_dir + "/y"))

    def test_create_symlink(self):
        self.run_client()

        os.symlink("abc", self.mount_dir + "/link")
        os.mkdir(self.mount_dir + "/d")
        os.symlink("sdgf/wer", self.mount_dir + "/d/link")

        self.assertTrue(os.path.islink(self.source_dir + "/link"))
        self.assertEqual(os.readlink(self.source_dir + "/link"), "abc")
        self.assertTrue(os.path.islink(self.source_dir + "/d/link"))
        self.assertEqual(os.readlink(self.source_dir + "/d/link"), "sdgf/wer")

    def test_rename(self):
        os.makedirs(self.source_dir + "/d")
        os.makedirs(self.source_dir + "/d2")
        os.makedirs(self.source_dir + "/s1")
        os.makedirs(self.source_dir + "/s2")
        with open(self.source_dir + "/d2/fle", "w"):
            pass
        with open(self.source_dir + "/x", "w"):
            pass
        with open(self.source_dir + "/x2", "w"):
            pass
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

    def test_chmod(self):
        with open(self.source_dir + "/x", "w"):
            pass

        self.run_client()

        os.chmod(self.mount_dir + "/x", 0o777)
        self.assertEqual(os.stat(self.source_dir + "/x").st_mode & 0o777, 0o777)
        self.assertEqual(os.stat(self.mount_dir + "/x").st_mode & 0o777, 0o777)

        os.chmod(self.mount_dir + "/x", 0o741)
        self.assertEqual(os.stat(self.source_dir + "/x").st_mode & 0o777, 0o741)
        self.assertEqual(os.stat(self.mount_dir + "/x").st_mode & 0o777, 0o741)

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
        with open(self.source_dir + "/existing", "w"):
            pass

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

    def test_read_open_separate(self):
        data1 = self.random_data_for_read()
        with open(self.source_dir + "/f1", "wb") as fw:
            fw.write(data1)
        data2 = self.random_data_for_read()
        with open(self.source_dir + "/f2", "wb") as fw:
            fw.write(data2)

        self.run_client()

        with open(self.mount_dir + "/f1", "rb") as fr:
            self.assertEqual(data1, fr.read())
        with open(self.mount_dir + "/f2", "rb") as fr:
            self.assertEqual(data2, fr.read())

    def test_read_open_together(self):
        data1 = self.random_data_for_read()
        with open(self.source_dir + "/f1", "wb") as fw:
            fw.write(data1)
        data2 = self.random_data_for_read()
        with open(self.source_dir + "/f2", "wb") as fw:
            fw.write(data2)

        self.run_client()

        with open(self.mount_dir + "/f1", "rb") as fr1:
            with open(self.mount_dir + "/f2", "rb") as fr2:
                self.assertEqual(data1, fr1.read())
                self.assertEqual(data2, fr2.read())

    def test_random_read(self):
        data = self.random_data_for_read()
        with open(self.source_dir + "/f", "wb") as fw:
            fw.write(data)

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
        with open(self.mount_dir + "/f1", "wb") as fw:
            fw.write(data1)
        data2 = os.urandom(10000000)
        with open(self.mount_dir + "/f2", "wb") as fw:
            fw.write(data2)

        self.assertTrue(os.path.isfile(self.mount_dir + "/f1"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f1"), 10000000)
        self.assertTrue(os.path.isfile(self.mount_dir + "/f2"))
        self.assertEqual(os.path.getsize(self.mount_dir + "/f2"), 10000000)

        with open(self.source_dir + "/f1", "rb") as fr:
            self.assertEqual(data1, fr.read())
        with open(self.source_dir + "/f2", "rb") as fr:
            self.assertEqual(data2, fr.read())

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

        with open(self.source_dir + "/f1", "rb") as fr:
                self.assertEqual(data1, fr.read())
        with open(self.source_dir + "/f2", "rb") as fr:
                self.assertEqual(data2, fr.read())

    def test_sparse_write(self):
        self.run_client()

        with open(self.mount_dir + "/f", "wb") as fw:
            fw.seek(1000)
            fw.write(b"abc")
            fw.seek(10000000)
            fw.write(b"123")

        with open(self.source_dir + "/f", "rb") as fr:
                data = (b'\0' * 1000) + b"abc" + (b'\0' * (10000000 - 1000 - 3)) + b"123"
                self.assertEqual(data, fr.read())

    def test_mixed_read_write(self):
        self.run_client()

        with open(self.mount_dir + "/f", "w+b") as frw:
            frw.seek(1000)
            frw.write(b"abc")
            frw.seek(950)
            self.assertEqual(frw.read(100), b'\0' * 50 + b"abc")
            frw.seek(2000)
            frw.write(b"efg")

        with open(self.source_dir + "/f", "rb") as fr:
                data = (b'\0' * 1000) + b"abc" + (b'\0' * (1000 - 3)) + b"efg"
                self.assertEqual(data, fr.read())

    def test_truncate(self):
        with open(self.source_dir + "/f", "wb") as fw:
            fw.write(b"a" * 1024)

        self.run_client()

        posix.truncate(self.mount_dir + "/f", 100)

        self.assertEqual(os.path.getsize(self.mount_dir + "/f"), 100)

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
