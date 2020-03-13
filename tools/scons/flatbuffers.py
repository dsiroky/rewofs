# -*- coding: utf-8 -*-
"""
Flatbuffer builders.
"""

import re

import SCons.Action
import SCons.Builder
import SCons.Scanner

#===========================================================================

include_re = re.compile(r'^include\s+"(\S+)"\s*;', re.M)

def _fbs_scan(node, env, path):
    contents = node.get_text_contents()
    return include_re.findall(contents)

FbsScanner = SCons.Scanner.Scanner(
                    name="FbsScanner",
                    function=_fbs_scan,
                    skeys=['$FLATC_SUFFIX'],
                    path_function=SCons.Scanner.FindPathDirs('FLATC_INCPATH'))

#==========================================================================

def generate(env):
    env.SetDefault(
            FLATC=env.WhereIs("flatc"),
            FLATC_FLAGS=[],
            FLATC_INCPATH=[],
            FLATC_SUFFIX=".fbs",
            FLATC_OUTSUFFIX="_generated.h",
            FLATC_OUTDIR="./${TARGET.dir}",
            FLATC_COM="$FLATC $FLATC_FLAGS $_FLATC_INCFLAGS -o $FLATC_OUTDIR ${SOURCE}",
            _FLATC_INCFLAGS="$( ${_concat('-I ', FLATC_INCPATH, '', __env__, RDirs, TARGET, SOURCE)} $)",
        )

    env["BUILDERS"]["Flatc"] = SCons.Builder.Builder(
            action=SCons.Action.Action("$FLATC_COM", "$FLATCCOMSTR"),
            prefix="$FLATC_OUTDIR/",
            suffix="$FLATC_OUTSUFFIX",
            src_suffix="$FLATC_SUFFIX",
            source_scanner=FbsScanner,
            single_source=True,
        )

#--------------------------------------------------------------------------

def exists(env):
    return True
