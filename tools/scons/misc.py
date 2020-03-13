# -*- coding: utf-8 -*-
"""
Default stuff.
"""

import os
import sys

import SCons

#==========================================================================

def find_temp_dir(env):
    if os.name == "nt":
        tmp = os.environ.get("TMP")
        if tmp is not None:
            return tmp
        else:
            for d in ("c:\\tmp",):
                if os.path.isdir(d):
                    return d
            else:
                return "c:\\temp"
    else:
        return os.environ.get("TMP") or "/tmp"

#--------------------------------------------------------------------------

def setup_linker(env, builder_name):
    def builder(env, target, sources, **args):
        orig_builder = env["BUILDERS"][builder_name + "original"]
        target = orig_builder(env, target, sources, **args)
        # for incremental linking
        env.Precious(target)
        return target

    env["BUILDERS"][builder_name + "original"] = env["BUILDERS"][builder_name]
    env["BUILDERS"][builder_name] = builder

#--------------------------------------------------------------------------

def pretify_output(env, verbose, commands):
    try:
        # MSW terminal coloring
        import colorama
        colorama.init()
    except ImportError:
        pass

    # pretty print
    if not verbose:
      for k in commands:
          env[k + "COMSTR"] = "%s $TARGET" % k

    # coloring
    if sys.stdout.isatty():
        for k in commands:
            envk = k + "COMSTR"
            if envk in env:
                env[envk] = "\033[36m%s\033[0m" % env[envk]

#==========================================================================

def generate(env):
    env.AddMethod(pretify_output, "PretifyOutput")
    env.AddMethod(find_temp_dir, "FindTempDir")
    env.AddMethod(setup_linker, "SetupLinker")

def exists(env):
    return True
