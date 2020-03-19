# -*- coding: utf-8 -*-
"""
Test server run.
"""

import os
import unittest
import threading
import tempfile
import time

from subprocess import Popen

import nose2.tools as nt

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
        self.client = Popen([REWOFS_PROG, "--mountpoint", self.mount_dir,
                                            "--connect", "ipc:///tmp/rewofs.ipc"])
        # let it settle
        time.sleep(0.5)

    def tearDown(self):
        self.client.kill()
        self.server.kill()
        os.system("fusermount -u " + self.mount_dir)

#===========================================================================

class TestBrowsing(TestClientServer):
    def test_directories_and_files_presence(self):
        os.makedirs(self.source_dir + "/a/b/c")
        os.makedirs(self.source_dir + "/a/bb")
        os.makedirs(self.source_dir + "/x/y")
        with open(self.source_dir + "/data", "w"):
            pass
        with open(self.source_dir + "/a/b/file", "w"):
            pass
        with open(self.source_dir + "/x/bin.box", "w"):
            pass
        os.symlink(self.source_dir + "/hurdygurdy", self.source_dir + "/a/bb/link")

        self.assertTrue(os.path.isdir(self.mount_dir + "/a"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/b"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/b/c"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/a/bb"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/x"))
        self.assertTrue(os.path.isdir(self.mount_dir + "/x/y"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/data"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/a/b/file"))
        self.assertTrue(os.path.isfile(self.mount_dir + "/x/bin.box"))
        self.assertTrue(os.path.islink(self.mount_dir + "/a/bb/link"))

        self.assertFalse(os.path.exists(self.mount_dir + "/u"))
        self.assertFalse(os.path.exists(self.mount_dir + "/u/sdf"))

    def test_read_symlinks(self):
        os.symlink("a", self.source_dir + "/lnk")
        os.symlink("a/b/c", self.source_dir + "/lnk2")
        os.symlink("/someroot/a/b/c", self.source_dir + "/lnk3")
        # internal limit is 1024 characters
        os.symlink("abcd" * 256, self.source_dir + "/lnk4")
        os.symlink("abcd" * 256 + "efgh", self.source_dir + "/lnk5")
        with open(self.source_dir + "/notlink", "w"):
            pass

        self.assertEqual(os.readlink(self.mount_dir + "/lnk"), "a")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk2"), "a/b/c")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk3"), "/someroot/a/b/c")
        self.assertEqual(os.readlink(self.mount_dir + "/lnk4"), "abcd" * 256)
        # internal limit is 1024 characters
        self.assertEqual(os.readlink(self.mount_dir + "/lnk5"), "abcd" * 256)

        with self.assertRaises(Exception):
            os.readlink(self.mount_dir + "/notlink")

    def test_mkdir(self):
        os.mkdir(self.mount_dir + "/x")
        os.makedirs(self.mount_dir + "/a/b/c")

        self.assertTrue(os.path.isdir(self.source_dir + "/x"));
        self.assertTrue(os.path.isdir(self.source_dir + "/a/b/c"));

    def test_mkdir_on_existing(self):
        os.mkdir(self.mount_dir + "/x")
        with open(self.source_dir + "/somefile", "w"):
            pass

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

        with self.assertRaises(OSError):
            os.rmdir(self.mount_dir + "/a/b/c")
        with self.assertRaises(NotADirectoryError):
            os.rmdir(self.mount_dir + "/a/b/c/x")
        with self.assertRaises(NotADirectoryError):
            os.rmdir(self.mount_dir + "/y")

        os.unlink(self.source_dir + "/a/b/c/x")
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

        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/a/b/c")
        with self.assertRaises(IsADirectoryError):
            os.unlink(self.mount_dir + "/d")

        os.unlink(self.mount_dir + "/a/b/c/x")
        os.unlink(self.mount_dir + "/y")

        self.assertFalse(os.path.isfile(self.mount_dir + "/a/b/c/x"))
        self.assertFalse(os.path.isfile(self.mount_dir + "/y"))

    def test_create_symlink(self):
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

        with self.assertRaises(OSError):
            os.rename(self.mount_dir + "/d", self.mount_dir + "/d2")
        with self.assertRaises(OSError):
            os.rename(self.mount_dir + "/x", self.mount_dir + "/a/b/c")

        os.rename(self.mount_dir + "/d", self.mount_dir + "/d3")
        os.rename(self.mount_dir + "/x", self.mount_dir + "/x3")
        os.rename(self.mount_dir + "/s1", self.mount_dir + "/s2/s")

        self.assertFalse(os.path.isdir(self.source_dir + "/d"))
        self.assertTrue(os.path.isdir(self.source_dir + "/d3"))
        self.assertFalse(os.path.isfile(self.source_dir + "/x"))
        self.assertTrue(os.path.isfile(self.source_dir + "/x3"))
        self.assertFalse(os.path.isdir(self.source_dir + "/s1"))
        self.assertTrue(os.path.isdir(self.source_dir + "/s2/s"))

    def test_chmod(self):
        with open(self.source_dir + "/x", "w"):
            pass

        os.chmod(self.mount_dir + "/x", 0o777)
        self.assertEqual(os.stat(self.source_dir + "/x").st_mode & 0o777, 0o777)
        self.assertEqual(os.stat(self.mount_dir + "/x").st_mode & 0o777, 0o777)

        os.chmod(self.mount_dir + "/x", 0o741)
        self.assertEqual(os.stat(self.source_dir + "/x").st_mode & 0o777, 0o741)
        self.assertEqual(os.stat(self.mount_dir + "/x").st_mode & 0o777, 0o741)
