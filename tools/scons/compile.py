# -*- coding: utf-8 -*-
"""
Compilation support.
"""

#===========================================================================

def WholeArchive(env, lib):
    """
    This is a neccessary wrap around static libraries if their symbols are to
    be included in the linked executable.
    """
    # hack from https://github.com/scons/scons/wiki/SharedLibraryWholeArchive
    # "#" must be prepended to avoid prepending a relative directory
    whole_archive = env.Command("#-Wl,--whole-archive", [], "")
    no_whole_archive = env.Command("#-Wl,--no-whole-archive", [], "")
    return whole_archive + lib + no_whole_archive

#--------------------------------------------------------------------------

def generate(env):
    env.AddMethod(WholeArchive, "WholeArchive")

#--------------------------------------------------------------------------

def exists(env):
    return True
